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
    static constexpr int frameStride = bands * (samples + 1);
    const float* frame(int table, int index, int band) const noexcept;
    float interpolateFixed(int table, float phase, int band) const noexcept;
    float interpolate(int table, float position, float phase, int band) const noexcept;
    std::array<int, tableCount> tableOffsets {};
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
        struct Layer
        {
            float phase = 0, ratio = 1, targetRatio = 1;
            float gainL = 0, gainR = 0, targetGainL = 0, targetGainR = 0;
        };

        std::uint32_t port = 0;
        int channel = 1, note = 60;
        std::uint64_t age = 0;
        bool held = false, fresh = true;
        float velocity = 1.0f, velocityGain = 1.0f;
        double noteFrequency = 440.0, targetNoteFrequency = 440.0, glideRatio = 1.0;
        int glideSamples = 0;
        Controllers controls {};
        Env amp, filter;
        std::array<Filter, 2> filters {};
        std::array<std::array<Layer, maxUnison>, oscillatorCount> layers {};
        std::array<float, 2> lfoPhases {};
        std::array<float, oscillatorCount> tuning {}, position {}, targetTuning {}, targetPosition {};
        std::array<int, oscillatorCount> table {}, previousTable {};
        std::array<float, oscillatorCount> tableMix {};
        float g = 0, damping = 0, targetG = 0, targetDamping = 0;
        float driveMod = 0, targetDriveMod = 0, ampGain = 1, targetAmpGain = 1;
        float lastL = 0, lastR = 0;
    };

    struct HeldNote { Event on; bool held; };
    struct Route { int source; Destination target; float amount; };
    void event(const Event&) noexcept;
    void trackNotes(const Event&) noexcept;
    const HeldNote* monoNote() const noexcept;
    void updateMono(bool retrigger) noexcept;
    void changeVoiceMode(PlayMode previous) noexcept;
    void setVoiceNote(Voice&, const Event&, double glideFrom) noexcept;
    void trigger(Voice&, const Event&, double glideFrom = 0.0) noexcept;
    void startVoice(const Event&, bool held = true) noexcept;
    void updateVoice(Voice&) noexcept;
    void advance(float& phase, float increment) const noexcept;
    void retire(Voice&) noexcept;

    const WaveBank& waveBank;
    Patch patch;
    std::array<Voice, polyphony> voices {};
    std::array<HeldNote, 256> heldNotes {};
    std::size_t heldCount = 0;
    std::array<Route, sourceCount * matrixColumnCount> routes {};
    std::size_t routeCount = 0;
    std::array<float, maxUnison> unisonPan {}, unisonRatio { 1.0f };
    std::array<float, 2> globalPhases {};
    std::uint64_t age = 0;
    float rate = 48000.0f, seconds = 1.0f / 48000.0f;
    float smoothing = 0, tableStep = 0, tailL = 0, tailR = 0;
    float outputGain = 0, drive = 0;
    int controlStep = 0;
};
}
