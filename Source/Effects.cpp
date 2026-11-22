#include "Effects.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace mot
{
namespace
{
constexpr float turn = 6.28318530718f;
constexpr float stereoOffset = 0.037f, stereoSkew = 0.008f, maxDrift = 0.004f;

// Plate topology and nominal delay lengths from Dattorro, Effect Design, Part 1, Fig. 1 and Table 2.
constexpr float plateReferenceRate = 29761.0f;
constexpr std::array<int, 4> plateInputLengths { 142, 107, 379, 277 };
constexpr std::array<std::array<int, 4>, 2> plateTankLengths {{
    { 672, 4453, 1800, 3720 }, { 908, 4217, 2656, 3163 }
}};
constexpr std::array<std::array<int, 7>, 2> platePickupOffsets {{
    { 266, 2974, 1913, 1996, 1990, 187, 1066 }, { 353, 3627, 1228, 2673, 2111, 335, 121 }
}};

int plateSamples(int referenceSamples, float rate) noexcept
{
    return std::max(1, static_cast<int>(std::round(referenceSamples * rate / plateReferenceRate)));
}

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

float Effects::DelayLine::allPass(float input, float delayed, float coefficient) noexcept
{
    const float next = input + coefficient * delayed;
    push(next);
    return delayed - coefficient * next;
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

void Effects::PlateReverb::Tank::prepare(float rate, std::size_t side)
{
    const auto& lengths = plateTankLengths[side];
    centre = static_cast<float>(plateSamples(lengths[0], rate));
    excursion = 0.0004f * rate;
    modulatedDiffuser.prepare(static_cast<int>(std::ceil(centre + excursion)) + 2);
    firstDelay.prepare(plateSamples(lengths[1], rate));
    secondDiffuser.prepare(plateSamples(lengths[2], rate));
    lastDelay.prepare(plateSamples(lengths[3], rate));
    phaseStep = turn * (side == 0 ? 0.61f : 0.87f) / rate;
}

void Effects::PlateReverb::Tank::reset() noexcept
{
    modulatedDiffuser.reset();
    firstDelay.reset();
    secondDiffuser.reset();
    lastDelay.reset();
    filtered = phase = 0.0f;
}

void Effects::PlateReverb::Tank::process(float input, float damping) noexcept
{
    advancePhase(phase, phaseStep);
    const float offset = std::max(1.0f, centre + excursion * std::sin(phase));
    const float diffused = modulatedDiffuser.allPass(input, modulatedDiffuser.interpolate(offset), -0.70f);
    filtered = blend(filtered, firstDelay.readOldest(), damping);
    firstDelay.push(diffused);
    lastDelay.push(secondDiffuser.allPass(filtered, secondDiffuser.readOldest(), 0.50f));
}

float Effects::PlateReverb::Tank::pickup(const Tank& opposite, const std::array<int, 7>& taps) const noexcept
{
    // Each virtual pickup combines reflections from different points in both halves of the plate.
    return firstDelay.read(taps[0]) + firstDelay.read(taps[1]) - secondDiffuser.read(taps[2]) + lastDelay.read(taps[3])
           - opposite.firstDelay.read(taps[4]) - opposite.secondDiffuser.read(taps[5]) - opposite.lastDelay.read(taps[6]);
}

void Effects::PlateReverb::prepare(float rate, const ReverbSettings& settings)
{
    for (std::size_t i = 0; i < inputDiffusers.size(); ++i)
        inputDiffusers[i].prepare(plateSamples(plateInputLengths[i], rate));
    int pathSamples = 0;
    for (std::size_t i = 0; i < tanks.size(); ++i)
    {
        tanks[i].prepare(rate, i);
        const auto& lengths = plateTankLengths[i];
        pathSamples += std::accumulate(lengths.begin(), lengths.end(), 0);
        for (std::size_t tap = 0; tap < outputTaps[i].size(); ++tap)
            outputTaps[i][tap] = plateSamples(platePickupOffsets[i][tap], rate);
    }
    // The figure-eight has two feedback multipliers around the full circuit.
    const float halfLoopSeconds = pathSamples / (2.0f * plateReferenceRate);
    decayExponent = std::log(0.001f) * halfLoopSeconds;
    decaySetting = -1.0f;
    controlSmoothing = 1.0f - std::exp(-1.0f / (0.008f * rate));
    bandwidth = onePole(12000.0f, rate);
    dark = onePole(1000.0f, rate);
    mix = settings.enabled ? settings.mix : 0.0f;
    reset();
}

void Effects::PlateReverb::reset() noexcept
{
    for (auto& diffuser : inputDiffusers)
        diffuser.reset();
    for (auto& tank : tanks)
        tank.reset();
    inputTone = 0.0f;
    running = false;
}

void Effects::PlateReverb::process(float& left, float& right, const ReverbSettings& settings) noexcept
{
    mix = fadeMix(mix, settings.enabled ? settings.mix : 0.0f, controlSmoothing);
    if (!settings.enabled && mix == 0.0f)
    {
        if (running)
            reset();
        return;
    }

    if (settings.decay != decaySetting)
    {
        decaySetting = settings.decay;
        targetFeedback = std::exp(decayExponent / plateDecaySeconds(decaySetting));
    }
    if (!running)
    {
        feedback = targetFeedback;
        damping = settings.damping;
        running = true;
    }
    feedback = blend(feedback, targetFeedback, controlSmoothing);
    damping = blend(damping, settings.damping, controlSmoothing);
    const float colour = blend(1.0f, dark, damping);

    inputTone = blend(inputTone, (left + right) * 0.5f, bandwidth);
    float input = inputTone * 0.55f;
    for (std::size_t i = 0; i < inputDiffusers.size(); ++i)
    {
        auto& diffuser = inputDiffusers[i];
        input = diffuser.allPass(input, diffuser.readOldest(), i < 2 ? 0.75f : 0.625f);
    }

    const float returnedL = tanks[0].lastDelay.readOldest(), returnedR = tanks[1].lastDelay.readOldest();
    tanks[0].process(input + feedback * returnedR, colour);
    tanks[1].process(input + feedback * returnedL, colour);
    const float wetL = 0.42f * tanks[1].pickup(tanks[0], outputTaps[0]);
    const float wetR = 0.42f * tanks[0].pickup(tanks[1], outputTaps[1]);
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
