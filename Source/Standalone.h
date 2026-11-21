#pragma once

#include "Synth.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace mot
{
class Standalone final : public juce::AudioIODeviceCallback, private juce::Timer,
                         private juce::ChangeListener
{
public:
    struct Output { juce::String type, name; bool selected; };

    Standalone();
    ~Standalone() override;

    const WaveBank& waves() const noexcept { return waveBank; }
    const NamedPatch& patch() const noexcept { return current; }
    void apply(NamedPatch patch);
    bool load(const juce::File& file);
    bool save(const juce::File& file);
    void play(int note, bool on);
    void panic();
    int inputCount() const noexcept { return static_cast<int>(inputs.size()); }
    int unavailableInputs() const noexcept { return unavailable; }
    bool hasRecentMidiNote() const noexcept;
    int voiceCount() const noexcept { return voices.load(std::memory_order_relaxed); }
    float outputPeak() const noexcept { return peak.load(std::memory_order_relaxed); }

    juce::String audioStatus() const;
    std::vector<Output> outputs();
    juce::String selectOutput(const Output& output);

private:
    struct Input;
    struct TimedEvent { Event event; double time; };
    static constexpr std::size_t eventCapacity = 1024;

    // Triple buffer. Each thread owns one slot. The tagged middle slot holds the
    // newest complete patch until the audio thread claims it.
    struct PatchMailbox
    {
        void publish(const Patch&) noexcept;
        const Patch* latest() noexcept;
        std::array<Patch, 3> slots {};
        std::atomic<unsigned> middle { 1 };
        unsigned writer = 2, reader = 0;
    };

    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshMidi();
    void receive(Input&, const juce::MidiMessage&);
    void push(const Event&);
    void enqueue(const Event&) noexcept;
    void storeSession();

    WaveBank waveBank;
    Synth synth { waveBank };
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::PropertiesFile> settings;
    NamedPatch current = factoryPresets().front();
    PatchMailbox patches;
    bool sessionDirty = false;
    juce::String audioError;

    std::vector<std::unique_ptr<Input>> inputs;
    juce::MidiDeviceListConnection midiDeviceChanges;
    std::uint32_t nextPort = 1;
    int unavailable = 0;
    std::mutex producers;
    std::array<TimedEvent, eventCapacity> fifo {};
    std::array<Event, eventCapacity> blockEvents {};
    std::atomic<std::uint32_t> writeIndex { 0 }, readIndex { 0 };
    std::atomic<bool> overflow { false };
    std::atomic<int> voices { 0 };
    std::atomic<float> peak { 0 };
    std::atomic<double> lastMidiNote { -1 };
    double audioRate = 48000;
};
}
