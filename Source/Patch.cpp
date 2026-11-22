#include "Patch.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace mot
{
const std::array<MacroTarget, macroTargetCount>& macroTargets() noexcept
{
    using Group = MacroGroup;
    using Unit = MacroUnit;
    // The patch format stores these indices. Append new destinations at the end.
    static constexpr std::array<MacroTarget, macroTargetCount> targets {{
        { Group::oscA, "Position", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.osc[0].position; } },
        { Group::oscA, "Level", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.osc[0].level; } },
        { Group::oscA, "Fine", Unit::cents, -50, 50, [](Patch& p) -> float& { return p.osc[0].fine; } },
        { Group::oscA, "Pan", Unit::pan, -1, 1, [](Patch& p) -> float& { return p.osc[0].pan; } },
        { Group::oscB, "Position", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.osc[1].position; } },
        { Group::oscB, "Level", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.osc[1].level; } },
        { Group::oscB, "Fine", Unit::cents, -50, 50, [](Patch& p) -> float& { return p.osc[1].fine; } },
        { Group::oscB, "Pan", Unit::pan, -1, 1, [](Patch& p) -> float& { return p.osc[1].pan; } },
        { Group::filter, "Cutoff", Unit::cutoff, 20, 20000, [](Patch& p) -> float& { return p.cutoff; } },
        { Group::filter, "Resonance", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.resonance; } },
        { Group::filter, "Drive", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.drive; } },
        { Group::filterEnv, "Attack", Unit::time, 0.002f, 6, [](Patch& p) -> float& { return p.filterEnv.attack; } },
        { Group::filterEnv, "Decay", Unit::time, 0.002f, 6, [](Patch& p) -> float& { return p.filterEnv.decay; } },
        { Group::filterEnv, "Sustain", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.filterEnv.sustain; } },
        { Group::filterEnv, "Release", Unit::time, 0.002f, 8, [](Patch& p) -> float& { return p.filterEnv.release; } },
        { Group::ampEnv, "Attack", Unit::time, 0.002f, 6, [](Patch& p) -> float& { return p.ampEnv.attack; } },
        { Group::ampEnv, "Decay", Unit::time, 0.002f, 6, [](Patch& p) -> float& { return p.ampEnv.decay; } },
        { Group::ampEnv, "Sustain", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.ampEnv.sustain; } },
        { Group::ampEnv, "Release", Unit::time, 0.002f, 8, [](Patch& p) -> float& { return p.ampEnv.release; } },
        { Group::lfo1, "Rate", Unit::frequency, 0.05f, 20, [](Patch& p) -> float& { return p.lfos[0].rate; } },
        { Group::lfo1, "Depth", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.lfos[0].depth; } },
        { Group::lfo2, "Rate", Unit::frequency, 0.05f, 20, [](Patch& p) -> float& { return p.lfos[1].rate; } },
        { Group::lfo2, "Depth", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.lfos[1].depth; } },
        { Group::voicing, "Glide", Unit::time, 0, maxGlide, [](Patch& p) -> float& { return p.voicing.glide; } },
        { Group::voicing, "Detune", Unit::cents, 0, maxUnisonDetune, [](Patch& p) -> float& { return p.voicing.detune; } },
        { Group::voicing, "Width", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.voicing.width; } },
        { Group::output, "Volume", Unit::percent, 0, 1, [](Patch& p) -> float& { return p.volume; } }
    }};
    return targets;
}

namespace
{
void applyMacroRange(Patch& patch, int index, const MacroRange& range, float value) noexcept
{
    const auto& target = macroTargets()[index];
    float result = range.start;
    if (value == 1.0f)
        result = range.end;
    else if (value > 0.0f)
    {
        if (target.unit == MacroUnit::cutoff || target.unit == MacroUnit::frequency)
            result = range.start * std::pow(range.end / range.start, value);
        else
            result += (range.end - range.start) * value;
    }
    target.parameter(patch) = std::clamp(result, target.min, target.max);
}
}

void moveMacro(Patch& patch, int index, float value) noexcept
{
    auto& macro = patch.macros[index];
    macro.value = std::clamp(value, 0.0f, 1.0f);
    for (int i = 0; i < macroTargetCount; ++i)
        if (const auto& range = macro.ranges[i])
            applyMacroRange(patch, i, *range, macro.value);
}

