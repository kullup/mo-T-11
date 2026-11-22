#include "Standalone.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace mot
{
namespace
{
double timeNow() noexcept
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "MO-T 11";
    options.folderName = "MO-T 11";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

const char* panelThemeKey(PanelTheme theme)
{
    switch (theme)
    {
        case PanelTheme::classicDark: return "classic-dark";
        case PanelTheme::midnight: return "midnight";
        case PanelTheme::classic: return "classic";
    }
    return "classic";
}
}

struct Standalone::Input final : public juce::MidiInputCallback
{
    Input(Standalone& app, juce::String token, std::uint32_t port)
        : owner(app), identifier(std::move(token)), id(port) {}

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override
    {
        owner.receive(*this, message);
    }

    Standalone& owner;
    const juce::String identifier;
    const std::uint32_t id;
    std::array<Controllers, 16> channels {};
    std::unique_ptr<juce::MidiInput> device;
};

void Standalone::PatchMailbox::publish(const Patch& patch) noexcept
{
    slots[writer] = patch;
    writer = middle.exchange(writer | 4u, std::memory_order_acq_rel) & 3u;
}

const Patch* Standalone::PatchMailbox::latest() noexcept
{
    if ((middle.load(std::memory_order_acquire) & 4u) == 0)
        return nullptr;
    reader = middle.exchange(reader, std::memory_order_acq_rel) & 3u;
    return &slots[reader];
}

Standalone::Standalone() : settings(std::make_unique<juce::PropertiesFile>(settingsOptions()))
{
    if (auto loaded = decodePatch(settings->getValue("patch").toStdString()))
        current = std::move(*loaded);
    patches.publish(current.sound);

    const auto saved = settings->getXmlValue("audio");
    audioError = deviceManager.initialise(0, 2, saved.get(), true);
    deviceManager.addAudioCallback(this);
    deviceManager.addChangeListener(this);
    midiDeviceChanges = juce::MidiDeviceListConnection::make([this] { refreshMidi(); });
    refreshMidi();
    startTimerHz(1);
}

Standalone::~Standalone()
{
    stopTimer();
    midiDeviceChanges.reset();
    deviceManager.removeChangeListener(this);
    storeSession();
    settings->saveIfNeeded();
    deviceManager.removeAudioCallback(this);
    inputs.clear();
}

void Standalone::apply(NamedPatch patch)
{
    current = std::move(patch);
    patches.publish(current.sound);
    sessionDirty = true;
}

PanelTheme Standalone::panelTheme() const
{
    const auto stored = settings->getValue("panelTheme");
    for (const auto theme : { PanelTheme::classicDark, PanelTheme::midnight })
        if (stored == panelThemeKey(theme))
            return theme;
    return PanelTheme::classic;
}

void Standalone::setPanelTheme(PanelTheme theme)
{
    settings->setValue("panelTheme", panelThemeKey(theme));
}

bool Standalone::load(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() > 16384)
        return false;
    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes) || bytes.isEmpty())
        return false;
    const auto next = decodePatch({ static_cast<const char*>(bytes.getData()), bytes.getSize() });
    if (!next)
        return false;
    apply(*next);
    return true;
}

bool Standalone::save(const juce::File& file)
{
    auto toSave = current;
    const auto label = file.getFileNameWithoutExtension().substring(0, 24).trim();
    if (label.isNotEmpty())
        toSave.name = label.toStdString();
    const auto data = encodePatch(toSave);
    if (!decodePatch(data))
        return false;
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithData(data.data(), data.size()) || !temporary.overwriteTargetFileWithTemporary())
        return false;
    apply(std::move(toSave));
    return true;
}

void Standalone::play(int note, bool on)
{
    push({ on ? EventType::noteOn : EventType::noteOff, 0, 1, note, 0.83f });
}

void Standalone::panic() { push({ EventType::panic }); }

bool Standalone::hasRecentMidiNote() const noexcept
{
    return timeNow() - lastMidiNote.load(std::memory_order_relaxed) < 0.3;
}

void Standalone::storeSession()
{
    if (!sessionDirty)
        return;
    settings->setValue("patch", juce::String::fromUTF8(encodePatch(current).c_str()));
    if (const auto xml = deviceManager.createStateXml())
        settings->setValue("audio", xml.get());
    sessionDirty = false;
}

void Standalone::changeListenerCallback(juce::ChangeBroadcaster*)
{
    sessionDirty = true;
    if (deviceManager.getCurrentAudioDevice() != nullptr)
        audioError.clear();
}

void Standalone::timerCallback()
{
    refreshMidi();
    storeSession();
}

void Standalone::refreshMidi()
{
    const auto available = juce::MidiInput::getAvailableDevices();
    for (auto it = inputs.begin(); it != inputs.end();)
    {
        const auto token = (*it)->identifier;
        const auto stillPresent = std::any_of(available.begin(), available.end(), [&](const auto& device)
        {
            return device.identifier == token;
        });
        if (stillPresent)
        {
            ++it;
            continue;
        }
        const auto id = (*it)->id;
        (*it)->device->stop();
        it = inputs.erase(it);
        push({ EventType::disconnect, id });
    }

    unavailable = 0;
    for (const auto& info : available)
    {
        if (std::any_of(inputs.begin(), inputs.end(), [&](const auto& input)
            { return input->identifier == info.identifier; }))
            continue;

        auto input = std::make_unique<Input>(*this, info.identifier, nextPort++);
        input->device = juce::MidiInput::openDevice(info.identifier, input.get());
        if (input->device == nullptr)
        {
            ++unavailable;
            continue;
        }
        input->device->start();
        inputs.push_back(std::move(input));
    }
}

