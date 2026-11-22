#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace mot
{
namespace
{
constexpr float turn = 6.28318530718f;
constexpr float stereoOffset = 0.037f, stereoSkew = 0.008f, maxDrift = 0.004f;

float blend(float a, float b, float amount) noexcept { return a + (b - a) * amount; }

float onePole(float frequency, float rate) noexcept
{
    return 1.0f - std::exp(-turn * frequency / rate);
}

float fadeMix(float current, float target, float smoothing) noexcept
{
    const float delta = target - current;
    return std::abs(delta) <= 0.00001f ? target : current + delta * smoothing;
}

void advancePhase(float& phase, float step) noexcept
{
    phase += step;
    if (phase >= turn)
        phase -= turn;
}
}

void Effects::DelayLine::prepare(int capacity)
{
    buffer.resize(capacity);
    reset();
}

void Effects::DelayLine::reset() noexcept
{
    // Ignore old samples until overwritten so Panic does not clear buffers on the audio thread.
    head = valid = 0;
}

void Effects::DelayLine::push(float sample) noexcept
{
    const auto capacity = static_cast<int>(buffer.size());
    buffer[head] = sample;
    if (++head == capacity)
        head = 0;
    valid = std::min(valid + 1, capacity);
}

float Effects::DelayLine::read(int offset) const noexcept
{
    if (offset > valid)
        return 0.0f;
    const int index = head - offset;
    return buffer[index < 0 ? index + static_cast<int>(buffer.size()) : index];
}

float Effects::DelayLine::interpolate(float offset) const noexcept
{
    const auto whole = static_cast<int>(offset);
    return blend(read(whole), read(whole + 1), offset - whole);
}

float Effects::DelayLine::readOldest() const noexcept
{
    return valid == static_cast<int>(buffer.size()) ? buffer[head] : 0.0f;
}

void Effects::TapeDelay::prepare(float sampleRate, const TapeDelaySettings& settings)
{
    rate = sampleRate;
    const float maxSeconds = maxTapeTime * (1.0f + stereoOffset) + stereoSkew + maxDrift;
    for (auto& line : lines)
        line.prepare(static_cast<int>(std::ceil(maxSeconds * rate)) + 2);
    delaySmoothing = 1.0f - std::exp(-1.0f / (0.065f * rate));
    controlSmoothing = 1.0f - std::exp(-1.0f / (0.008f * rate));
    wowStep = turn * 0.45f / rate;
    flutterStep = turn * 5.3f / rate;
    bright = onePole(8500.0f, rate);
    dark = onePole(1750.0f, rate);
    mix = settings.enabled ? settings.mix : 0.0f;
    reset();
}

void Effects::TapeDelay::reset() noexcept
{
    for (auto& line : lines)
        line.reset();
    tone = {};
    wow = flutter = 0.0f;
    running = false;
}

void Effects::TapeDelay::process(float& left, float& right, const TapeDelaySettings& settings) noexcept
{
    mix = fadeMix(mix, settings.enabled ? settings.mix : 0.0f, controlSmoothing);
    if (!settings.enabled && mix == 0.0f)
    {
        if (running)
            reset();
        return;
    }

    const float targetDelay = settings.time * rate;
    if (!running)
    {
        delay = targetDelay;
        feedback = settings.feedback;
        wear = settings.wear;
        running = true;
    }
    // Slew the moving tape head and keep it advancing during large time changes.
    delay += std::clamp((targetDelay - delay) * delaySmoothing, -0.7f, 0.7f);
    feedback = blend(feedback, settings.feedback, controlSmoothing);
    wear = blend(wear, settings.wear, controlSmoothing);
    advancePhase(wow, wowStep);
    advancePhase(flutter, flutterStep);
    const float drift = rate * blend(0.00012f, maxDrift, wear)
                        * (0.85f * std::sin(wow) + 0.15f * std::sin(flutter));
    const float tapL = lines[0].interpolate(std::max(1.0f, delay + drift));
    const float tapR = lines[1].interpolate(std::max(1.0f, delay * (1.0f + stereoOffset) + rate * stereoSkew - drift));
    const float colour = blend(bright, dark, wear);
    tone[0] = blend(tone[0], tapL, colour);
    tone[1] = blend(tone[1], tapR, colour);
    const auto saturate = [drive = blend(0.8f, 3.5f, wear)](float sample)
    {
        const float driven = sample * drive;
        return sample / std::sqrt(1.0f + driven * driven);
    };
    const float wetL = saturate(tone[0]), wetR = saturate(tone[1]);
    lines[0].push(left + feedback * blend(wetL, wetR, 0.20f));
    lines[1].push(right + feedback * blend(wetR, wetL, 0.20f));
    left = blend(left, wetL, mix);
    right = blend(right, wetR, mix);
}

void Effects::Reverb::Channel::prepare(float rate, int channel)
{
    const auto samples = [rate](float seconds)
    {
        return std::max(1, static_cast<int>(std::round(seconds * rate)));
    };
    constexpr std::array<float, 4> combTimes { 0.0297f, 0.0371f, 0.0411f, 0.0437f };
    constexpr std::array<float, 3> diffusionTimes { 0.0051f, 0.0037f, 0.0017f };
    for (std::size_t i = 0; i < combs.size(); ++i)
        combs[i].prepare(samples(combTimes[i] + channel * 0.00137f));
    for (std::size_t i = 0; i < diffusers.size(); ++i)
        diffusers[i].prepare(samples(diffusionTimes[i] + channel * 0.00031f));
    filtered = {};
}

void Effects::Reverb::Channel::reset() noexcept
{
    for (auto& comb : combs)
        comb.reset();
    for (auto& diffuser : diffusers)
        diffuser.reset();
    filtered = {};
}

float Effects::Reverb::Channel::process(float input, float feedback, float damping) noexcept
{
    // Parallel damped combs build the tail. All-pass stages increase echo density.
    float wet = 0.0f;
    for (std::size_t i = 0; i < combs.size(); ++i)
    {
        const float delayed = combs[i].readOldest();
        filtered[i] = blend(filtered[i], delayed, damping);
        combs[i].push(input + feedback * filtered[i]);
        wet += delayed;
    }
    wet *= 0.25f;
    for (auto& diffuser : diffusers)
    {
        constexpr float diffusion = 0.58f;
        const float delayed = diffuser.readOldest();
        const float next = wet + diffusion * delayed;
        diffuser.push(next);
        wet = delayed - diffusion * next;
    }
    return wet;
}

void Effects::Reverb::prepare(float rate, const ReverbSettings& settings)
{
    for (std::size_t i = 0; i < channels.size(); ++i)
        channels[i].prepare(rate, static_cast<int>(i));
    controlSmoothing = 1.0f - std::exp(-1.0f / (0.008f * rate));
    bright = onePole(12500.0f, rate);
    dark = onePole(900.0f, rate);
    mix = settings.enabled ? settings.mix : 0.0f;
    running = false;
}

void Effects::Reverb::reset() noexcept
{
    for (auto& channel : channels)
        channel.reset();
    running = false;
}

void Effects::Reverb::process(float& left, float& right, const ReverbSettings& settings) noexcept
{
    mix = fadeMix(mix, settings.enabled ? settings.mix : 0.0f, controlSmoothing);
    if (!settings.enabled && mix == 0.0f)
    {
        if (running)
            reset();
        return;
    }

    if (!running)
    {
        size = settings.size;
        damping = settings.damping;
        running = true;
    }
    size = blend(size, settings.size, controlSmoothing);
    damping = blend(damping, settings.damping, controlSmoothing);
    const float feedback = blend(0.52f, 0.93f, size);
    const float colour = blend(bright, dark, damping);
    const float wetL = channels[0].process(blend(left, right, 0.20f), feedback, colour);
    const float wetR = channels[1].process(blend(right, left, 0.20f), feedback, colour);
    left = blend(left, wetL, mix);
    right = blend(right, wetR, mix);
}

void Effects::prepare(float sampleRate, const Patch& patch)
{
    tape.prepare(sampleRate, patch.tapeDelay);
    reverb.prepare(sampleRate, patch.reverb);
}

void Effects::reset() noexcept
{
    tape.reset();
    reverb.reset();
}

void Effects::process(float& left, float& right, const Patch& patch) noexcept
{
    tape.process(left, right, patch.tapeDelay);
    reverb.process(left, right, patch.reverb);
}
}
