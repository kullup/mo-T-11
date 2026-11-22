#include "Synth.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <sstream>
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
    p.matrix[velocity][defaultMatrixColumn(pan)] = 1.0f;
    synth.setPatch(p);
    Block positive(synth, 2048, { Event {} });
    check(peak(positive.left) < 0.00001f && peak(positive.right) > 0.05f, "Positive pan route is incorrect");
    p.matrix[velocity][defaultMatrixColumn(pan)] = -1.0f;
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
        check(isFixedTable(table) ? difference == 0.0f : difference > 0.5f,
              "Wavetable position did not match its fixed or morphing mode");
        check(std::abs(bank.display(table, 0.5f, 0) - bank.display(table, 0.5f, 1)) < 1.0e-6f,
              "Wavetable is discontinuous at the cycle seam");
        check(bank.sample(table, 0.5f, 0.6f, 24000, 48000) == 0.0f,
              "A fundamental beyond the band limit was rendered");
    }
}

void assignableMatrix(const WaveBank& bank)
{
    Synth synth(bank);
    constexpr int col = defaultMatrixColumn(positionA);
    for (int i = 0; i < oscillatorCount; ++i)
    {
        Patch p = sinePatch();
        for (auto& osc : p.osc)
            osc.level = 0.0f;
        p.osc[i].table = sineTable;
        p.osc[i].level = 0.8f;
        p.matrixTargets[col] = levelDestinations[i];
        p.matrix[velocity][col] = -1.0f;
        synth.prepare(48000);
        synth.setPatch(p);
        const Block muted(synth, 4096, { Event {} });
        check(peak(muted.left) == 0.0f && peak(muted.right) == 0.0f, "Oscillator level route failed to mute it");

        p.matrixTargets[col] = panDestinations[i];
        synth.setPatch(p);
        const Block repanned(synth, 4096);
        check(peak(repanned.left) > 0.05f && peak(repanned.right) < 0.00001f,
              "Retargeted route did not apply its existing depth to oscillator pan");
    }

    Patch p = sinePatch();
    synth.prepare(48000);
    synth.setPatch(p);
    const Block dry(synth, 4096, { Event {} });
    p.matrixTargets[col] = filterDrive;
    p.matrix[velocity][col] = 1.0f;
    synth.prepare(48000);
    synth.setPatch(p);
    const Block driven(synth, 4096, { Event {} });
    check(peak(driven.left) > 1.2f * peak(dry.left), "Filter drive modulation did not affect the signal");

    p.matrixTargets[col] = ampGain;
    p.matrix[velocity][col] = -1.0f;
    synth.prepare(48000);
    synth.setPatch(p);
    const Block silent(synth, 4096, { Event {} });
    check(peak(silent.left) == 0.0f && peak(silent.right) == 0.0f, "Amp gain route failed to silence the voice");
}

