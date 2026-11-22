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
        float allPass(float input, float delayed, float coefficient) noexcept;

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

    struct PlateReverb
    {
        struct Tank
        {
            void prepare(float sampleRate, std::size_t side);
            void reset() noexcept;
            void process(float input, float damping) noexcept;
            float pickup(const Tank& opposite, const std::array<int, 7>& taps) const noexcept;

            DelayLine modulatedDiffuser, firstDelay, secondDiffuser, lastDelay;
            float filtered = 0, phase = 0, phaseStep = 0, centre = 0, excursion = 0;
        };

        void prepare(float sampleRate, const ReverbSettings&);
        void reset() noexcept;
        void process(float& left, float& right, const ReverbSettings&) noexcept;

        std::array<DelayLine, 4> inputDiffusers;
        std::array<Tank, 2> tanks;
        std::array<std::array<int, 7>, 2> outputTaps {};
        float mix = 0, damping = 0, feedback = 0, targetFeedback = 0, decaySetting = -1;
        float decayExponent = 0, controlSmoothing = 0, bandwidth = 0, inputTone = 0, dark = 0;
        bool running = false;
    };

    TapeDelay tape;
    PlateReverb reverb;
};
}
