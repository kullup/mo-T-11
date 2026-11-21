#pragma once

#include "Patch.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mot
{
class WaveBank
{
public:
    static constexpr int frames = 16;
    WaveBank();
    float display(int table, float position, float phase) const noexcept;
    float sample(int table, float position, float phase, float frequency, float sampleRate) const noexcept;

private:
    static constexpr int samples = 1024;
    static constexpr int bands = 9;
    const float* frame(int table, int index, int band) const noexcept;
    float interpolate(int table, float position, float phase, int band) const noexcept;
    std::vector<float> data;
};

struct Controllers { float bend = 0, wheel = 0, pressure = 0; bool sustain = false; };

enum class EventType { noteOn, noteOff, expression, allNotesOff, allSoundOff, disconnect, panic };

// A port supplies its current channel expression with note-on and controller events.
// Event timestamps are sample offsets within the block passed to render().
struct Event
{
    EventType type = EventType::noteOn;
    std::uint32_t port = 0;
    int channel = 1, note = 60;
    float velocity = 1.0f;
    Controllers controls {};
    int offset = 0;
};

class Synth
{
public:
    static constexpr int polyphony = 16;
    explicit Synth(const WaveBank& bank);
    void prepare(double sampleRate) noexcept;
    void setPatch(const Patch& patch) noexcept;
    void render(float* left, float* right, int samples, const Event* events, std::size_t count) noexcept;
    int activeVoices() const noexcept;

private:
    struct Env
    {
        enum Stage { idle, attack, decay, sustain, release } stage = idle;
        float value = 0, releaseStart = 0;
        void noteOff() noexcept;
        float tick(const Envelope&, float seconds) noexcept;
    };

    struct Filter
    {
        float i1 = 0, i2 = 0;
        float tick(float input, float a1, float a2, float a3, float damping, FilterMode) noexcept;
    };

    struct Voice
    {
        std::uint32_t port = 0;
        int channel = 1, note = 60;
        std::uint64_t age = 0;
        bool held = false, fresh = true;
        float velocity = 1.0f, noteFrequency = 440.0f;
        Controllers controls {};
        Env amp, filter;
        std::array<Filter, 2> filters {};
        std::array<float, 2> phases {}, lfoPhases {};
        std::array<float, 2> frequency {}, position {}, gainL {}, gainR {};
        std::array<float, 2> targetFrequency {}, targetPosition {}, targetGainL {}, targetGainR {};
        std::array<int, 2> table {}, previousTable {};
        std::array<float, 2> tableMix { 1.0f, 1.0f };
        float g = 0, damping = 0, targetG = 0, targetDamping = 0;
        float lastL = 0, lastR = 0;
    };

    struct Route { int source, target; float amount; };
    void event(const Event&) noexcept;
    void startVoice(const Event&) noexcept;
    void updateVoice(Voice&) noexcept;
    void advance(float& phase, float increment) const noexcept;
    void retire(Voice&) noexcept;

    const WaveBank& waveBank;
    Patch patch;
    std::array<Voice, polyphony> voices {};
    std::array<Route, sourceCount * destinationCount> routes {};
    std::size_t routeCount = 0;
    std::array<float, 2> globalPhases {};
    std::uint64_t age = 0;
    float rate = 48000.0f, seconds = 1.0f / 48000.0f;
    float smoothing = 0, tableStep = 0, tailL = 0, tailR = 0;
    float outputGain = 0, drive = 0;
    int controlStep = 0;
};
}
