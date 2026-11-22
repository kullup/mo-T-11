#include "Synth.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace mot;

void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct Block
{
    std::vector<float> left, right;
    Block(Synth& synth, int samples, std::initializer_list<Event> events = {})
        : left(samples), right(samples)
    {
        synth.render(left.data(), right.data(), samples, events.begin(), events.size());
    }
};

float peak(const std::vector<float>& values)
{
    float max = 0.0f;
    for (float value : values)
    {
        check(std::isfinite(value), "Non-finite output");
        max = std::max(max, std::abs(value));
    }
    return max;
}

float pitch(const std::vector<float>& values, int begin, int end)
{
    float first = 0.0f, last = 0.0f;
    int crossings = 0;
    for (int i = begin + 1; i < end; ++i)
        if (values[i - 1] < 0.0f && values[i] >= 0.0f)
        {
            last = i - values[i] / (values[i] - values[i - 1]);
            if (crossings++ == 0)
                first = last;
        }
    check(crossings >= 2, "Not enough cycles to measure pitch");
    return (crossings - 1) * 48000.0f / (last - first);
}

Patch sinePatch()
{
    Patch p;
    p.osc[0].position = 0.0f;
    p.ampEnv = { 0.06f, 0.02f, 1.0f, 0.02f };
    return p;
}

void notes(const WaveBank& bank)
{
    Synth synth(bank);
    Event atOffset;
    atOffset.offset = 128;
    Block sound(synth, 4096, { atOffset });
    check(peak(sound.left) > 0.04f, "Init patch must produce a usable signal");
    check(std::all_of(sound.left.begin(), sound.left.begin() + atOffset.offset,
                      [](float value) { return value == 0.0f; }), "Note started before its timestamp");
    Block release(synth, 14400, { { EventType::noteOff } });
    check(peak(release.left) > 0.01f, "No release tail");
    check(synth.activeVoices() == 0 && peak(Block(synth, 32).left) == 0.0f, "Release never reached silence");

    Block duplicate(synth, 2048, { { EventType::noteOn, 1 }, { EventType::noteOn, 2 } });
    check(synth.activeVoices() == 2, "Matching notes on two MIDI ports must coexist");
    Block off(synth, 12000, { { EventType::noteOff, 1 } });
    check(synth.activeVoices() == 1 && peak(off.right) > 0.02f, "Note-off affected another MIDI port");
    Block disconnected(synth, 2048, { { EventType::disconnect, 2 } });
    check(synth.activeVoices() == 0, "Disconnect left a voice active");

    Event pedal { EventType::expression, 2, 7 };
    pedal.controls.sustain = true;
    Block held(synth, 16000, { { EventType::noteOn, 2, 7 }, pedal, { EventType::noteOff, 2, 7 } });
    check(synth.activeVoices() == 1, "Sustain pedal did not hold the released note");
    pedal.controls.sustain = false;
    Block released(synth, 12000, { pedal });
    check(synth.activeVoices() == 0, "Pedal-up did not release sustained notes");

    Block omni(synth, 128, { { EventType::noteOn, 3, 1 }, { EventType::noteOn, 3, 16 } });
    Block allOff(synth, 12000, { { EventType::allNotesOff, 3, 1 } });
    check(synth.activeVoices() == 1, "All notes off leaked to another channel");
    Block panic(synth, 2048, { { EventType::panic } });
    check(synth.activeVoices() == 0 && peak(Block(synth, 32).right) == 0.0f, "Panic did not clear the engine");
}