void analogOscillators(const WaveBank& bank)
{
    const auto ratioAtEighth = [&](int table)
    {
        return bank.display(table, 0.5f, 0.125f) / bank.display(table, 0.5f, 0.25f);
    };
    check(std::abs(ratioAtEighth(sineTable) - std::sqrt(0.5f)) < 0.01f, "Sine table has the wrong shape");
    check(std::abs(ratioAtEighth(triangleTable) - 0.5f) < 0.01f, "Triangle table has the wrong slope");
    check(std::abs(ratioAtEighth(sawTable) - 0.5f) < 0.01f && bank.display(sawTable, 0.5f, 0.625f) < -0.5f,
          "Sawtooth table has the wrong slope or reset");
    check(std::abs(ratioAtEighth(squareTable) - 1.0f) < 0.02f && bank.display(squareTable, 0.5f, 0.75f) < -0.7f,
          "Square table has the wrong shape");
    for (float position : { 0.0f, 0.27f, 0.5f, 1.0f })
    {
        int high = 0;
        for (int sample = 0; sample < 1024; ++sample)
            high += bank.display(pulseTable, position, sample / 1024.0f) > 0.0f;
        check(std::abs(high / 1024.0f - pulseDuty(position)) < 0.01f, "Pulse position did not set the expected duty cycle");
    }

    Patch p = sinePatch();
    p.osc[0].level = 0.0f;
    p.osc[2] = { sineTable, 0.25f, 0.8f, 0, 0, 0.0f, -1.0f };
    p.matrix[velocity][defaultMatrixColumn(pitchC)] = -1.0f;
    Synth synth(bank);
    synth.setPatch(p);
    const Event a440 { EventType::noteOn, 0, 1, 69 };
    const Block third(synth, 7200, { a440 });
    check(peak(third.left) > 0.05f && peak(third.right) < 0.00001f
          && std::abs(pitch(third.left, 4200, 7200) - 220.0f) < 0.5f,
          "Oscillator C did not render with its pan and pitch route");

    p.osc[2].table = pulseTable;
    p.matrix[velocity][defaultMatrixColumn(positionC)] = 0.5f;
    synth.prepare(48000);
    synth.setPatch(p);
    const Block modulated(synth, 4800, { a440 });
    p.osc[2].position = 0.75f;
    p.matrix[velocity][defaultMatrixColumn(positionC)] = 0.0f;
    synth.prepare(48000);
    synth.setPatch(p);
    const Block direct(synth, 4800, { a440 });
    check(modulated.left == direct.left, "Oscillator C position route did not set pulse width");

    auto tuned = sinePatch();
    tuned.osc[0].table = sineTable;
    for (int semitones : { -24, 24 })
    {
        tuned.osc[0].semitone = semitones;
        synth.prepare(48000);
        synth.setPatch(tuned);
        const Block shifted(synth, 9600, { a440 });
        const float expected = semitones < 0 ? 110.0f : 1760.0f;
        check(std::abs(pitch(shifted.left, 4800, 9600) - expected) < 1.0f, "Two-octave oscillator tune is incorrect");
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

void effects(const WaveBank& bank)
{
    Synth synth(bank);
    auto patch = sinePatch();
    patch.ampEnv = { 0.002f, 0.02f, 1.0f, 0.005f };
    patch.tapeDelay = { true, 0.15f, 0.62f, 0.32f, 0.55f };
    Event release { EventType::noteOff };
    for (const int rate : { 44100, 96000 })
    {
        const auto samples = [rate](double time) { return static_cast<int>(rate * time); };
        release.offset = samples(0.01);
        synth.setPatch(patch);
        synth.prepare(rate);
        const Block note(synth, samples(0.02), { {}, release });
        check(peak(note.left) > 0.02f && synth.activeVoices() == 0, "Tape delay removed the dry note");
        check(peak(Block(synth, samples(0.10)).left) == 0.0f, "Tape echo arrived early");
        const Block firstEcho(synth, samples(0.07));
        check(peak(firstEcho.left) > 0.02f && peak(firstEcho.right) > 0.02f
              && firstEcho.left != firstEcho.right, "Tape delay did not produce stereo echoes");
        check(peak(Block(synth, samples(0.17)).left) > 0.005f, "Tape delay did not produce a feedback repeat");
        check(peak(Block(synth, samples(0.5), { { EventType::panic } }).left) == 0.0f,
              "Panic left tape delay repeats in the buffer");
    }

    patch.tapeDelay.enabled = false;
    patch.reverb = { true, 0.85f, 0.30f, 0.60f };
    synth.setPatch(patch);
    synth.prepare(48000);
    release.offset = 480;
    Block(synth, 960, { {}, release });
    const Block tail(synth, 24000);
    check(peak(tail.left) > 0.005f && peak(tail.right) > 0.005f && tail.left != tail.right,
          "Reverb did not ring out in stereo after the note ended");
    check(peak(Block(synth, 24000, { { EventType::panic } }).left) == 0.0f, "Panic did not clear reverb");

    Block(synth, 960, { {}, release });
    Block(synth, 2400);
    patch.reverb.enabled = false;
    synth.setPatch(patch);
    Block(synth, 9600);
    check(peak(Block(synth, 4800).left) == 0.0f, "Bypass left reverb audible");
    patch.reverb.enabled = true;
    synth.setPatch(patch);
    check(peak(Block(synth, 4800).left) == 0.0f, "Re-enabling reverb brought back a bypassed tail");
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
    custom.sound.osc[2] = { pulseTable, 0.67f, 0.55f, -1, 3, 2.5f, -0.4f, 0.2f, false };
    custom.sound.matrix[pressure][defaultMatrixColumn(pan)] = -0.987f;
    custom.sound.matrix[wheel][defaultMatrixColumn(positionC)] = 0.27f;
    custom.sound.matrix[velocity][defaultMatrixColumn(pitchC)] = -0.31f;
    custom.sound.voicing = { PlayMode::legato, 0.123f, 4, 35.0f, 0.83f };
    const auto encoded = encodePatch(custom);
    const auto decoded = decodePatch(encoded);
    check(decoded && encodePatch(*decoded) == encoded, "Custom patch failed round-trip");
    check(!decodePatch(encoded.substr(0, encoded.size() / 2)), "Truncated patch was accepted");
    check(!decodePatch("MOT11 7" + encoded.substr(7)), "Unknown patch version was accepted");
    check(!decodePatch(encoded + "junk"), "Unexpected trailing content was accepted");
    const auto voicing = encoded.find("VOICING ");
    const auto targets = encoded.find("TARGETS ", voicing);
    const auto matrix = encoded.find("MATRIX\n", targets);
    const auto tape = encoded.find("TAPE_DELAY ", matrix);
    const auto third = encoded.find("OSC_C "), filter = encoded.find("FILTER ");
    const auto oldMatrix = [&](Destination last)
    {
        std::ostringstream out;
        out << "MATRIX\n" << std::setprecision(std::numeric_limits<float>::max_digits10);
        for (const auto& row : custom.sound.matrix)
        {
            for (int i = 0; i <= last; ++i)
                out << row[defaultMatrixColumn(static_cast<Destination>(i))] << ' ';
            out << '\n';
        }
        return out.str();
    };
    const auto legacy4 = decodePatch("MOT11 4" + encoded.substr(7, targets - 7) + oldMatrix(pitchC));
    check(legacy4 && encodePatch(*legacy4) == encoded, "Version 4 patch failed to migrate");
    const auto legacy5 = decodePatch("MOT11 5" + encoded.substr(7, tape - 7));
    check(legacy5 && encodePatch(*legacy5) == encoded, "Version 5 patch did not load with both effects bypassed");
    custom.sound.tapeDelay = { true, maxTapeTime, maxTapeFeedback, 0.71f, 0.27f };
    custom.sound.reverb = { true, 0.21f, 0.87f, 0.52f };
    const auto withEffects = encodePatch(custom);
    const auto decodedEffects = decodePatch(withEffects);
    check(decodedEffects && encodePatch(*decodedEffects) == withEffects, "Effect settings failed round-trip");
    check(!decodePatch(withEffects.substr(0, tape) + "TAPE_DELAY 1 0.32 1 0.8 0.1\nREVERB 1 1 0.9 1\n"),
          "Out-of-range tape feedback was accepted");
    custom.sound.tapeDelay = {};
    custom.sound.reverb = {};
    auto migrated = custom;
    migrated.sound.osc[2] = Patch {}.osc[2];
    for (auto& row : migrated.sound.matrix)
        row[defaultMatrixColumn(positionC)] = row[defaultMatrixColumn(pitchC)] = 0.0f;
    const auto oldPrefix = encoded.substr(7, third - 7) + encoded.substr(filter, voicing - filter);
    const auto legacy2 = decodePatch("MOT11 2" + oldPrefix + encoded.substr(voicing, targets - voicing) + oldMatrix(pan));
    check(legacy2 && encodePatch(*legacy2) == encodePatch(migrated), "Version 2 patch failed to migrate");
    migrated.sound.voicing = {};
    const auto legacy1 = decodePatch("MOT11 1" + oldPrefix + oldMatrix(pan));
    check(legacy1 && encodePatch(*legacy1) == encodePatch(migrated), "Version 1 patch failed to migrate");
    check(!decodePatch(encoded.substr(0, voicing) + "VOICING 2 0.1 5 50 1\n" + encoded.substr(targets)),
          "Out-of-range unison was accepted");
    check(!decodePatch(encoded.substr(0, targets) + "TARGETS 0 1 7 2 3 8 4 5 99\n" + encoded.substr(matrix)),
          "Out-of-range matrix destination was accepted");
    check(!decodePatch(encoded.substr(0, targets) + "TARGETS 0 1 7 2 3 8 4 5 0\n" + encoded.substr(matrix)),
          "Duplicate matrix destination was accepted");

    custom.sound.matrixTargets[defaultMatrixColumn(pan)] = ampGain;
    const auto reassigned = encodePatch(custom);
    const auto decodedReassigned = decodePatch(reassigned);
    check(decodedReassigned && encodePatch(*decodedReassigned) == reassigned,
          "Reassigned target and route depths failed to round-trip");

    custom.sound.osc[0].semitone = -24;
    custom.sound.osc[1].semitone = 24;
    const auto tuned = encodePatch(custom);
    const auto decodedTuned = decodePatch(tuned);
    check(decodedTuned && encodePatch(*decodedTuned) == tuned, "Two-octave oscillator tune failed round-trip");
    custom.sound.osc[1].semitone = 25;
    check(!decodePatch(encodePatch(custom)), "Out-of-range oscillator tune was accepted");
}
}

int main()
{
    try
    {
        const WaveBank bank;
        notes(bank);
        modulationAndPolyphony(bank);
        assignableMatrix(bank);
        analogOscillators(bank);
        monoPlaying(bank);
        glideAndUnison(bank);
        effects(bank);
        patches();
        std::cout << "MO-T 11 headless checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
