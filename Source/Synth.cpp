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

int storedFrames(int table) noexcept
{
    if (table == pulseTable) return 0;
    return isFixedTable(table) ? 1 : WaveBank::frames;
}

Harmonic spectrum(int table, int harmonic, float position)
{
    const auto h = static_cast<float>(harmonic);
    const auto odd = harmonic % 2 != 0;
    const auto fundamental = harmonic == 1 ? 1.0f : 0.0f;
    const float triangle = odd ? ((harmonic % 4 == 1 ? 1.0f : -1.0f) / (h * h)) : 0.0f;
    const float saw = (odd ? 1.0f : -1.0f) * 0.80f / h;
    const float square = odd ? 0.88f / h : 0.0f;
    switch (table)
    {
        case basicShapes:
        {
            const float segment = position * 3.0f;
            return { segment < 1 ? lerp(fundamental, triangle, segment)
                                 : segment < 2 ? lerp(triangle, saw, segment - 1)
                                               : lerp(saw, square, segment - 2), 0.0f };
        }
        case pulseWidth:
        {
            const float width = lerp(0.50f, 0.07f, position);
            return { 2.0f * std::sin(pi * h * width) / (pi * h), 0.0f };
        }
        case hollowBody:
            return { fundamental * 0.56f + (odd ? 0.65f * std::sin(h * lerp(0.38f, 1.3f, position))
                                                       / std::pow(h, lerp(1.9f, 0.83f, position)) : 0.0f), 0.0f };
        case harmonicLadder:
            return { h <= 16.0f ? std::exp(-0.36f * std::abs(h - lerp(1.0f, 12.0f, position)))
                                     / std::sqrt(h) + fundamental * 0.24f : 0.0f, 0.0f };
        case foldedSine:
        {
            const float band = (h - lerp(1.0f, 15.0f, position)) / lerp(1.0f, 6.7f, position);
            return { odd ? (harmonic % 4 == 1 ? 1.0f : -1.0f) * std::exp(-band * band) / std::sqrt(h)
                               + fundamental * 0.20f : 0.0f, 0.0f };
        }
        case glassFormants:
        {
            const float first = (h - lerp(2.1f, 11.0f, position)) / 1.8f;
            const float second = (h - lerp(13.0f, 29.0f, position)) / 3.1f;
            const float peak = (std::exp(-first * first) + 0.58f * std::exp(-second * second)) / std::sqrt(h);
            return { 0.31f * fundamental + peak * std::cos(1.17f * h), peak * std::sin(1.17f * h) };
        }
        case sineTable:     return { fundamental, 0.0f };
        case triangleTable: return { triangle, 0.0f };
        case sawTable:      return { saw, 0.0f };
        case squareTable:   return { square, 0.0f };
        default: return {};
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

WaveBank::WaveBank()
{
    int offset = 0;
    for (int table = 0; table < tableCount; ++table)
    {
        tableOffsets[table] = offset;
        offset += storedFrames(table) * frameStride;
    }
    data.resize(offset);

    std::array<float, samples> sine {}, cosine {};
    for (int i = 0; i < samples; ++i)
    {
        const float angle = 2.0f * pi * static_cast<float>(i) / samples;
        sine[i] = std::sin(angle);
        cosine[i] = std::cos(angle);
    }

    for (int table = 0; table < tableCount; ++table)
        for (int index = 0, count = storedFrames(table); index < count; ++index)
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
                auto* target = data.data() + tableOffsets[table] + index * frameStride + band * (samples + 1);
                std::copy(sum.begin(), sum.end(), target);
                target[samples] = target[0];
                for (float value : sum)
                    peak = std::max(peak, std::abs(value));
            }
            const float scale = 0.92f / std::max(peak, 0.01f);
            auto* begin = data.data() + tableOffsets[table] + index * frameStride;
            for (auto* value = begin; value != begin + frameStride; ++value)
                *value *= scale;
        }
}

const float* WaveBank::frame(int table, int index, int band) const noexcept
{
    return data.data() + tableOffsets[table] + index * frameStride + band * (samples + 1);
}

float WaveBank::interpolateFixed(int table, float phase, int band) const noexcept
{
    const float p = (phase - std::floor(phase)) * samples;
    const int sample = std::min(static_cast<int>(p), samples - 1);
    const auto* values = frame(table, 0, band);
    return lerp(values[sample], values[sample + 1], p - sample);
}