void Standalone::receive(Input& input, const juce::MidiMessage& message)
{
    const int channel = message.getChannel();
    if (channel < 1 || channel > 16)
        return;
    const std::lock_guard<std::mutex> lock(producers);
    auto& controls = input.channels[static_cast<std::size_t>(channel - 1)];
    Event e { EventType::expression, input.id, channel };
    if (message.isNoteOn())
    {
        e.type = EventType::noteOn;
        e.note = message.getNoteNumber();
        e.velocity = message.getFloatVelocity();
        lastMidiNote.store(timeNow(), std::memory_order_relaxed);
    }
    else if (message.isNoteOff())
    {
        e.type = EventType::noteOff;
        e.note = message.getNoteNumber();
    }
    else if (message.isAllNotesOff())
        e.type = EventType::allNotesOff;
    else if (message.isAllSoundOff())
        e.type = EventType::allSoundOff;
    else if (message.isPitchWheel())
        controls.bend = (message.getPitchWheelValue() - 8192) / 8192.0f;
    else if (message.isChannelPressure())
        controls.pressure = message.getChannelPressureValue() / 127.0f;
    else if (message.isControllerOfType(1))
        controls.wheel = message.getControllerValue() / 127.0f;
    else if (message.isControllerOfType(64))
        controls.sustain = message.getControllerValue() >= 64;
    else if (message.isControllerOfType(121))
        controls = {};
    else
        return;
    e.controls = controls;
    enqueue(e);
}

void Standalone::push(const Event& event)
{
    const std::lock_guard<std::mutex> lock(producers);
    enqueue(event);
}

void Standalone::enqueue(const Event& event) noexcept
{
    const auto index = writeIndex.load(std::memory_order_relaxed);
    if (index - readIndex.load(std::memory_order_acquire) >= eventCapacity)
    {
        overflow.store(true, std::memory_order_release);
        return;
    }
    fifo[index % eventCapacity] = { event, timeNow() };
    writeIndex.store(index + 1, std::memory_order_release);
}

void Standalone::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    audioRate = device->getCurrentSampleRate();
    synth.prepare(audioRate);
    readIndex.store(writeIndex.load(std::memory_order_acquire), std::memory_order_release);
    voices.store(0, std::memory_order_relaxed);
    peak.store(0, std::memory_order_relaxed);
}

void Standalone::audioDeviceStopped()
{
    voices.store(0, std::memory_order_relaxed);
    peak.store(0, std::memory_order_relaxed);
}

void Standalone::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs,
                                                  int channels, int samples,
                                                  const juce::AudioIODeviceCallbackContext&)
{
    if (samples <= 0)
        return;
    const auto now = timeNow();
    const auto duration = static_cast<double>(samples) / audioRate;
    const auto end = writeIndex.load(std::memory_order_acquire);
    auto start = readIndex.load(std::memory_order_relaxed);
    std::size_t count = 0;
    if (overflow.exchange(false, std::memory_order_acq_rel))
    {
        start = end;
        blockEvents[count++] = { EventType::panic };
    }
    while (start != end)
    {
        const auto& entry = fifo[start % eventCapacity];
        const auto offset = std::clamp((entry.time - now + duration) * audioRate, 0.0,
                                       static_cast<double>(samples - 1));
        blockEvents[count] = entry.event;
        blockEvents[count++].offset = static_cast<int>(offset);
        ++start;
    }
    readIndex.store(end, std::memory_order_release);
    if (const auto* latest = patches.latest())
        synth.setPatch(*latest);

    float* left = nullptr;
    float* right = nullptr;
    for (int channel = 0; channel < channels; ++channel)
    {
        if (auto* output = outputs[channel])
        {
            if (left == nullptr)       left = output;
            else if (right == nullptr) right = output;
            else                       std::fill_n(output, samples, 0.0f);
        }
    }
    const juce::ScopedNoDenormals noDenormals;
    synth.render(left, right, samples, blockEvents.data(), count);
    float max = 0;
    for (auto* output : { left, right })
        if (output != nullptr)
            for (int i = 0; i < samples; ++i)
                max = std::max(max, std::abs(output[i]));
    auto previous = peak.load(std::memory_order_relaxed);
    peak.store(std::max(max, previous * std::exp(static_cast<float>(-duration / 0.30))), std::memory_order_relaxed);
    voices.store(synth.activeVoices(), std::memory_order_relaxed);
}

juce::String Standalone::audioStatus() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName() + "  /  " + juce::String(device->getCurrentSampleRate() / 1000.0, 1) + " kHz";
    return audioError.isNotEmpty() ? audioError : juce::String("No audio output");
}

std::vector<Standalone::Output> Standalone::outputs()
{
    std::vector<Output> result;
    const auto* currentDevice = deviceManager.getCurrentAudioDevice();
    const auto currentName = currentDevice != nullptr ? currentDevice->getName() : juce::String();
    const auto currentType = deviceManager.getCurrentAudioDeviceType();
    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames(false))
        {
            const auto api = type->getTypeName();
            result.push_back({ api, name, api == currentType && name == currentName });
        }
    }
    return result;
}

juce::String Standalone::selectOutput(const Output& output)
{
    const auto backup = deviceManager.createStateXml();
    if (output.type != deviceManager.getCurrentAudioDeviceType())
        deviceManager.setCurrentAudioDeviceType(output.type, true);
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName.clear();
    setup.outputDeviceName = output.name;
    setup.useDefaultOutputChannels = true;
    setup.sampleRate = 0;
    setup.bufferSize = 0;
    const auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
        audioError = deviceManager.initialise(0, 2, backup.get(), true);
    else
        audioError.clear();
    sessionDirty = true;
    return error;
}
}