void modulationAndPolyphony(const WaveBank& bank)
{
    Synth synth(bank);
    Patch p;
    p.matrix[velocity][pan] = 1.0f;
    synth.setPatch(p);
    Block positive(synth, 2048, { Event {} });
    check(peak(positive.left) < 0.00001f && peak(positive.right) > 0.05f, "Positive pan route is incorrect");
    p.matrix[velocity][pan] = -1.0f;
    synth.prepare(48000);
    synth.setPatch(p);
    Block negative(synth, 2048, { { EventType::noteOn, 0, 1, 64 } });
    check(peak(negative.left) > 0.05f && peak(negative.right) < 0.00001f, "Negative pan route is incorrect");

    p = factoryPresets()[6].sound;
    p.volume = p.resonance = p.drive = 1.0f;
    synth.prepare(44100);
    synth.setPatch(p);
    std::array<Event, 24> on {};
    for (int i = 0; i < static_cast<int>(on.size()); ++i)
        on[i].note = 42 + i;
    std::array<float, 2048> left {}, right {};
    synth.render(left.data(), right.data(), static_cast<int>(left.size()), on.data(), on.size());
    check(synth.activeVoices() == Synth::polyphony, "Voice stealing did not respect the polyphony limit");
    check(peak({ left.begin(), left.end() }) < 1.0f && peak({ right.begin(), right.end() }) < 1.0f,
          "Polyphonic output exceeded full scale");

    for (int table = 0; table < tableCount; ++table)
    {
        float difference = 0;
        for (int i = 0; i < 64; ++i)
        {
            const float phase = i / 64.0f;
            difference += std::abs(bank.display(table, 0, phase) - bank.display(table, 1, phase));
        }
        check(difference > 0.5f, "A factory wavetable has identical end frames");
        check(std::abs(bank.display(table, 0.5f, 0) - bank.display(table, 0.5f, 1)) < 1.0e-6f,
              "Wavetable is discontinuous at the cycle seam");
        check(bank.sample(table, 0.5f, 0.6f, 24000, 48000) == 0.0f,
              "A fundamental beyond the band limit was rendered");
    }
}

void monoPlaying(const WaveBank& bank)
{
    const Event low { EventType::noteOn, 1, 1, 57 };
    const Event high { EventType::noteOn, 2, 16, 69 };
    const Event releaseHigh { EventType::noteOff, 2, 16, 69 };
    auto p = sinePatch();
    Synth synth(bank);
    p.voicing.mode = PlayMode::mono;
    synth.setPatch(p);
    Block(synth, 4800, { low });
    const Block retriggered(synth, 960, { high });
    check(synth.activeVoices() == 1, "Mono sounded multiple MIDI notes");
    const Block fallback(synth, 4800, { releaseHigh });
    check(std::abs(pitch(fallback.left, 3600, 4800) - 220.0f) < 1.0f, "Mono did not return to the lower held note");

    synth.prepare(48000);
    p.voicing.mode = PlayMode::legato;
    synth.setPatch(p);
    Block(synth, 4800, { low });
    const Block legato(synth, 960, { high });
    check(peak(legato.left) > peak(retriggered.left) * 2.5f, "Legato restarted the amp envelope");
    const Block resumed(synth, 960, { releaseHigh });
    check(peak(resumed.left) > peak(retriggered.left) * 2.5f, "Legato fallback restarted the amp envelope");

    Event pedal { EventType::expression, 1, 1 };
    pedal.controls.sustain = true;
    Block(synth, 4800, { pedal, { EventType::noteOff, 1, 1, 57 } });
    check(synth.activeVoices() == 1, "Sustain did not hold the mono gate");
    pedal.controls.sustain = false;
    Block(synth, 4800, { pedal });
    check(synth.activeVoices() == 0, "Pedal-up left the mono gate active");

    Block(synth, 4800, { low, high, { EventType::disconnect, 2 } });
    check(synth.activeVoices() == 1, "Disconnect did not fall back to another MIDI port");
    Block(synth, 4800, { { EventType::allSoundOff, 1, 1 } });
    check(synth.activeVoices() == 0, "All Sound Off left a mono note active");
    Block(synth, 4800, { low, high, { EventType::panic }, releaseHigh });
    check(synth.activeVoices() == 0, "Panic left a note in the mono stack");

    p.voicing.mode = PlayMode::poly;
    synth.setPatch(p);
    Block(synth, 4800, { low, high });
    p.voicing.mode = PlayMode::legato;
    synth.setPatch(p);
    const Block collapsed(synth, 2400);
    check(synth.activeVoices() == 1 && std::abs(pitch(collapsed.left, 1000, 2400) - 440.0f) < 1.0f,
          "Switching to mono lost the most recently held note");
    p.voicing.mode = PlayMode::poly;
    synth.setPatch(p);
    check(synth.activeVoices() == 2, "Switching back to poly lost a held note");
}

