#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace mot
{
constexpr int tableCount = 6;
constexpr int sourceCount = 8;
constexpr int destinationCount = 7;
constexpr int presetCount = 8;
constexpr int maxUnison = 4;
constexpr float maxGlide = 2.0f, maxUnisonDetune = 50.0f;

enum Source { lfo1, lfo2, filterEnvelope, ampEnvelope, velocity, keyTrack, wheel, pressure };
enum Destination { positionA, positionB, pitchA, pitchB, cutoff, resonance, pan };
enum class FilterMode { lowPass, bandPass, highPass };
enum class Shape { sine, triangle, ramp, square };
enum class PlayMode { poly, mono, legato };

inline constexpr std::array<const char*, tableCount> tableNames {
    "Basic Shapes", "Pulse Width", "Hollow Body", "Harmonic Ladder", "Folded Sine", "Glass Formants"
};
inline constexpr std::array<const char*, sourceCount> sourceNames {
    "LFO 1", "LFO 2", "Filter Env", "Amp Env", "Velocity", "Key Track", "Mod Wheel", "Pressure"
};
inline constexpr std::array<const char*, destinationCount> destinationNames {
    "Pos A", "Pos B", "Pitch A", "Pitch B", "Cutoff", "Reso", "Pan"
};

struct Oscillator
{
    int table = 0;
    float position = 0.27f, level = 0.8f;
    int octave = 0, semitone = 0;
    float fine = 0.0f, pan = 0.0f, phase = 0.0f;
    bool resetPhase = true;
};

struct Envelope { float attack, decay, sustain, release; };

struct Lfo
{
    float rate = 1.0f, depth = 1.0f;
    Shape shape = Shape::sine;
    bool restart = false;
};

struct Voicing
{
    PlayMode mode = PlayMode::poly;
    float glide = 0.0f;
    int unison = 1;
    float detune = 12.0f, width = 0.75f;
};

struct Patch
{
    std::array<Oscillator, 2> osc { Oscillator {}, Oscillator { 0, 0.6f, 0.0f } };
    FilterMode filterMode = FilterMode::lowPass;
    float cutoff = 7800.0f, resonance = 0.13f, drive = 0.0f, volume = 0.65f;
    Envelope filterEnv { 0.015f, 0.28f, 0.20f, 0.25f };
    Envelope ampEnv { 0.008f, 0.24f, 0.80f, 0.20f };
    std::array<Lfo, 2> lfos { Lfo {}, Lfo { 0.37f, 1.0f, Shape::triangle, false } };
    std::array<std::array<float, destinationCount>, sourceCount> matrix {};
    Voicing voicing;
};

struct NamedPatch { std::string name; Patch sound; };

const std::array<NamedPatch, presetCount>& factoryPresets();
std::string encodePatch(const NamedPatch& patch);
std::optional<NamedPatch> decodePatch(std::string_view data);
}
