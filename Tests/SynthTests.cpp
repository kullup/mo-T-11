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

float pitch(const std::vector<float>& values, int begin, int end, int sampleRate = 48000)
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
    return (crossings - 1) * static_cast<float>(sampleRate) / (last - first);
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
    for (int i = 0; i < oscillatorCount; ++i)
    {
        Patch p = sinePatch();
        for (auto& osc : p.osc)
            osc.level = 0.0f;
        p.osc[i].table = sineTable;
        p.osc[i].level = 0.8f;
        p.matrix[velocity][levelDestinations[i]] = -1.0f;
        synth.prepare(48000);
        synth.setPatch(p);
        const Block muted(synth, 4096, { Event {} });
        check(peak(muted.left) == 0.0f && peak(muted.right) == 0.0f, "Oscillator level route failed to mute it");

        std::swap(p.matrix[velocity][levelDestinations[i]], p.matrix[velocity][panDestinations[i]]);
        synth.setPatch(p);
        const Block repanned(synth, 4096);
        check(peak(repanned.left) > 0.05f && peak(repanned.right) < 0.00001f,
              "Retargeted route did not apply its existing depth to oscillator pan");
    }

    Patch p = sinePatch();
    synth.prepare(48000);
    synth.setPatch(p);
    const Block dry(synth, 4096, { Event {} });
    p.matrix[velocity][filterDrive] = 1.0f;
    synth.prepare(48000);
    synth.setPatch(p);
    const Block driven(synth, 4096, { Event {} });
    check(peak(driven.left) > 1.2f * peak(dry.left), "Filter drive modulation did not affect the signal");

    p.matrix[velocity][filterDrive].reset();
    p.matrix[velocity][ampGain] = -1.0f;
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
    p.matrix[velocity][pitchC] = -1.0f;
    Synth synth(bank);
    synth.setPatch(p);
    const Event a440 { EventType::noteOn, 0, 1, 69 };
    const Block third(synth, 7200, { a440 });
    check(peak(third.left) > 0.05f && peak(third.right) < 0.00001f
          && std::abs(pitch(third.left, 4200, 7200) - 220.0f) < 0.5f,
          "Oscillator C did not render with its pan and pitch route");

    p.osc[2].table = pulseTable;
    p.matrix[velocity][positionC] = 0.5f;
    synth.prepare(48000);
    synth.setPatch(p);
    const Block modulated(synth, 4800, { a440 });
    p.osc[2].position = 0.75f;
    p.matrix[velocity][positionC] = 0.0f;
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