void setMacroRange(Patch& patch, int macro, int target, MacroRange range) noexcept
{
    auto& control = patch.macros[macro];
    control.ranges[target] = range;
    applyMacroRange(patch, target, range, control.value);
}

std::optional<float> encoderValue(EncoderMode mode, int value, float current) noexcept
{
    if (mode == EncoderMode::absolute)
        return static_cast<float>(value) / 127.0f;

    int delta = 0;
    switch (mode)
    {
        case EncoderMode::relative1: delta = value - 64; break;
        case EncoderMode::relative2: delta = value < 64 ? value : value - 128; break;
        case EncoderMode::relative3: delta = value - 16; break;
        default: break;
    }
    if (delta == 0 || std::abs(delta) > 3)
        return std::nullopt;
    return std::clamp(current + static_cast<float>(delta) / 127.0f, 0.0f, 1.0f);
}

const std::array<NamedPatch, presetCount>& factoryPresets()
{
    static const auto presets = []
    {
        std::array<NamedPatch, presetCount> bank {};
        bank[0] = { "Init", {} };

        Patch bass;
        bass.osc[0] = { 0, 0.86f, 0.79f };
        bass.osc[1] = { 1, 0.63f, 0.35f, -1 };
        bass.cutoff = 320; bass.resonance = 0.31f; bass.drive = 0.35f;
        bass.ampEnv = { 0.004f, 0.22f, 0.58f, 0.10f };
        bass.filterEnv = { 0.004f, 0.15f, 0.04f, 0.18f };
        bass.matrix[filterEnvelope][cutoff] = 0.66f;
        bass.matrix[velocity][cutoff] = 0.21f;
        bank[1] = { "Low Circuit", bass };

        Patch pad;
        pad.osc[0] = { 5, 0.25f, 0.58f, 0, 0, -6, -0.48f, 0.0f, false };
        pad.osc[1] = { 2, 0.60f, 0.45f, 0, 0, 9, 0.47f, 0.0f, false };
        pad.cutoff = 4200; pad.resonance = 0.23f;
        pad.ampEnv = { 0.52f, 0.87f, 0.73f, 1.34f };
        pad.lfos[0].rate = 0.25f;
        pad.matrix[lfo1][positionA] = 0.24f;
        pad.matrix[lfo2][positionB] = -0.22f;
        pad.matrix[lfo2][pan] = 0.26f;
        bank[2] = { "Glass Current", pad };

        Patch pluck;
        pluck.osc[0] = { 3, 0.49f, 0.7f };
        pluck.osc[1] = { 4, 0.31f, 0.33f, 1, 0, 8.0f, 0.4f };
        pluck.cutoff = 1250; pluck.resonance = 0.28f;
        pluck.ampEnv = { 0.003f, 0.32f, 0.06f, 0.33f };
        pluck.filterEnv = { 0.003f, 0.14f, 0.0f, 0.23f };
        pluck.matrix[filterEnvelope][cutoff] = 0.55f;
        pluck.matrix[ampEnvelope][positionB] = 0.25f;
        pluck.matrix[keyTrack][cutoff] = 0.33f;
        bank[3] = { "Copper Pluck", pluck };

        Patch lead;
        lead.osc[0] = { 1, 0.3f, 0.68f, 0, 0, -7, -0.19f };
        lead.osc[1] = { 0, 0.95f, 0.49f, 0, 0, 8, 0.19f };
        lead.cutoff = 3450; lead.resonance = 0.28f; lead.drive = 0.26f;
        lead.ampEnv = { 0.012f, 0.19f, 0.68f, 0.16f };
        lead.lfos[0] = { 5.8f, 1.0f, Shape::sine, true };
        lead.matrix[lfo1][pitchA] = 0.014f;
        lead.matrix[lfo1][pitchB] = 0.014f;
        lead.matrix[wheel][positionA] = 0.45f;
        lead.matrix[pressure][cutoff] = 0.43f;
        bank[4] = { "Pulse Driver", lead };

        Patch organ;
        organ.osc[0] = { 2, 0.41f, 0.65f };
        organ.osc[1] = { 3, 0.25f, 0.4f, 0, 7, 1.0f, 0.24f };
        organ.cutoff = 5300; organ.filterMode = FilterMode::bandPass;
        organ.ampEnv = { 0.025f, 0.14f, 0.86f, 0.30f };
        organ.matrix[lfo2][cutoff] = 0.23f;
        organ.matrix[lfo2][pan] = -0.32f;
        bank[5] = { "Hollow Choir", organ };

        Patch scan;
        scan.osc[0] = { 4, 0.51f, 0.61f, 0, 0, 0.0f, -0.22f };
        scan.osc[1] = { 5, 0.48f, 0.33f, 1, 0, -6, 0.24f };
        scan.cutoff = 3150; scan.resonance = 0.36f;
        scan.ampEnv = { 0.04f, 0.48f, 0.75f, 0.34f };
        scan.lfos[0] = { 3.4f, 0.90f, Shape::ramp, true };
        scan.matrix[lfo1][positionA] = 0.47f;
        scan.matrix[lfo1][positionB] = -0.47f;
        scan.matrix[lfo1][cutoff] = 0.33f;
        bank[6] = { "Vector Scan", scan };

        Patch keys;
        keys.osc[0] = { 0, 0.18f, 0.77f };
        keys.osc[1] = { 5, 0.17f, 0.28f, 1, 0, 11.0f, 0.45f };
        keys.cutoff = 5600; keys.resonance = 0.19f;
        keys.ampEnv = { 0.01f, 0.65f, 0.35f, 0.48f };
        keys.filterEnv = { 0.006f, 0.23f, 0.0f, 0.3f };
        keys.matrix[filterEnvelope][positionB] = 0.48f;
        keys.matrix[velocity][cutoff] = 0.22f;
        keys.matrix[lfo2][pitchB] = 0.012f;
        bank[7] = { "Night Keys", keys };
        return bank;
    }();
    return presets;
}