float WaveBank::interpolate(int table, float position, float phase, int band) const noexcept
{
    if (isFixedTable(table))
        return interpolateFixed(table, phase, band);
    if (table == pulseTable)
    {
        // Two band-limited saws cancel the ramp and move one edge continuously with duty.
        const float duty = pulseDuty(unit(position));
        const float edge = phase + 0.5f;
        const float high = interpolateFixed(sawTable, edge - duty, band);
        const float low = interpolateFixed(sawTable, edge, band);
        return (high - low) * (0.5f / std::max(duty, 1.0f - duty));
    }
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
    heldCount = 0;
    globalPhases = {};
    age = 0;
    controlStep = 0;
    tailL = tailR = 0;
    outputGain = patch.volume;
    drive = patch.drive;
}

void Synth::setPatch(const Patch& newPatch) noexcept
{
    const auto previousMode = patch.voicing.mode;
    patch = newPatch;
    const int count = std::clamp(patch.voicing.unison, 1, maxUnison);
    patch.voicing.unison = count;
    for (int i = 0; i < count; ++i)
    {
        const float spread = count == 1 ? 0.0f : 2.0f * i / (count - 1) - 1.0f;
        unisonPan[i] = spread * patch.voicing.width;
        unisonRatio[i] = std::exp2(spread * patch.voicing.detune / 1200.0f);
    }
    routeCount = 0;
    for (int from = 0; from < sourceCount; ++from)
        for (int col = 0; col < matrixColumnCount; ++col)
            if (patch.matrix[from][col] != 0.0f)
                routes[routeCount++] = { from, patch.matrixTargets[col], patch.matrix[from][col] };
    changeVoiceMode(previousMode);
    if (patch.voicing.glide <= 0.0f)
        for (auto& v : voices)
            if (v.glideSamples > 0)
            {
                v.noteFrequency = v.targetNoteFrequency;
                v.glideSamples = 0;
            }
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

void Synth::startVoice(const Event& e, bool held) noexcept
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

    trigger(*selected, e);
    selected->held = held;
}

void Synth::setVoiceNote(Voice& v, const Event& e, double glideFrom) noexcept
{
    v.port = e.port;
    v.channel = e.channel;
    v.note = e.note;
    v.targetNoteFrequency = 440.0 * std::exp2((e.note - 69) / 12.0);
    v.glideSamples = glideFrom > 0.0 ? static_cast<int>(patch.voicing.glide * rate) : 0;
    v.noteFrequency = v.targetNoteFrequency;
    if (v.glideSamples > 0)
    {
        v.noteFrequency = glideFrom;
        v.glideRatio = std::pow(v.targetNoteFrequency / glideFrom, 1.0 / v.glideSamples);
    }
    v.velocity = unit(e.velocity);
    v.controls = e.controls;
    v.held = true;
    v.age = ++age;
}

void Synth::trigger(Voice& v, const Event& e, double glideFrom) noexcept
{
    retire(v);
    v = {};
    setVoiceNote(v, e, glideFrom);
    v.velocityGain = v.velocity;
    v.amp.stage = v.filter.stage = Env::attack;
    for (std::size_t i = 0; i < v.layers.size(); ++i)
    {
        const auto& osc = patch.osc[i];
        const auto seed = static_cast<std::uint32_t>(v.age) * 2654435761u + static_cast<std::uint32_t>(i) * 2246822519u;
        for (int layer = 0; layer < maxUnison; ++layer)
        {
            const auto layerSeed = seed + static_cast<std::uint32_t>(layer) * 1597334677u;
            v.layers[i][layer].phase = osc.resetPhase ? osc.phase + 0.11f * layer
                                                     : static_cast<float>(layerSeed >> 8) / 16777216.0f;
        }
        v.table[i] = v.previousTable[i] = osc.table;
        v.tableMix[i] = 1.0f;
    }
}

