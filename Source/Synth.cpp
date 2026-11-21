#include "Synth.h"

#include <algorithm>
#include <cmath>

namespace mot
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float halfPi = pi * 0.5f;

float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
float unit(float x) noexcept { return std::clamp(x, 0.0f, 1.0f); }
float bipolar(float x) noexcept { return std::clamp(x, -1.0f, 1.0f); }

struct Harmonic { float sine, cosine; };

Harmonic spectrum(int table, int harmonic, float position)
{
    const auto h = static_cast<float>(harmonic);
    const auto odd = harmonic % 2 != 0;
    const auto fundamental = harmonic == 1 ? 1.0f : 0.0f;
    switch (table)
    {
        case 0:
        {
            const float triangle = odd ? ((harmonic % 4 == 1 ? 1.0f : -1.0f) / (h * h)) : 0.0f;
            const float saw = (odd ? 1.0f : -1.0f) * 0.80f / h;
            const float square = odd ? 0.88f / h : 0.0f;
            const float segment = position * 3.0f;
            return { segment < 1 ? lerp(fundamental, triangle, segment)
                                 : segment < 2 ? lerp(triangle, saw, segment - 1)
                                               : lerp(saw, square, segment - 2), 0.0f };
        }
        case 1:
        {
            const float width = lerp(0.50f, 0.07f, position);
            return { 2.0f * std::sin(pi * h * width) / (pi * h), 0.0f };
        }
        case 2:
            return { fundamental * 0.56f + (odd ? 0.65f * std::sin(h * lerp(0.38f, 1.3f, position))
                                                       / std::pow(h, lerp(1.9f, 0.83f, position)) : 0.0f), 0.0f };
        case 3:
            return { h <= 16.0f ? std::exp(-0.36f * std::abs(h - lerp(1.0f, 12.0f, position)))
                                     / std::sqrt(h) + fundamental * 0.24f : 0.0f, 0.0f };
        case 4:
        {
            const float band = (h - lerp(1.0f, 15.0f, position)) / lerp(1.0f, 6.7f, position);
            return { odd ? (harmonic % 4 == 1 ? 1.0f : -1.0f) * std::exp(-band * band) / std::sqrt(h)
                               + fundamental * 0.20f : 0.0f, 0.0f };
        }
        default:
        {
            const float first = (h - lerp(2.1f, 11.0f, position)) / 1.8f;
            const float second = (h - lerp(13.0f, 29.0f, position)) / 3.1f;
            const float peak = (std::exp(-first * first) + 0.58f * std::exp(-second * second)) / std::sqrt(h);
            return { 0.31f * fundamental + peak * std::cos(1.17f * h), peak * std::sin(1.17f * h) };
        }
    }
}

float lfoSample(Shape shape, float phase) noexcept
{
    switch (shape)
    {
        case Shape::sine:     return std::sin(2.0f * pi * phase);
        case Shape::triangle: return 1.0f - 4.0f * std::abs(phase - 0.5f);
        case Shape::ramp:     return 2.0f * phase - 1.0f;
        case Shape::square:   return phase < 0.5f ? 1.0f : -1.0f;
    }
    return 0.0f;
}
}

WaveBank::WaveBank() : data(tableCount * frames * bands * (samples + 1))
{
    std::array<float, samples> sine {}, cosine {};
    for (int i = 0; i < samples; ++i)
    {
        const float angle = 2.0f * pi * static_cast<float>(i) / samples;
        sine[i] = std::sin(angle);
        cosine[i] = std::cos(angle);
    }

    for (int table = 0; table < tableCount; ++table)
        for (int index = 0; index < frames; ++index)
        {
            std::array<float, samples> sum {};
            int added = 0;
            float peak = 0.0f;
            for (int band = bands - 1; band >= 0; --band)
            {
                const int limit = 1 << (bands - 1 - band);
                for (int h = added + 1; h <= limit; ++h)
                {
                    const Harmonic weight = spectrum(table, h, static_cast<float>(index) / (frames - 1));
                    if (weight.sine == 0.0f && weight.cosine == 0.0f)
                        continue;
                    for (int i = 0; i < samples; ++i)
                    {
                        const int t = (i * h) & (samples - 1);
                        sum[i] += weight.sine * sine[t] + weight.cosine * cosine[t];
                    }
                }
                added = limit;
                auto* target = data.data() + ((table * frames + index) * bands + band) * (samples + 1);
                std::copy(sum.begin(), sum.end(), target);
                target[samples] = target[0];
                for (float value : sum)
                    peak = std::max(peak, std::abs(value));
            }
            const float scale = 0.92f / std::max(peak, 0.01f);
            auto* begin = data.data() + (table * frames + index) * bands * (samples + 1);
            for (auto* value = begin; value != begin + bands * (samples + 1); ++value)
                *value *= scale;
        }
}

