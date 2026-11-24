#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace mot
{
constexpr int oscillatorCount = 3;
constexpr int sourceCount = 8;
constexpr int presetCount = 9;
constexpr int maxUnison = 4;
constexpr int maxOscillatorSemitones = 24;
constexpr float maxGlide = 2.0f, maxUnisonDetune = 50.0f;
constexpr float minTapeTime = 0.05f, maxTapeTime = 1.2f, maxTapeFeedback = 0.9f;
constexpr float minPlateDecay = 0.25f, maxPlateDecay = 20.0f;
constexpr int minArpTempo = 40, maxArpTempo = 240;

float plateDecaySeconds(float normalized) noexcept;

enum Source { lfo1, lfo2, filterEnvelope, ampEnvelope, velocity, keyTrack, wheel, pressure };
// Destination values are stored in patch files. Append new destinations before destinationCount.
enum Destination
{
    positionA, positionB, pitchA, pitchB, cutoff, resonance, pan, positionC, pitchC,
    levelA, levelB, levelC, panA, panB, panC, filterDrive, ampGain, destinationCount
};
enum Wavetable
{
    basicShapes, pulseWidth, hollowBody, harmonicLadder, foldedSine, glassFormants,
    sineTable, triangleTable, sawTable, squareTable, pulseTable, tableCount
};
enum class FilterMode { lowPass, bandPass, highPass };
enum class Shape { sine, triangle, ramp, square };
enum class PlayMode { poly, mono, legato };
enum class ArpDirection { up, down, upDown };

inline constexpr std::array<const char*, tableCount> tableNames {
    "Basic Shapes", "Pulse Width", "Hollow Body", "Harmonic Ladder", "Folded Sine", "Glass Formants",
    "Sine", "Triangle", "Sawtooth", "Square", "Variable Pulse"
};
inline constexpr bool isFixedTable(int table) noexcept { return table >= sineTable && table <= squareTable; }
inline constexpr float pulseDuty(float position) noexcept { return 0.05f + 0.90f * position; }
inline constexpr std::array<Destination, oscillatorCount> positionDestinations { positionA, positionB, positionC };
inline constexpr std::array<Destination, oscillatorCount> pitchDestinations { pitchA, pitchB, pitchC };
inline constexpr std::array<Destination, oscillatorCount> levelDestinations { levelA, levelB, levelC };
inline constexpr std::array<Destination, oscillatorCount> panDestinations { panA, panB, panC };
inline constexpr std::array<const char*, sourceCount> sourceNames {
    "LFO 1", "LFO 2", "Filter Env", "Amp Env", "Velocity", "Key Track", "Mod Wheel", "Pressure"
};
inline constexpr std::array<const char*, destinationCount> destinationNames {
    "Pos A", "Pos B", "Pitch A", "Pitch B", "Cutoff", "Reso", "Pan", "Pos C", "Pitch C",
    "Level A", "Level B", "Level C", "Pan A", "Pan B", "Pan C", "Drive", "Amp Gain"
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

struct TapeDelaySettings
{
    bool enabled = false;
    float time = 0.32f, feedback = 0.38f, wear = 0.32f, mix = 0.28f;
};

struct ReverbSettings
{
    bool enabled = false;
    float decay = 0.55f, damping = 0.42f, mix = 0.25f;
};

struct ArpSettings
{
    bool enabled = false;
    int tempo = 120;
    ArpDirection direction = ArpDirection::up;
};

struct Patch
{
    std::array<Oscillator, oscillatorCount> osc {
        Oscillator {}, Oscillator { basicShapes, 0.6f, 0.0f }, Oscillator { triangleTable, 0.5f, 0.0f }
    };
    FilterMode filterMode = FilterMode::lowPass;
    float cutoff = 7800.0f, resonance = 0.13f, drive = 0.0f, volume = 0.65f;
    Envelope filterEnv { 0.015f, 0.28f, 0.20f, 0.25f };
    Envelope ampEnv { 0.008f, 0.24f, 0.80f, 0.20f };
    std::array<Lfo, 2> lfos { Lfo {}, Lfo { 0.37f, 1.0f, Shape::triangle, false } };
    std::array<std::array<std::optional<float>, destinationCount>, sourceCount> matrix {};
    Voicing voicing;
    TapeDelaySettings tapeDelay;
    ReverbSettings reverb;
    ArpSettings arp;
};

struct NamedPatch { std::string name; Patch sound; };

const std::array<NamedPatch, presetCount>& factoryPresets();
std::string encodePatch(const NamedPatch& patch);
std::optional<NamedPatch> decodePatch(std::string_view data);
}