void glideAndUnison(const WaveBank& bank)
{
    const Event low { EventType::noteOn, 1, 1, 57 };
    const Event high { EventType::noteOn, 1, 1, 69 };
    auto p = sinePatch();
    p.ampEnv.attack = 0.002f;
    p.voicing.mode = PlayMode::legato;
    p.voicing.glide = 0.2f;
    Synth synth(bank);
    synth.setPatch(p);
    Block(synth, 4800, { low });
    const Block sliding(synth, 11000, { high });
    check(std::abs(pitch(sliding.left, 0, 960) - 228.0f) < 6.0f, "Glide did not start near the previous pitch");
    check(std::abs(pitch(sliding.left, 4400, 5200) - 311.0f) < 6.0f, "Glide did not interpolate in semitones");
    check(std::abs(pitch(sliding.left, 9700, 11000) - 440.0f) < 1.0f, "Glide missed its target or duration");
    const Block slidingBack(synth, 11000, { { EventType::noteOff, 1, 1, 69 } });
    check(std::abs(pitch(slidingBack.left, 0, 960) - 425.0f) < 8.0f
          && std::abs(pitch(slidingBack.left, 9700, 11000) - 220.0f) < 1.0f,
          "Glide did not return smoothly to the held note");
    const Block released(synth, 2200, { { EventType::allNotesOff, 1, 1 } });
    check(synth.activeVoices() == 0 && peak(released.left) > 0.01f, "All Notes Off did not release the mono note");
    const Block detached(synth, 960, { high });
    check(std::abs(pitch(detached.left, 120, 960) - 440.0f) < 1.0f, "Glide leaked across a gap between notes");

    p.voicing = { PlayMode::poly, 0.0f, 2, 50.0f, 0.0f };
    synth.prepare(48000);
    synth.setPatch(p);
    const Block centred(synth, 4800, { high });
    check(std::equal(centred.left.begin(), centred.left.end(), centred.right.begin(),
                     [](float left, float right) { return std::abs(left - right) < 1.0e-6f; })
          && peak(centred.left) > 0.03f, "Zero-width unison was not centred");
    p.voicing.width = 1.0f;
    synth.setPatch(p);
    const Block wide(synth, 7200);
    check(std::abs(pitch(wide.left, 4800, 7200) - 427.47f) < 1.0f
          && std::abs(pitch(wide.right, 4800, 7200) - 452.89f) < 1.0f,
          "Stereo unison did not pan and detune its layers independently");
    p.voicing.unison = maxUnison;
    synth.setPatch(p);
    std::array<Event, Synth::polyphony - 1> chord {};
    for (std::size_t i = 0; i < chord.size(); ++i)
        chord[i].note = 42 + static_cast<int>(i);
    std::array<float, 4096> left {}, right {};
    synth.render(left.data(), right.data(), static_cast<int>(left.size()), chord.data(), chord.size());
    check(synth.activeVoices() == Synth::polyphony, "Unison reduced note polyphony");
    check(peak({ left.begin(), left.end() }) < 1.0f && peak({ right.begin(), right.end() }) < 1.0f,
          "Unison chord exceeded full scale");
}

void performanceMacros(const WaveBank& bank)
{
    Patch patch;
    const auto targetIndex = [&](float& parameter)
    {
        const auto& targets = macroTargets();
        const auto it = std::find_if(targets.begin(), targets.end(),
                                     [&](const auto& target) { return &target.parameter(patch) == &parameter; });
        check(it != targets.end(), "Parameter missing from macro destinations");
        return static_cast<int>(it - targets.begin());
    };
    const int position = targetIndex(patch.osc[0].position);
    const int level = targetIndex(patch.osc[1].level);
    const int filter = targetIndex(patch.cutoff);
    setMacroRange(patch, 0, position, { 0.2f, 0.8f });
    setMacroRange(patch, 0, level, { 0.9f, 0.1f });
    setMacroRange(patch, 0, filter, { 200, 3200 });
    moveMacro(patch, 0, 0.5f);
    check(std::abs(patch.osc[0].position - 0.5f) < 0.00001f && std::abs(patch.osc[1].level - 0.5f) < 0.00001f
          && std::abs(patch.cutoff - 800.0f) < 0.01f, "Macro midpoint failed");
    moveMacro(patch, 0, 1.0f);
    check(patch.osc[0].position == 0.8f && patch.osc[1].level == 0.1f && patch.cutoff == 3200,
          "Macro clockwise endpoints failed");
    setMacroRange(patch, 1, filter, { 100, 500 });
    moveMacro(patch, 1, 1.0f);
    check(patch.cutoff == 500 && patch.osc[1].level == 0.1f && patch.macros[0].value == 1.0f,
          "Second macro affected other targets");
    moveMacro(patch, 0, 0.0f);
    check(patch.osc[0].position == 0.2f && patch.osc[1].level == 0.9f && patch.cutoff == 200,
          "Macro counterclockwise endpoints failed");

    setMacroRange(patch, 1, targetIndex(patch.volume), { 0.65f, 0.0f });
    moveMacro(patch, 1, 0.0f);
    Synth synth(bank);
    synth.setPatch(patch);
    check(peak(Block(synth, 4096, { {} }).left) > 0.01f, "Held note was silent before macro change");
    moveMacro(patch, 1, 1.0f);
    synth.setPatch(patch);
    Block settling(synth, 4096);
    check(peak(Block(synth, 1024).left) < 0.0001f, "Macro did not change audio on a held note");

    check(encoderValue(EncoderMode::absolute, 0, 0.5f) == 0.0f
          && encoderValue(EncoderMode::absolute, 127, 0.5f) == 1.0f, "Absolute MIDI endpoints failed");
    for (const auto mode : { EncoderMode::relative1, EncoderMode::relative2, EncoderMode::relative3 })
    {
        const int positive = mode == EncoderMode::relative1 ? 66 : mode == EncoderMode::relative2 ? 2 : 18;
        const int negative = mode == EncoderMode::relative1 ? 62 : mode == EncoderMode::relative2 ? 126 : 14;
        check(encoderValue(mode, positive, 0.5f) == 0.5f + 2.0f / 127.0f
              && encoderValue(mode, negative, 0.5f) == 0.5f - 2.0f / 127.0f
              && !encoderValue(mode, 0, 0.5f)
              && encoderValue(mode, positive, 1.0f) == 1.0f, "Relative MIDI decoding failed");
    }
}