const float* WaveBank::frame(int table, int index, int band) const noexcept
{
    return data.data() + ((table * frames + index) * bands + band) * (samples + 1);
}

float WaveBank::interpolate(int table, float position, float phase, int band) const noexcept
{
    const float i = unit(position) * (frames - 1);
    const int index = std::min(static_cast<int>(i), frames - 2);
    const float p = (phase - std::floor(phase)) * samples;
    const int sample = std::min(static_cast<int>(p), samples - 1);
    const auto* a = frame(table, index, band);
    const auto* b = frame(table, index + 1, band);
    return lerp(lerp(a[sample], a[sample + 1], p - sample),
                lerp(b[sample], b[sample + 1], p - sample), i - index);
}

float WaveBank::display(int table, float position, float phase) const noexcept
{
    return interpolate(table, position, phase, 0);
}

float WaveBank::sample(int table, float position, float phase, float frequency, float sampleRate) const noexcept
{
    const float ceiling = 0.46f * sampleRate;
    if (frequency >= ceiling)
        return 0.0f;
    const float mip = std::clamp(1.0f + std::log2(frequency * (1 << (bands - 1)) / ceiling),
                                 0.0f, static_cast<float>(bands - 1));
    const auto band = static_cast<int>(mip);
    const float a = interpolate(table, position, phase, band);
    if (band == bands - 1)
        return a * unit((ceiling - frequency) / (ceiling * 0.15f));
    return lerp(a, interpolate(table, position, phase, band + 1), mip - band);
}

void Synth::Env::noteOff() noexcept
{
    if (stage != idle && stage != release)
    {
        releaseStart = value;
        stage = release;
    }
}

float Synth::Env::tick(const Envelope& e, float dt) noexcept
{
    switch (stage)
    {
        case idle: break;
        case attack:
            value += dt / e.attack;
            if (value >= 1.0f) { value = 1.0f; stage = decay; }
            break;
        case decay:
            if (value > e.sustain)
                value = std::max(e.sustain, value - dt * (1.0f - e.sustain) / e.decay);
            if (value <= e.sustain)
                stage = sustain;
            break;
        case sustain:
            value = lerp(value, e.sustain, std::min(1.0f, dt * 240.0f));
            break;
        case release:
            value -= dt * releaseStart / e.release;
            if (value <= 1.0e-6f) { value = 0.0f; stage = idle; }
            break;
    }
    return value;
}

float Synth::Filter::tick(float input, float a1, float a2, float a3, float k, FilterMode mode) noexcept
{
    const float v3 = input - i2;
    const float band = a1 * i1 + a2 * v3;
    const float low = i2 + a2 * i1 + a3 * v3;
    i1 = 2.0f * band - i1;
    i2 = 2.0f * low - i2;
    switch (mode)
    {
        case FilterMode::bandPass: return band;
        case FilterMode::highPass: return input - k * band - low;
        default:                   return low;
    }
}

Synth::Synth(const WaveBank& bank) : waveBank(bank) { prepare(48000); }

void Synth::prepare(double newRate) noexcept
{
    rate = static_cast<float>(std::max(1.0, newRate));
    seconds = 1.0f / rate;
    smoothing = 1.0f - std::exp(-seconds / 0.006f);
    tableStep = seconds / 0.012f;
    voices = {};
    globalPhases = {};
    age = 0;
    controlStep = 0;
    tailL = tailR = 0;
    outputGain = patch.volume;
    drive = patch.drive;
}

void Synth::setPatch(const Patch& newPatch) noexcept
{
    patch = newPatch;
    routeCount = 0;
    for (int from = 0; from < sourceCount; ++from)
        for (int to = 0; to < destinationCount; ++to)
            if (patch.matrix[from][to] != 0.0f)
                routes[routeCount++] = { from, to, patch.matrix[from][to] };
    controlStep = 0;
}

void Synth::advance(float& phase, float increment) const noexcept
{
    phase += increment;
    phase -= std::floor(phase);
}

void Synth::retire(Voice& v) noexcept
{
    if (v.amp.stage != Env::idle)
    {
        tailL += v.lastL;
        tailR += v.lastR;
        v = {};
    }
}