std::string encodePatch(const NamedPatch& named)
{
    const auto& p = named.sound;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "MOT11 3\nNAME " << std::quoted(named.name) << '\n';
    for (std::size_t i = 0; i < p.osc.size(); ++i)
    {
        const auto& o = p.osc[i];
        out << (i == 0 ? "OSC_A" : "OSC_B") << ' ' << o.table << ' ' << o.position << ' '
            << o.level << ' ' << o.octave << ' ' << o.semitone << ' ' << o.fine << ' '
            << o.pan << ' ' << o.phase << ' ' << o.resetPhase << '\n';
    }
    out << "FILTER " << static_cast<int>(p.filterMode) << ' ' << p.cutoff << ' ' << p.resonance
        << ' ' << p.drive << "\nVOLUME " << p.volume << '\n';
    const auto writeEnvelope = [&](const char* label, const Envelope& e)
    {
        out << label << ' ' << e.attack << ' ' << e.decay << ' ' << e.sustain << ' ' << e.release << '\n';
    };
    writeEnvelope("FILTER_ENV", p.filterEnv);
    writeEnvelope("AMP_ENV", p.ampEnv);
    for (std::size_t i = 0; i < p.lfos.size(); ++i)
    {
        const auto& lfo = p.lfos[i];
        out << (i == 0 ? "LFO_1" : "LFO_2") << ' ' << lfo.rate << ' ' << lfo.depth << ' '
            << static_cast<int>(lfo.shape) << ' ' << lfo.restart << '\n';
    }
    const auto& v = p.voicing;
    out << "VOICING " << static_cast<int>(v.mode) << ' ' << v.glide << ' ' << v.unison
        << ' ' << v.detune << ' ' << v.width << "\nMATRIX\n";
    for (const auto& row : p.matrix)
    {
        for (auto value : row)
            out << value << ' ';
        out << '\n';
    }
    out << "MACROS\n";
    for (const auto& macro : p.macros)
    {
        const auto count = std::count_if(macro.ranges.begin(), macro.ranges.end(),
                                         [](const auto& range) { return range.has_value(); });
        out << "MACRO " << macro.value << ' ' << count << '\n';
        for (int i = 0; i < macroTargetCount; ++i)
            if (const auto& range = macro.ranges[i])
                out << i << ' ' << range->start << ' ' << range->end << '\n';
    }
    return out.str();
}

