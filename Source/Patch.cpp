#include "Patch.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace mot
{
namespace
{
constexpr std::array<const char*, oscillatorCount> oscillatorTags { "OSC_A", "OSC_B", "OSC_C" };
constexpr int patchVersion = 6;
constexpr int macroBranchVersion = 3;
constexpr int thirdOscillatorVersion = 4;
constexpr int legacyOscillatorCount = 2, legacyDestinationCount = 7;
constexpr int fixedMatrixDestinationCount = 9;
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
        bass.matrix[filterEnvelope][defaultMatrixColumn(cutoff)] = 0.66f;
        bass.matrix[velocity][defaultMatrixColumn(cutoff)] = 0.21f;
        bank[1] = { "Low Circuit", bass };

        Patch pad;
        pad.osc[0] = { 5, 0.25f, 0.58f, 0, 0, -6, -0.48f, 0.0f, false };
        pad.osc[1] = { 2, 0.60f, 0.45f, 0, 0, 9, 0.47f, 0.0f, false };
        pad.cutoff = 4200; pad.resonance = 0.23f;
        pad.ampEnv = { 0.52f, 0.87f, 0.73f, 1.34f };
        pad.lfos[0].rate = 0.25f;
        pad.matrix[lfo1][defaultMatrixColumn(positionA)] = 0.24f;
        pad.matrix[lfo2][defaultMatrixColumn(positionB)] = -0.22f;
        pad.matrix[lfo2][defaultMatrixColumn(pan)] = 0.26f;
        bank[2] = { "Glass Current", pad };

        Patch pluck;
        pluck.osc[0] = { 3, 0.49f, 0.7f };
        pluck.osc[1] = { 4, 0.31f, 0.33f, 1, 0, 8.0f, 0.4f };
        pluck.cutoff = 1250; pluck.resonance = 0.28f;
        pluck.ampEnv = { 0.003f, 0.32f, 0.06f, 0.33f };
        pluck.filterEnv = { 0.003f, 0.14f, 0.0f, 0.23f };
        pluck.matrix[filterEnvelope][defaultMatrixColumn(cutoff)] = 0.55f;
        pluck.matrix[ampEnvelope][defaultMatrixColumn(positionB)] = 0.25f;
        pluck.matrix[keyTrack][defaultMatrixColumn(cutoff)] = 0.33f;
        bank[3] = { "Copper Pluck", pluck };

        Patch lead;
        lead.osc[0] = { 1, 0.3f, 0.68f, 0, 0, -7, -0.19f };
        lead.osc[1] = { 0, 0.95f, 0.49f, 0, 0, 8, 0.19f };
        lead.cutoff = 3450; lead.resonance = 0.28f; lead.drive = 0.26f;
        lead.ampEnv = { 0.012f, 0.19f, 0.68f, 0.16f };
        lead.lfos[0] = { 5.8f, 1.0f, Shape::sine, true };
        lead.matrix[lfo1][defaultMatrixColumn(pitchA)] = 0.014f;
        lead.matrix[lfo1][defaultMatrixColumn(pitchB)] = 0.014f;
        lead.matrix[wheel][defaultMatrixColumn(positionA)] = 0.45f;
        lead.matrix[pressure][defaultMatrixColumn(cutoff)] = 0.43f;
        bank[4] = { "Pulse Driver", lead };

        Patch organ;
        organ.osc[0] = { 2, 0.41f, 0.65f };
        organ.osc[1] = { 3, 0.25f, 0.4f, 0, 7, 1.0f, 0.24f };
        organ.cutoff = 5300; organ.filterMode = FilterMode::bandPass;
        organ.ampEnv = { 0.025f, 0.14f, 0.86f, 0.30f };
        organ.matrix[lfo2][defaultMatrixColumn(cutoff)] = 0.23f;
        organ.matrix[lfo2][defaultMatrixColumn(pan)] = -0.32f;
        bank[5] = { "Hollow Choir", organ };

        Patch scan;
        scan.osc[0] = { 4, 0.51f, 0.61f, 0, 0, 0.0f, -0.22f };
        scan.osc[1] = { 5, 0.48f, 0.33f, 1, 0, -6, 0.24f };
        scan.cutoff = 3150; scan.resonance = 0.36f;
        scan.ampEnv = { 0.04f, 0.48f, 0.75f, 0.34f };
        scan.lfos[0] = { 3.4f, 0.90f, Shape::ramp, true };
        scan.matrix[lfo1][defaultMatrixColumn(positionA)] = 0.47f;
        scan.matrix[lfo1][defaultMatrixColumn(positionB)] = -0.47f;
        scan.matrix[lfo1][defaultMatrixColumn(cutoff)] = 0.33f;
        bank[6] = { "Vector Scan", scan };

        Patch keys;
        keys.osc[0] = { 0, 0.18f, 0.77f };
        keys.osc[1] = { 5, 0.17f, 0.28f, 1, 0, 11.0f, 0.45f };
        keys.cutoff = 5600; keys.resonance = 0.19f;
        keys.ampEnv = { 0.01f, 0.65f, 0.35f, 0.48f };
        keys.filterEnv = { 0.006f, 0.23f, 0.0f, 0.3f };
        keys.matrix[filterEnvelope][defaultMatrixColumn(positionB)] = 0.48f;
        keys.matrix[velocity][defaultMatrixColumn(cutoff)] = 0.22f;
        keys.matrix[lfo2][defaultMatrixColumn(pitchB)] = 0.012f;
        bank[7] = { "Night Keys", keys };

        Patch analog;
        analog.osc[0] = { sawTable, 0.5f, 0.65f, 0, 0, -5.0f, -0.21f, 0.0f, false };
        analog.osc[1] = { squareTable, 0.5f, 0.43f, 0, 0, 5.0f, 0.21f, 0.0f, false };
        analog.osc[2] = { triangleTable, 0.5f, 0.31f, -1 };
        analog.cutoff = 1600; analog.resonance = 0.28f; analog.drive = 0.12f;
        analog.ampEnv = { 0.012f, 0.31f, 0.72f, 0.24f };
        analog.filterEnv = { 0.006f, 0.36f, 0.13f, 0.25f };
        analog.matrix[filterEnvelope][defaultMatrixColumn(cutoff)] = 0.58f;
        bank[8] = { "Analog Stack", analog };
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
    out << "MOT11 " << patchVersion << "\nNAME " << std::quoted(named.name) << '\n';
    for (std::size_t i = 0; i < p.osc.size(); ++i)
    {
        const auto& o = p.osc[i];
        out << oscillatorTags[i] << ' ' << o.table << ' ' << o.position << ' '
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
        << ' ' << v.detune << ' ' << v.width << "\nTARGETS";
    for (auto target : p.matrixTargets)
        out << ' ' << static_cast<int>(target);
    out << "\nMATRIX\n";
    for (const auto& row : p.matrix)
    {
        for (auto value : row)
            out << value << ' ';
        out << '\n';
    }
    const auto& tape = p.tapeDelay;
    const auto& reverb = p.reverb;
    out << "TAPE_DELAY " << tape.enabled << ' ' << tape.time << ' ' << tape.feedback
        << ' ' << tape.wear << ' ' << tape.mix << '\n';
    out << "REVERB " << reverb.enabled << ' ' << reverb.size << ' ' << reverb.damping << ' ' << reverb.mix << '\n';
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
    if (!readTag("MOT11") || !readRange(version, 1, patchVersion) || version == macroBranchVersion || !readTag("NAME")
        || !(in >> std::quoted(named.name)) || named.name.empty() || named.name.size() > 96
        || named.name.find_first_of("\r\n\t") != std::string::npos
        || named.name.find('\0') != std::string::npos)
        return std::nullopt;

    auto& p = named.sound;
    const bool legacy = version < thirdOscillatorVersion;
    const int oscillatorSlots = legacy ? legacyOscillatorCount : oscillatorCount;
    const int lastTable = legacy ? static_cast<int>(glassFormants) : tableCount - 1;
    for (int i = 0; i < oscillatorSlots; ++i)
    {
        auto& o = p.osc[i];
        if (!readTag(oscillatorTags[i]) || !readRange(o.table, 0, lastTable)
            || !readRange(o.position, 0, 1) || !readRange(o.level, 0, 1)
            || !readRange(o.octave, -2, 2) || !readRange(o.semitone, -maxOscillatorSemitones, maxOscillatorSemitones)
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
    const bool assignableMatrix = version >= 5;
    if (assignableMatrix)
    {
        if (!readTag("TARGETS"))
            return std::nullopt;
        std::array<bool, destinationCount> assigned {};
        for (auto& target : p.matrixTargets)
        {
            int id = 0;
            if (!readRange(id, 0, destinationCount - 1) || assigned[id])
                return std::nullopt;
            target = static_cast<Destination>(id);
            assigned[id] = true;
        }
    }
    if (!readTag("MATRIX"))
        return std::nullopt;
    const int routesPerSource = legacy ? legacyDestinationCount : fixedMatrixDestinationCount;
    for (auto& row : p.matrix)
    {
        if (assignableMatrix)
        {
            for (auto& amount : row)
                if (!readRange(amount, -1, 1))
                    return std::nullopt;
            continue;
        }
        for (int i = 0; i < routesPerSource; ++i)
            if (!readRange(row[defaultMatrixColumn(static_cast<Destination>(i))], -1, 1))
                return std::nullopt;
    }
    if (version >= 6)
    {
        auto& tape = p.tapeDelay;
        auto& reverb = p.reverb;
        if (!readTag("TAPE_DELAY") || !(in >> tape.enabled) || !readRange(tape.time, minTapeTime, maxTapeTime)
            || !readRange(tape.feedback, 0, maxTapeFeedback) || !readRange(tape.wear, 0, 1)
            || !readRange(tape.mix, 0, 1) || !readTag("REVERB") || !(in >> reverb.enabled)
            || !readRange(reverb.size, 0, 1) || !readRange(reverb.damping, 0, 1) || !readRange(reverb.mix, 0, 1))
            return std::nullopt;
    }
    in >> std::ws;
    return in.eof() ? std::optional { named } : std::nullopt;
}
}
