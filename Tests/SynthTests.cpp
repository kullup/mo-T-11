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
    const auto encoded = encodePatch(custom);
    const auto decoded = decodePatch(encoded);
    check(decoded && encodePatch(*decoded) == encoded, "Custom patch failed round-trip");
    check(!decodePatch(encoded.substr(0, encoded.size() / 2)), "Truncated patch was accepted");
    check(!decodePatch("MOT11 2" + encoded.substr(7)), "Unknown patch version was accepted");
    check(!decodePatch(encoded + "junk"), "Unexpected trailing content was accepted");
}
}

int main()
{
    try
    {
        const WaveBank bank;
        notes(bank);
        modulationAndPolyphony(bank);
        patches();
        std::cout << "MO-T 11 headless checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