void pluckedStrings(const WaveBank& bank)
{
    const Event a440 { EventType::noteOn, 0, 1, 69 };
    const Event a220 { EventType::noteOn, 0, 1, 57 };
    Synth synth(bank);
    Patch p;
    p.ampEnv = { 0.002f, 0.02f, 1.0f, 0.10f };
    p.cutoff = 14000.0f;
    auto& string = p.osc[0];
    string.mode = OscillatorMode::pluckedString;
    string.brightness = 0.0f;

    for (int rate : { 44100, 48000, 96000 })
    {
        synth.prepare(rate);
        synth.setPatch(p);
        const Block pluck(synth, rate / 5, { a440 });
        const float frequency = pitch(pluck.left, rate / 20, rate / 5, rate);
        check(std::abs(frequency - 440.0f) < 1.0f, "String pitch changed with sample rate");
    }

    const auto renderPluck = [&]
    {
        synth.prepare(48000);
        synth.setPatch(p);
        return Block(synth, 3000, { a440 });
    };
    const auto overtone = [](const Block& block)
    {
        constexpr double angleStep = 6.283185307179586 * 880.0 / 48000.0;
        double sine = 0.0, cosine = 0.0;
        for (int i = 480; i < 2880; ++i)
        {
            sine += block.left[i] * std::sin(angleStep * i);
            cosine += block.left[i] * std::cos(angleStep * i);
        }
        return std::hypot(sine, cosine);
    };
    const Block dark = renderPluck();
    string.brightness = 1.0f;
    const Block bright = renderPluck();
    check(overtone(bright) > 2.0 * overtone(dark), "String brightness did not change overtone level");
    string.brightness = 0.0f;
    p.matrix[velocity][positionA] = 1.0f;
    check(renderPluck().left == bright.left, "Position route did not set string brightness");
    p.matrix[velocity][positionA].reset();

    string.stringDecay = minStringDecay;
    renderPluck();
    Block(synth, 14400);
    const Block shortRing(synth, 4800);
    string.stringDecay = maxStringDecay;
    renderPluck();
    Block(synth, 14400);
    const Block longRing(synth, 4800);
    check(peak(shortRing.left) < 1.0e-5f && peak(longRing.left) > 0.02f, "String decay did not control ring time");

    string.stringDecay = minStringDecay;
    p.voicing.mode = PlayMode::legato;
    renderPluck();
    Block(synth, 14400);
    const Block connected(synth, 3000, { a220 });
    check(peak(connected.left) < 1.0e-5f, "Legato re-excited the string");
    p.voicing.mode = PlayMode::mono;
    renderPluck();
    Block(synth, 14400);
    const Block picked(synth, 3000, { a220 });
    check(peak(picked.left) > 0.02f, "Mono did not re-excite the string");
    const Block panic(synth, 4800, { { EventType::panic } });
    check(synth.activeVoices() == 0 && panic.left.back() == 0.0f, "Panic left a ringing string");

    p.voicing.mode = PlayMode::poly;
    string.level = 0.0f;
    renderPluck();
    Block(synth, 14400);
    string.level = 0.8f;
    synth.setPatch(p);
    check(peak(Block(synth, 3000).left) < 1.0e-5f, "A muted string stopped decaying");

    string.mode = OscillatorMode::waveform;
    p.osc[0].table = sineTable;
    const Block wave = renderPluck();
    string.mode = OscillatorMode::pluckedString;
    synth.setPatch(p);
    const Block switched(synth, 3000);
    check(std::abs(switched.left.front() - wave.left.back()) < 0.015f && peak(switched.left) > 0.02f,
          "Live string selection did not crossfade and excite the held note");
    Block(synth, 14400);
    check(peak(Block(synth, 3000).left) < 1.0e-5f, "A string selected on a held note did not decay");

    auto wire = factoryPresets().back().sound;
    wire.osc[0].pan = -1.0f;
    wire.osc[1].pan = 1.0f;
    synth.prepare(48000);
    synth.setPatch(wire);
    const Block onset(synth, 4800, { a440 });
    Block(synth, 48000);
    const Block sustain(synth, 4800);
    check(peak(onset.left) > 0.02f && peak(sustain.left) < 1.0e-5f && peak(sustain.right) > 0.01f,
          "Wire Bloom did not pair a string attack with a sustained wavetable");

    p.voicing = { PlayMode::poly, 0.0f, maxUnison, 50.0f, 1.0f };
    string.level = 0.9f;
    p.osc.fill(string);
    p.osc[1].octave = 2;
    p.osc[1].semitone = 24;
    p.osc[2].octave = -2;
    p.osc[2].semitone = -24;
    synth.prepare(48000);
    synth.setPatch(p);
    std::array<Event, Synth::polyphony> chord {};
    for (int i = 0; i < Synth::polyphony; ++i)
        chord[i].note = i * 8;
    Block stack(synth, 4096);
    synth.render(stack.left.data(), stack.right.data(), 4096, chord.data(), chord.size());
    const float left = peak(stack.left), right = peak(stack.right);
    check(synth.activeVoices() == Synth::polyphony && left > 0.02f && left < 1.0f && right > 0.02f && right < 1.0f,
          "Plucked strings failed under full polyphony and unison");
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

void plateDecay()
{
    check(plateDecaySeconds(0.0f) == 0.25f && plateDecaySeconds(1.0f) == 20.0f,
          "Plate knob does not cover the full decay range");
    for (const int rate : { 44100, 96000 })
    {
        const auto measure = [rate](float decay, int lateSecond)
        {
            Patch patch;
            patch.reverb = { true, decay, 0.2f, 1.0f };
            Effects fx;
            fx.prepare(static_cast<float>(rate), patch);
            std::array<double, 2> energy {};
            for (int i = 0; i < (lateSecond + 1) * rate; ++i)
            {
                float left = i == 0 ? 1.0f : 0.0f, right = left;
                fx.process(left, right, patch);
                check(std::isfinite(left) && std::isfinite(right), "Plate impulse response became non-finite");
                const double power = left * left + right * right;
                if (i < rate) energy[0] += power;
                if (i >= lateSecond * rate) energy[1] += power;
            }
            return energy;
        };
        const auto shortTail = measure(0.0f, 2);
        const auto longTail = measure(1.0f, 15);
        check(shortTail[1] < shortTail[0] * 1.0e-8, "Minimum plate decay left a long tail");
        check(longTail[1] > longTail[0] * 1.0e-7 && longTail[1] < longTail[0] * 0.001,
              "Maximum plate decay did not sustain a fading tail for 15 seconds");
    }
}

std::vector<float> renderScore(Synth& synth, int samples, const std::vector<Event>& score, int blockSize)
{
    std::vector<float> audio(samples);
    std::size_t next = 0;
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int size = std::min(blockSize, samples - offset);
        std::vector<Event> events;
        while (next < score.size() && score[next].offset < offset + size)
        {
            events.push_back(score[next++]);
            events.back().offset -= offset;
        }
        synth.render(audio.data() + offset, nullptr, size, events.data(), events.size());
    }
    return audio;
}

