#pragma once

#include "Patch.h"

#include <vector>

namespace mot
{
class Effects
{
public:
    void prepare(float sampleRate, const Patch& patch);
    void reset() noexcept;
    void process(float& left, float& right, const Patch& patch) noexcept;

private:
    struct DelayLine
    {
        void prepare(int capacity);
        void reset() noexcept;
        void push(float sample) noexcept;
        float read(int offset) const noexcept;
        float interpolate(float offset) const noexcept;
        float readOldest() const noexcept;

        std::vector<float> buffer;
        int head = 0, valid = 0;
    };

    struct TapeDelay
    {
        void prepare(float sampleRate, const TapeDelaySettings&);
        void reset() noexcept;
        void process(float& left, float& right, const TapeDelaySettings&) noexcept;

        std::array<DelayLine, 2> lines;
        std::array<float, 2> tone {};
        float rate = 48000.0f, delay = 0, mix = 0, feedback = 0, wear = 0;
        float wow = 0, flutter = 0, wowStep = 0, flutterStep = 0, bright = 0, dark = 0;
        float delaySmoothing = 0, controlSmoothing = 0;
        bool running = false;
    };

    struct Reverb
    {
        struct Channel
        {
            void prepare(float sampleRate, int channel);
            void reset() noexcept;
            float process(float input, float feedback, float damping) noexcept;

            std::array<DelayLine, 4> combs;
            std::array<float, 4> filtered {};
            std::array<DelayLine, 3> diffusers;
        };

        void prepare(float sampleRate, const ReverbSettings&);
        void reset() noexcept;
        void process(float& left, float& right, const ReverbSettings&) noexcept;

        std::array<Channel, 2> channels;
        float mix = 0, size = 0, damping = 0, controlSmoothing = 0, bright = 0, dark = 0;
        bool running = false;
    };

    TapeDelay tape;
    Reverb reverb;
};
}