void Synth::trackNotes(const Event& e) noexcept
{
    const bool pressed = e.type == EventType::noteOn && e.velocity > 0.0f;
    const bool released = e.type == EventType::noteOff || (e.type == EventType::noteOn && !pressed);
    std::size_t remaining = 0;
    for (std::size_t i = 0; i < heldCount; ++i)
    {
        auto& entry = heldNotes[i];
        auto& on = entry.on;
        const bool samePort = on.port == e.port;
        if (samePort && e.type == EventType::disconnect)
            continue;
        if (samePort && on.channel == e.channel)
        {
            if (e.type == EventType::allSoundOff || (pressed && on.note == e.note))
                continue;
            if (e.type == EventType::expression)
                on.controls = e.controls;
            if (e.type == EventType::allNotesOff || (released && on.note == e.note))
                entry.held = false;
            if (!entry.held && !on.controls.sustain)
                continue;
        }
        heldNotes[remaining++] = entry;
    }
    heldCount = remaining;
    if (pressed)
    {
        if (heldCount == heldNotes.size())
        {
            std::move(heldNotes.begin() + 1, heldNotes.end(), heldNotes.begin());
            --heldCount;
        }
        heldNotes[heldCount++] = { e, true };
    }
}

const Synth::HeldNote* Synth::monoNote() const noexcept
{
    for (std::size_t i = heldCount; i > 0; --i)
        if (heldNotes[i - 1].held)
            return &heldNotes[i - 1];
    return heldCount == 0 ? nullptr : &heldNotes[heldCount - 1];
}

void Synth::updateMono(bool retrigger) noexcept
{
    auto& voice = voices.front();
    const auto* next = monoNote();
    if (next == nullptr)
    {
        voice.held = false;
        voice.amp.noteOff();
        voice.filter.noteOff();
        return;
    }

    const auto& on = next->on;
    const bool gated = voice.amp.stage != Env::idle && voice.amp.stage != Env::release;
    const bool sameNote = voice.port == on.port && voice.channel == on.channel && voice.note == on.note;
    if (!gated || !sameNote || retrigger)
    {
        const double glideFrom = gated ? voice.noteFrequency : 0.0;
        if (gated && !sameNote && patch.voicing.mode == PlayMode::legato)
            setVoiceNote(voice, on, glideFrom);
        else
            trigger(voice, on, glideFrom);
    }
    voice.held = next->held;
    voice.controls = on.controls;
    controlStep = 0;
}

void Synth::changeVoiceMode(PlayMode previous) noexcept
{
    const auto next = patch.voicing.mode;
    if (previous == next)
        return;
    if (previous == PlayMode::poly)
    {
        if (const auto* note = monoNote())
        {
            const auto& on = note->on;
            const auto current = std::find_if(voices.begin(), voices.end(), [&](const Voice& v)
            {
                return v.amp.stage != Env::idle && v.amp.stage != Env::release
                    && v.port == on.port && v.channel == on.channel && v.note == on.note;
            });
            if (current != voices.end())
                std::iter_swap(voices.begin(), current);
            else
                retire(voices.front());
        }
        for (auto it = voices.begin() + 1; it != voices.end(); ++it)
            retire(*it);
        updateMono(false);
    }
    else if (next == PlayMode::poly)
    {
        const auto* current = monoNote();
        for (std::size_t i = 0; i < heldCount; ++i)
        {
            const auto& entry = heldNotes[i];
            if (&entry != current)
                startVoice(entry.on, entry.held);
        }
    }
}