void Synth::startVoice(const Event& e) noexcept
{
    Voice* selected = nullptr;
    for (auto& v : voices)
        if (v.amp.stage != Env::idle && v.port == e.port && v.channel == e.channel && v.note == e.note)
        {
            selected = &v;
            break;
        }

    if (selected == nullptr)
    {
        selected = &*std::min_element(voices.begin(), voices.end(), [](const Voice& a, const Voice& b)
        {
            const auto priority = [](const Voice& v)
            {
                if (v.amp.stage == Env::idle) return 0;
                if (v.amp.stage == Env::release) return 1;
                return v.held ? 3 : 2;
            };
            return priority(a) == priority(b) ? a.age < b.age : priority(a) < priority(b);
        });
    }

    retire(*selected);
    auto& v = *selected;
    v = {};
    v.port = e.port;
    v.channel = e.channel;
    v.note = e.note;
    v.noteFrequency = 440.0f * std::exp2((e.note - 69) / 12.0f);
    v.velocity = unit(e.velocity);
    v.controls = e.controls;
    v.held = true;
    v.age = ++age;
    v.amp.stage = v.filter.stage = Env::attack;
    for (std::size_t i = 0; i < v.phases.size(); ++i)
    {
        const auto& osc = patch.osc[i];
        const auto seed = static_cast<std::uint32_t>(v.age) * 2654435761u + static_cast<std::uint32_t>(i) * 2246822519u;
        v.phases[i] = osc.resetPhase ? osc.phase : static_cast<float>(seed >> 8) / 16777216.0f;
        v.table[i] = v.previousTable[i] = osc.table;
    }
}

void Synth::event(const Event& e) noexcept
{
    if (e.type == EventType::panic)
    {
        for (auto& v : voices) retire(v);
        return;
    }
    if (e.type == EventType::noteOn && e.velocity > 0.0f)
    {
        startVoice(e);
        return;
    }
    for (auto& v : voices)
    {
        if (v.amp.stage == Env::idle || v.port != e.port)
            continue;
        if (e.type == EventType::disconnect)
        {
            retire(v);
            continue;
        }
        if (v.channel != e.channel)
            continue;
        if (e.type == EventType::allSoundOff)
        {
            retire(v);
            continue;
        }
        if (e.type == EventType::expression)
            v.controls = e.controls;
        if (e.type == EventType::allNotesOff ||
            ((e.type == EventType::noteOff || e.type == EventType::noteOn) && v.note == e.note))
            v.held = false;
        if (!v.held && !v.controls.sustain)
        {
            v.amp.noteOff();
            v.filter.noteOff();
        }
    }
}

void Synth::updateVoice(Voice& v) noexcept
{
    const auto lfo = [&](std::size_t index)
    {
        const auto& settings = patch.lfos[index];
        return lfoSample(settings.shape, settings.restart ? v.lfoPhases[index] : globalPhases[index]) * settings.depth;
    };
    const std::array<float, sourceCount> values {
        lfo(0), lfo(1), v.filter.value, v.amp.value, v.velocity,
        bipolar((v.note - 60) / 48.0f), v.controls.wheel, v.controls.pressure
    };
    std::array<float, destinationCount> mod {};
    for (std::size_t i = 0; i < routeCount; ++i)
    {
        const auto& route = routes[i];
        mod[route.target] += values[route.source] * route.amount;
    }

    for (int i = 0; i < 2; ++i)
    {
        const auto& osc = patch.osc[i];
        const float tune = 12.0f * osc.octave + osc.semitone + osc.fine * 0.01f
                           + 2.0f * v.controls.bend + 12.0f * bipolar(mod[pitchA + i]);
        v.targetFrequency[i] = v.noteFrequency * std::exp2(tune / 12.0f);
        v.targetPosition[i] = unit(osc.position + mod[positionA + i]);
        const float angle = (bipolar(osc.pan + mod[pan]) + 1.0f) * 0.5f * halfPi;
        v.targetGainL[i] = osc.level * std::cos(angle);
        v.targetGainR[i] = osc.level * std::sin(angle);
        if (v.table[i] != osc.table)
        {
            v.previousTable[i] = v.table[i];
            v.table[i] = osc.table;
            v.tableMix[i] = 0.0f;
        }
    }

    const float hz = std::clamp(patch.cutoff * std::exp2(4.0f * bipolar(mod[cutoff])), 15.0f, rate * 0.43f);
    v.targetG = std::tan(pi * hz * seconds);
    v.targetDamping = lerp(1.75f, 0.21f, unit(patch.resonance + mod[resonance]));
    if (v.fresh)
    {
        v.frequency = v.targetFrequency;
        v.position = v.targetPosition;
        v.gainL = v.targetGainL;
        v.gainR = v.targetGainR;
        v.g = v.targetG;
        v.damping = v.targetDamping;
        v.fresh = false;
    }
}