std::optional<NamedPatch> decodePatch(std::string_view data)
{
    if (data.size() > 16384)
        return std::nullopt;
    std::istringstream in { std::string(data) };
    in.imbue(std::locale::classic());
    const auto readTag = [&](const char* expected)
    {
        std::string tag;
        return (in >> tag) && tag == expected;
    };
    const auto readRange = [&](auto& value, auto min, auto max)
    {
        return (in >> value) && value >= min && value <= max;
    };

    NamedPatch named;
    int version = 0;
    if (!readTag("MOT11") || !readRange(version, 1, 3) || !readTag("NAME")
        || !(in >> std::quoted(named.name)) || named.name.empty() || named.name.size() > 96
        || named.name.find_first_of("\r\n\t") != std::string::npos
        || named.name.find('\0') != std::string::npos)
        return std::nullopt;

    auto& p = named.sound;
    for (std::size_t i = 0; i < p.osc.size(); ++i)
    {
        auto& o = p.osc[i];
        if (!readTag(i == 0 ? "OSC_A" : "OSC_B") || !readRange(o.table, 0, tableCount - 1)
            || !readRange(o.position, 0, 1) || !readRange(o.level, 0, 1)
            || !readRange(o.octave, -2, 2) || !readRange(o.semitone, -12, 12)
            || !readRange(o.fine, -50, 50) || !readRange(o.pan, -1, 1)
            || !readRange(o.phase, 0, 1) || !(in >> o.resetPhase))
            return std::nullopt;
    }
    int mode = 0;
    if (!readTag("FILTER") || !readRange(mode, 0, 2) || !readRange(p.cutoff, 20, 20000)
        || !readRange(p.resonance, 0, 1) || !readRange(p.drive, 0, 1) || !readTag("VOLUME")
        || !readRange(p.volume, 0, 1))
        return std::nullopt;
    p.filterMode = static_cast<FilterMode>(mode);

    const auto readEnvelope = [&](const char* label, Envelope& e)
    {
        return readTag(label) && readRange(e.attack, 0.002f, 6) && readRange(e.decay, 0.002f, 6)
            && readRange(e.sustain, 0, 1) && readRange(e.release, 0.002f, 8);
    };
    if (!readEnvelope("FILTER_ENV", p.filterEnv) || !readEnvelope("AMP_ENV", p.ampEnv))
        return std::nullopt;

    for (std::size_t i = 0; i < p.lfos.size(); ++i)
    {
        auto& lfo = p.lfos[i];
        int shape = 0;
        if (!readTag(i == 0 ? "LFO_1" : "LFO_2") || !readRange(lfo.rate, 0.05f, 20)
            || !readRange(lfo.depth, 0, 1) || !readRange(shape, 0, 3) || !(in >> lfo.restart))
            return std::nullopt;
        lfo.shape = static_cast<Shape>(shape);
    }
    if (version >= 2)
    {
        auto& v = p.voicing;
        int playMode = 0;
        if (!readTag("VOICING") || !readRange(playMode, 0, 2) || !readRange(v.glide, 0, maxGlide)
            || !readRange(v.unison, 1, maxUnison) || !readRange(v.detune, 0, maxUnisonDetune)
            || !readRange(v.width, 0, 1))
            return std::nullopt;
        v.mode = static_cast<PlayMode>(playMode);
    }
    if (!readTag("MATRIX"))
        return std::nullopt;
    for (auto& row : p.matrix)
        for (auto& value : row)
            if (!readRange(value, -1, 1))
                return std::nullopt;
    if (version >= 3)
    {
        if (!readTag("MACROS"))
            return std::nullopt;
        for (auto& macro : p.macros)
        {
            int count = 0;
            if (!readTag("MACRO") || !readRange(macro.value, 0, 1) || !readRange(count, 0, macroTargetCount))
                return std::nullopt;
            for (int i = 0; i < count; ++i)
            {
                int index = 0;
                if (!readRange(index, 0, macroTargetCount - 1) || macro.ranges[index])
                    return std::nullopt;
                const auto& target = macroTargets()[index];
                MacroRange range {};
                if (!readRange(range.start, target.min, target.max) || !readRange(range.end, target.min, target.max))
                    return std::nullopt;
                macro.ranges[index] = range;
            }
        }
    }
    in >> std::ws;
    return in.eof() ? std::optional { named } : std::nullopt;
}
}