void Synth::event(const Event& e) noexcept
{
    if (e.type == EventType::panic)
    {
        for (auto& v : voices) retire(v);
        heldCount = 0;
        return;
    }
    trackNotes(e);
    const bool pressed = e.type == EventType::noteOn && e.velocity > 0.0f;
    if (patch.voicing.mode != PlayMode::poly)
    {
        auto& voice = voices.front();
        const bool sameChannel = voice.port == e.port && voice.channel == e.channel;
        if (voice.port == e.port && (e.type == EventType::disconnect ||
            (e.type == EventType::allSoundOff && sameChannel)))
            retire(voice);
        else if (e.type == EventType::expression && sameChannel)
            voice.controls = e.controls;
        updateMono(pressed);
        return;
    }
    if (pressed)
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

    for (int i = 0; i < oscillatorCount; ++i)
    {
        const auto& osc = patch.osc[i];
        const float tune = 12.0f * osc.octave + osc.semitone + osc.fine * 0.01f
                           + 2.0f * v.controls.bend + 12.0f * bipolar(mod[pitchDestinations[i]]);
        v.targetTuning[i] = std::exp2(tune / 12.0f);
        v.targetPosition[i] = unit(osc.position + mod[positionDestinations[i]]);
        const int count = patch.voicing.unison;
        const float centre = bipolar(osc.pan + mod[pan] + mod[panDestinations[i]]);
        const float level = unit(osc.level + mod[levelDestinations[i]]) / count;
        for (int index = 0; index < maxUnison; ++index)
        {
            auto& layer = v.layers[i][index];
            if (index < count)
            {
                const float angle = (bipolar(centre + unisonPan[index]) + 1.0f) * 0.5f * halfPi;
                layer.targetGainL = level * std::cos(angle);
                layer.targetGainR = level * std::sin(angle);
                layer.targetRatio = unisonRatio[index];
            }
            else
            {
                layer.targetGainL = layer.targetGainR = 0.0f;
            }
            if (v.fresh)
            {
                layer.gainL = layer.targetGainL;
                layer.gainR = layer.targetGainR;
                layer.ratio = layer.targetRatio;
            }
        }
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
    v.targetDriveMod = bipolar(mod[filterDrive]);
    v.targetAmpGain = 1.0f + bipolar(mod[ampGain]);
    if (v.fresh)
    {
        v.tuning = v.targetTuning;
        v.position = v.targetPosition;
        v.g = v.targetG;
        v.damping = v.targetDamping;
        v.driveMod = v.targetDriveMod;
        v.ampGain = v.targetAmpGain;
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
            if (v.glideSamples > 0)
            {
                --v.glideSamples;
                v.noteFrequency = v.glideSamples == 0 ? v.targetNoteFrequency : v.noteFrequency * v.glideRatio;
            }
            for (std::size_t i = 0; i < v.lfoPhases.size(); ++i)
                advance(v.lfoPhases[i], patch.lfos[i].rate * seconds);

            float voiceL = 0.0f, voiceR = 0.0f;
            for (int i = 0; i < oscillatorCount; ++i)
            {
                v.tuning[i] = lerp(v.tuning[i], v.targetTuning[i], smoothing);
                v.position[i] = lerp(v.position[i], v.targetPosition[i], smoothing);
                const float base = static_cast<float>(v.noteFrequency) * v.tuning[i];
                for (int index = 0; index < maxUnison; ++index)
                {
                    auto& layer = v.layers[i][index];
                    if (index >= patch.voicing.unison && layer.gainL + layer.gainR <= 0.00001f)
                        continue;
                    layer.ratio = lerp(layer.ratio, layer.targetRatio, smoothing);
                    layer.gainL = lerp(layer.gainL, layer.targetGainL, smoothing);
                    layer.gainR = lerp(layer.gainR, layer.targetGainR, smoothing);
                    const float hz = base * layer.ratio;
                    if (layer.gainL + layer.gainR > 0.00001f)
                    {
                        float value = waveBank.sample(v.table[i], v.position[i], layer.phase, hz, rate);
                        if (v.tableMix[i] < 1.0f)
                        {
                            const float previous = waveBank.sample(v.previousTable[i], v.position[i], layer.phase, hz, rate);
                            value = lerp(previous, value, v.tableMix[i]);
                        }
                        voiceL += value * layer.gainL;
                        voiceR += value * layer.gainR;
                    }
                    advance(layer.phase, hz * seconds);
                }
                v.tableMix[i] = unit(v.tableMix[i] + tableStep);
            }
            v.g = lerp(v.g, v.targetG, smoothing);
            v.damping = lerp(v.damping, v.targetDamping, smoothing);
            const float a1 = 1.0f / (1.0f + v.g * (v.g + v.damping));
            const float a2 = v.g * a1;
            const float a3 = v.g * a2;
            v.driveMod = lerp(v.driveMod, v.targetDriveMod, smoothing);
            const float voiceDrive = unit(drive + v.driveMod);
            const auto saturate = [&](float value)
            {
                return lerp(value, std::tanh(value * (1.0f + 6.0f * voiceDrive)), voiceDrive);
            };
            v.velocityGain = lerp(v.velocityGain, v.velocity, smoothing);
            v.ampGain = lerp(v.ampGain, v.targetAmpGain, smoothing);
            const float amplitude = amp * v.velocityGain * v.ampGain * 0.23f;
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