void patches()
{
    for (const auto& preset : factoryPresets())
    {
        const auto encoded = encodePatch(preset);
        const auto decoded = decodePatch(encoded);
        check(decoded && encodePatch(*decoded) == encoded, "Preset failed round-trip");
    }
    NamedPatch custom { "Custom \"01\"", factoryPresets()[2].sound };
    custom.sound.matrix[7][6] = -0.987f;
    custom.sound.voicing = { PlayMode::legato, 0.123f, 4, 35.0f, 0.83f };
    custom.sound.macros[0].ranges[1] = MacroRange { 0.8f, 0.13f };
    custom.sound.macros[1].ranges[8] = MacroRange { 180, 4000 };
    moveMacro(custom.sound, 1, 0.65f);
    const auto encoded = encodePatch(custom);
    const auto decoded = decodePatch(encoded);
    check(decoded && encodePatch(*decoded) == encoded, "Custom patch failed round-trip");
    check(!decodePatch(encoded.substr(0, encoded.size() / 2)), "Truncated patch was accepted");
    check(!decodePatch("MOT11 4" + encoded.substr(7)), "Unknown patch version was accepted");
    check(!decodePatch(encoded + "junk"), "Unexpected trailing content was accepted");
    const auto voicing = encoded.find("VOICING ");
    const auto matrix = encoded.find("MATRIX\n", voicing);
    const auto macros = encoded.find("MACROS\n", matrix);
    const auto versionTwo = decodePatch("MOT11 2" + encoded.substr(7, macros - 7));
    check(versionTwo && versionTwo->sound.voicing.mode == PlayMode::legato
          && versionTwo->sound.macros[1].value == 0 && !versionTwo->sound.macros[1].ranges[8],
          "Version 2 patch failed to load with default macros");
    const auto legacy = decodePatch("MOT11 1" + encoded.substr(7, voicing - 7)
                                    + encoded.substr(matrix, macros - matrix));
    const Voicing defaults;
    check(legacy && legacy->sound.voicing.mode == defaults.mode && legacy->sound.voicing.glide == defaults.glide
          && legacy->sound.voicing.unison == defaults.unison && legacy->sound.voicing.detune == defaults.detune
          && legacy->sound.voicing.width == defaults.width && legacy->sound.matrix == custom.sound.matrix,
          "Legacy patch failed to load with default voicing");
    check(!decodePatch(encoded.substr(0, voicing) + "VOICING 2 0.1 5 50 1\n" + encoded.substr(matrix)),
          "Out-of-range unison was accepted");
    check(!decodePatch(encoded.substr(0, macros) + "MACROS\nMACRO 0 1\n8 0 1000\nMACRO 0 0\n"),
          "Out-of-range macro endpoint was accepted");
    check(!decodePatch(encoded.substr(0, macros) + "MACROS\nMACRO 0 2\n1 0 1\n1 1 0\nMACRO 0 0\n"),
          "Duplicate macro destination was accepted");
}
}

int main()
{
    try
    {
        const WaveBank bank;
        notes(bank);
        modulationAndPolyphony(bank);
        monoPlaying(bank);
        glideAndUnison(bank);
        performanceMacros(bank);
        patches();
        std::cout << "MO-T 11 headless checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