void arpeggiator(const WaveBank& bank)
{
    const auto noteAt = [](int note, int offset, EventType type = EventType::noteOn)
    {
        Event event { type, 0, 1, note };
        event.offset = offset;
        return event;
    };
    const auto patch = sinePatch();
    constexpr int start = 73, step = 6000, gate = 4200;
    const std::vector<Event> chord { noteAt(67, start), noteAt(60, start), noteAt(64, start) };
    constexpr std::array patterns {
        std::array { 60, 64, 67, 60, 64, 67 },
        std::array { 67, 64, 60, 67, 64, 60 },
        std::array { 60, 64, 67, 64, 60, 64 }
    };
    Synth synth(bank), reference(bank);
    for (std::size_t direction = 0; direction < patterns.size(); ++direction)
    {
        std::vector<Event> score;
        int time = start;
        for (int note : patterns[direction])
        {
            score.push_back(noteAt(note, time));
            score.push_back(noteAt(note, time + gate, EventType::noteOff));
            time += step;
        }
        auto p = patch;
        p.voicing.unison = 2;
        reference.prepare(48000);
        reference.setPatch(p);
        const auto expected = renderScore(reference, time, score, time);
        p.arp = { true, 120, static_cast<ArpDirection>(direction) };
        for (auto mode : { PlayMode::poly, PlayMode::mono, PlayMode::legato })
        {
            p.voicing.mode = mode;
            p.voicing.glide = 0.15f;
            synth.prepare(48000);
            synth.setPatch(p);
            check(renderScore(synth, time, chord, 257) == expected,
                  "Arp pitch order, retrigger, or sample timing differed from the score");
        }
    }

    auto p = patch;
    p.arp = { true, 137, ArpDirection::up };
    synth.prepare(44100);
    synth.setPatch(p);
    reference.prepare(44100);
    reference.setPatch(patch);
    constexpr double fractionalStep = 15.0 * 44100 / 137;
    std::vector<Event> fractionalScore;
    for (int i = 0; i < 12; ++i)
    {
        const int note = i % 2 == 0 ? 60 : 67;
        fractionalScore.push_back(noteAt(note, static_cast<int>(std::ceil(i * fractionalStep))));
        fractionalScore.push_back(noteAt(note, static_cast<int>(std::ceil((i + 0.7) * fractionalStep)), EventType::noteOff));
    }
    const int duration = static_cast<int>(std::ceil(12 * fractionalStep));
    check(renderScore(synth, duration, { noteAt(60, 0), noteAt(67, 0) }, 127)
              == renderScore(reference, duration, fractionalScore, 2048),
          "Fractional arp timing drifted at 44.1 kHz");

    p.arp.tempo = 120;
    synth.prepare(48000);
    synth.setPatch(p);
    reference.prepare(48000);
    reference.setPatch(patch);
    const Block first(synth, 3000, { noteAt(60, 0), noteAt(67, 0) });
    check(first.left == Block(reference, 3000, { noteAt(60, 0) }).left, "Arp started with the wrong pitch");
    p.arp.tempo = 240;
    p.arp.direction = ArpDirection::down;
    synth.setPatch(p);
    reference.setPatch(patch);
    const Block edited(synth, 3000);
    check(edited.left == Block(reference, 3000, { noteAt(60, 600, EventType::noteOff), noteAt(67, 1500) }).left,
          "Live tempo or direction edit restarted the clock or changed the active pitch");

    p = patch;
    p.matrix[pressure][pan] = 1.0f;
    reference.prepare(48000);
    reference.setPatch(p);
    p.arp.enabled = true;
    synth.prepare(48000);
    synth.setPatch(p);
    auto older = noteAt(60, 0);
    older.velocity = 0.4f;
    auto newer = older;
    newer.port = 4;
    newer.channel = 16;
    newer.velocity = 0.9f;
    auto expression = newer;
    expression.type = EventType::expression;
    expression.controls.pressure = 1.0f;
    expression.offset = 256;
    auto releaseNewer = newer;
    releaseNewer.type = EventType::noteOff;
    releaseNewer.offset = step + 200;
    auto gateOff = [](Event event, int offset)
    {
        event.type = EventType::noteOff;
        event.offset = offset;
        return event;
    };
    auto alternate = noteAt(67, step);
    auto fallback = older;
    fallback.offset = 2 * step;
    const std::vector<Event> duplicateScore { newer, expression, gateOff(newer, gate), alternate,
                                              gateOff(alternate, step + gate), fallback };
    check(renderScore(synth, 2 * step + 1500, { older, newer, noteAt(67, 0), expression, releaseNewer }, 511)
              == renderScore(reference, 2 * step + 1500, duplicateScore, 512),
          "Arp repeated a duplicate pitch or lost source velocity or expression");

    p = patch;
    p.arp.enabled = true;
    synth.prepare(48000);
    synth.setPatch(p);
    Event pedal { EventType::expression, 2, 7 };
    pedal.controls.sustain = true;
    const Event on { EventType::noteOn, 2, 7 };
    const Event off { EventType::noteOff, 2, 7 };
    Block sustain(synth, step + 1000, { on, pedal, off });
    check(synth.activeVoices() == 1 && peak(sustain.left) > 0.02f, "Sustain did not keep the arp running");
    pedal.controls.sustain = false;
    Block pedalUp(synth, step + 1000, { pedal });
    check(synth.activeVoices() == 0, "Pedal-up left an arp voice active");
    check(peak(Block(synth, step).left) == 0.0f, "Arp retriggered after pedal-up");

    p.ampEnv.release = 0.2f;
    synth.setPatch(p);
    Block repeat(synth, step + 1000, { on });
    check(synth.activeVoices() == 2, "A repeated arp pitch cut its previous release tail");
    Block hardMute(synth, step + 1000, { { EventType::allSoundOff, 2, 7 } });
    check(synth.activeVoices() == 0, "Channel sound-off left arp gates or tails active");
    Block connected(synth, 1000, { on });
    check(synth.activeVoices() == 1, "Arp did not restart immediately after sound-off");
    Block disconnected(synth, step + 1000, { { EventType::disconnect, 2 } });
    check(synth.activeVoices() == 0, "Disconnect left arp notes active");
    Block allNotesOff(synth, 2 * step, { on, { EventType::allNotesOff, 2, 7 } });
    check(peak(allNotesOff.left) == 0.0f, "All-notes-off left a pending arp trigger");
    Block beforePanic(synth, 1000, { on });
    Block panic(synth, 2 * step, { { EventType::panic } });
    check(synth.activeVoices() == 0 && peak(Block(synth, step).left) == 0.0f,
          "Panic left the arp clock or voices active");

    Block held(synth, 1000, { on, noteAt(67, 0) });
    p.arp.enabled = false;
    synth.setPatch(p);
    check(synth.activeVoices() == 2, "Disabling arp did not restore the held chord");
    p.arp.enabled = true;
    synth.setPatch(p);
    check(peak(Block(synth, 1000).left) > 0.02f && synth.activeVoices() == 1,
          "Enabling arp with a held chord did not start a single step");
    synth.prepare(44100);
    check(peak(Block(synth, step).left) == 0.0f, "Audio restart left the arp running");
}