void Synth::render(float* left, float* right, int samples, const Event* events, std::size_t count) noexcept
{
    std::size_t next = 0;
    for (int sample = 0; sample < samples; ++sample)
    {
        while (next < count && events[next].offset <= sample)
            event(events[next++]);

        const bool update = controlStep == 0;
        controlStep = (controlStep + 1) % 8;
        outputGain = lerp(outputGain, patch.volume, smoothing);
        drive = lerp(drive, patch.drive, smoothing);
        for (std::size_t i = 0; i < globalPhases.size(); ++i)
            advance(globalPhases[i], patch.lfos[i].rate * seconds);

        float mixL = tailL, mixR = tailR;
        const float tailDecay = 1.0f - std::min(1.0f, seconds * 900.0f);
        tailL = std::abs(tailL) < 1.0e-8f ? 0.0f : tailL * tailDecay;
        tailR = std::abs(tailR) < 1.0e-8f ? 0.0f : tailR * tailDecay;

        for (auto& v : voices)
        {
            if (v.amp.stage == Env::idle)
                continue;
            const float amp = v.amp.tick(patch.ampEnv, seconds);
            v.filter.tick(patch.filterEnv, seconds);
            if (update || v.fresh)
                updateVoice(v);
            for (std::size_t i = 0; i < v.lfoPhases.size(); ++i)
                advance(v.lfoPhases[i], patch.lfos[i].rate * seconds);

            float voiceL = 0.0f, voiceR = 0.0f;
            for (int i = 0; i < 2; ++i)
            {
                v.frequency[i] = lerp(v.frequency[i], v.targetFrequency[i], smoothing);
                v.position[i] = lerp(v.position[i], v.targetPosition[i], smoothing);
                v.gainL[i] = lerp(v.gainL[i], v.targetGainL[i], smoothing);
                v.gainR[i] = lerp(v.gainR[i], v.targetGainR[i], smoothing);
                if (v.gainL[i] + v.gainR[i] > 0.00001f)
                {
                    float value = waveBank.sample(v.table[i], v.position[i], v.phases[i], v.frequency[i], rate);
                    if (v.tableMix[i] < 1.0f)
                    {
                        const float previous = waveBank.sample(v.previousTable[i], v.position[i], v.phases[i], v.frequency[i], rate);
                        value = lerp(previous, value, v.tableMix[i]);
                    }
                    voiceL += value * v.gainL[i];
                    voiceR += value * v.gainR[i];
                }
                v.tableMix[i] = unit(v.tableMix[i] + tableStep);
                advance(v.phases[i], v.frequency[i] * seconds);
            }
            v.g = lerp(v.g, v.targetG, smoothing);
            v.damping = lerp(v.damping, v.targetDamping, smoothing);
            const float a1 = 1.0f / (1.0f + v.g * (v.g + v.damping));
            const float a2 = v.g * a1;
            const float a3 = v.g * a2;
            const auto saturate = [&](float value)
            {
                return lerp(value, std::tanh(value * (1.0f + 6.0f * drive)), drive);
            };
            const float amplitude = amp * v.velocity * 0.23f;
            v.lastL = v.filters[0].tick(saturate(voiceL), a1, a2, a3, v.damping, patch.filterMode) * amplitude;
            v.lastR = v.filters[1].tick(saturate(voiceR), a1, a2, a3, v.damping, patch.filterMode) * amplitude;
            mixL += v.lastL;
            mixR += v.lastR;
        }

        const auto limit = [&](float value)
        {
            const float x = outputGain * value;
            return x / std::sqrt(1.0f + x * x);
        };
        if (right != nullptr)
        {
            left[sample] = limit(mixL);
            right[sample] = limit(mixR);
        }
        else if (left != nullptr)
        {
            left[sample] = limit((mixL + mixR) * 0.5f);
        }
    }
}

int Synth::activeVoices() const noexcept
{
    return static_cast<int>(std::count_if(voices.begin(), voices.end(), [](const Voice& v)
    {
        return v.amp.stage != Env::idle;
    }));
}
}