void patches()
{
    for (const auto& preset : factoryPresets())
    {
        const auto encoded = encodePatch(preset);
        const auto decoded = decodePatch(encoded);
        check(decoded && !decoded->sound.arp.enabled && encodePatch(*decoded) == encoded, "Preset failed round-trip");
    }
    NamedPatch custom { "Custom \"01\"", factoryPresets()[2].sound };
    custom.sound.osc[2] = { pulseTable, 0.67f, 0.55f, -1, 3, 2.5f, -0.4f, 0.2f, false };
    custom.sound.matrix[pressure][pan] = -0.987f;
    custom.sound.matrix[wheel][positionC] = 0.27f;
    custom.sound.matrix[velocity][pitchC] = -0.31f;
    custom.sound.voicing = { PlayMode::legato, 0.123f, 4, 35.0f, 0.83f };
    const auto encoded = encodePatch(custom);
    const auto decoded = decodePatch(encoded);
    check(decoded && encodePatch(*decoded) == encoded, "Custom patch failed round-trip");
    check(!decodePatch(encoded.substr(0, encoded.size() / 2)), "Truncated patch was accepted");
    check(!decodePatch("MOT11 10" + encoded.substr(7)), "Unknown patch version was accepted");
    check(!decodePatch(encoded + "junk"), "Unexpected trailing content was accepted");
    const auto voicing = encoded.find("VOICING ");
    const auto routes = encoded.find("ROUTES ", voicing);
    const auto tape = encoded.find("TAPE_DELAY ", routes);
    const auto arp = encoded.find("ARP ", tape);
    const auto modes = encoded.find("OSC_MODES ", arp);
    const auto legacy8 = decodePatch("MOT11 8" + encoded.substr(7, modes - 7));
    check(legacy8 && encodePatch(*legacy8) == encoded, "Version 8 patch did not load with waveform defaults");
    const auto legacy7 = decodePatch("MOT11 7" + encoded.substr(7, arp - 7));
    check(legacy7 && encodePatch(*legacy7) == encoded, "Version 7 patch did not load with arp defaults");
    auto withArp = custom;
    withArp.sound.arp = { true, 163, ArpDirection::upDown };
    const auto encodedArp = encodePatch(withArp);
    const auto decodedArp = decodePatch(encodedArp);
    check(decodedArp && encodePatch(*decodedArp) == encodedArp, "Arp settings failed round-trip");
    const auto legacy8Prefix = "MOT11 8" + encoded.substr(7, arp - 7);
    check(!decodePatch(legacy8Prefix), "Missing arp settings were accepted in a version 8 patch");
    for (const auto* invalid : { "ARP 2 120 0\n", "ARP 1 39 0\n", "ARP 1 241 0\n", "ARP 1 120 3\n" })
        check(!decodePatch(legacy8Prefix + invalid), "Invalid arp settings were accepted");
    check(!decodePatch(encoded.substr(0, modes)), "Version 9 patch accepted missing oscillator modes");
    for (const auto* invalid : { "OSC_MODES 2 .5 .8 0 .5 .8 0 .5 .8\n", "OSC_MODES 1 1.1 .8 0 .5 .8 0 .5 .8\n",
                                "OSC_MODES 1 .5 .1 0 .5 .8 0 .5 .8\n", "OSC_MODES 1 .5 .8 0 .5 .8\n" })
        check(!decodePatch(encoded.substr(0, modes) + invalid), "Malformed oscillator modes were accepted");
    auto mixed = custom;
    mixed.sound.osc[0].mode = OscillatorMode::pluckedString;
    mixed.sound.osc[0].brightness = 0.91f;
    mixed.sound.osc[0].stringDecay = minStringDecay;
    mixed.sound.osc[1].brightness = 0.37f;
    mixed.sound.osc[2].mode = OscillatorMode::pluckedString;
    mixed.sound.osc[2].stringDecay = maxStringDecay;
    const auto encodedMixed = encodePatch(mixed);
    const auto decodedMixed = decodePatch(encodedMixed);
    check(decodedMixed && encodePatch(*decodedMixed) == encodedMixed, "Mixed oscillator modes failed round-trip");
    const auto third = encoded.find("OSC_C "), filter = encoded.find("FILTER ");
    const auto oldMatrix = [&](const auto& targets)
    {
        std::ostringstream out;
        out << "MATRIX\n" << std::setprecision(std::numeric_limits<float>::max_digits10);
        for (const auto& row : custom.sound.matrix)
        {
            for (auto target : targets)
                out << row[target].value_or(0.0f) << ' ';
            out << '\n';
        }
        return out.str();
    };
    constexpr std::array fixedTargets { positionA, positionB, pitchA, pitchB, cutoff, resonance, pan };
    const auto fixedMatrix = oldMatrix(fixedTargets);
    const auto thirdOscMatrix = oldMatrix(std::array { positionA, positionB, pitchA, pitchB, cutoff, resonance,
                                                      pan, positionC, pitchC });
    const auto assignableMatrix = oldMatrix(std::array { positionA, positionB, positionC, pitchA, pitchB, pitchC,
                                                       cutoff, resonance, pan });
    const auto legacyHeader = encoded.substr(7, routes - 7);
    const auto legacy4 = decodePatch("MOT11 4" + legacyHeader + thirdOscMatrix);
    check(legacy4 && encodePatch(*legacy4) == encoded, "Version 4 patch failed to migrate");
    const auto legacy5 = decodePatch("MOT11 5" + legacyHeader + "TARGETS 0 1 7 2 3 8 4 5 6\n" + assignableMatrix);
    check(legacy5 && encodePatch(*legacy5) == encoded, "Version 5 patch did not load with both effects bypassed");
    custom.sound.tapeDelay = { true, maxTapeTime, maxTapeFeedback, 0.71f, 0.27f };
    custom.sound.reverb = { true, 0.21f, 0.87f, 0.52f };
    const auto withEffects = encodePatch(custom);
    const auto decodedEffects = decodePatch(withEffects);
    check(decodedEffects && encodePatch(*decodedEffects) == withEffects, "Effect settings failed round-trip");
    auto reassigned = custom;
    for (auto& row : reassigned.sound.matrix)
        std::swap(row[pan], row[ampGain]);
    const auto legacy6 = decodePatch("MOT11 6" + legacyHeader + "TARGETS 0 1 7 2 3 8 4 5 16\n"
                                    + assignableMatrix + withEffects.substr(tape, withEffects.find("ARP ", tape) - tape));
    check(legacy6 && encodePatch(*legacy6) == encodePatch(reassigned), "Reassigned version 6 routes failed to migrate");
    check(!decodePatch(withEffects.substr(0, tape) + "TAPE_DELAY 1 0.32 1 0.8 0.1\nREVERB 1 1 0.9 1\nARP 0 120 0\n"),
          "Out-of-range tape feedback was accepted");
    custom.sound.tapeDelay = {};
    custom.sound.reverb = {};
    auto migrated = custom;
    migrated.sound.osc[2] = Patch {}.osc[2];
    for (auto& row : migrated.sound.matrix)
    {
        row[positionC].reset();
        row[pitchC].reset();
    }
    const auto oldPrefix = encoded.substr(7, third - 7) + encoded.substr(filter, voicing - filter);
    const auto legacy2 = decodePatch("MOT11 2" + oldPrefix + encoded.substr(voicing, routes - voicing) + fixedMatrix);
    check(legacy2 && encodePatch(*legacy2) == encodePatch(migrated), "Version 2 patch failed to migrate");
    migrated.sound.voicing = {};
    const auto legacy1 = decodePatch("MOT11 1" + oldPrefix + fixedMatrix);
    check(legacy1 && encodePatch(*legacy1) == encodePatch(migrated), "Version 1 patch failed to migrate");
    check(!decodePatch(encoded.substr(0, voicing) + "VOICING 2 0.1 5 50 1\n" + encoded.substr(routes)),
          "Out-of-range unison was accepted");
    check(!decodePatch("MOT11 5" + legacyHeader + "TARGETS 0 1 7 2 3 8 4 5 99\n" + assignableMatrix),
          "Out-of-range legacy destination was accepted");
    check(!decodePatch("MOT11 5" + legacyHeader + "TARGETS 0 1 7 2 3 8 4 5 0\n" + assignableMatrix),
          "Duplicate legacy destination was accepted");
    check(!decodePatch(encoded.substr(0, routes) + "ROUTES 2\n0 0 0\n0 0 0.5\n" + encoded.substr(tape)),
          "Duplicate source-destination route was accepted");

    NamedPatch fullMatrix { "Full Matrix", {} };
    for (auto& row : fullMatrix.sound.matrix)
        for (int target = 0; target < destinationCount; ++target)
            row[target] = target % 2 == 0 ? 0.0f : -0.25f;
    const auto decodedFullMatrix = decodePatch(encodePatch(fullMatrix));
    check(decodedFullMatrix && decodedFullMatrix->sound.matrix == fullMatrix.sound.matrix,
          "A full matrix with zero-depth routes failed to round-trip");

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
        pluckedStrings(bank);
        monoPlaying(bank);
        glideAndUnison(bank);
        effects(bank);
        plateDecay();
        arpeggiator(bank);
        patches();
        std::cout << "MO-T 11 headless checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
