#include "Panel.h"
#include "Standalone.h"

namespace
{
class Window final : public juce::DocumentWindow
{
public:
    explicit Window(mot::Standalone& audio)
        : DocumentWindow("MO-T 11", juce::Colour(0xff252829), allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new mot::Panel(audio), true);
        setResizable(true, false);
        setResizeLimits(1080, 513, 2400, 1440);
        if (auto* constrainer = getConstrainer())
            constrainer->setFixedAspectRatio(static_cast<double>(mot::Panel::designWidth) / mot::Panel::designHeight);
        grip = std::make_unique<juce::ResizableCornerComponent>(this, getConstrainer());
        grip->setAlwaysOnTop(true);
        addAndMakeVisible(*grip);
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        int width = mot::Panel::designWidth;
        if (display != nullptr)
            width = juce::jmax(1080, juce::jmin(width, display->userArea.getWidth() - 60,
                (display->userArea.getHeight() - 90) * mot::Panel::designWidth / mot::Panel::designHeight));
        centreWithSize(width, width * mot::Panel::designHeight / mot::Panel::designWidth);
        setVisible(true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (auto* panel = performancePanel(); panel != nullptr && panel->playComputerKey(key))
            return true;
        return DocumentWindow::keyPressed(key);
    }

    bool keyStateChanged(bool isKeyDown) override
    {
        if (auto* panel = performancePanel(); !isKeyDown && panel != nullptr && panel->releaseComputerKeys())
            return true;
        return DocumentWindow::keyStateChanged(isKeyDown);
    }

    void activeWindowStatusChanged() override
    {
        DocumentWindow::activeWindowStatusChanged();
        if (auto* panel = performancePanel(); !isActiveWindow() && panel != nullptr)
            panel->releasePerformanceKeys();
    }

    void resized() override
    {
        DocumentWindow::resized();
        if (grip != nullptr)
        {
            grip->setVisible(!isFullScreen());
            grip->setBounds(getWidth() - 22, getHeight() - 22, 22, 22);
        }
    }

private:
    mot::Panel* performancePanel() const noexcept { return static_cast<mot::Panel*>(getContentComponent()); }
    std::unique_ptr<juce::ResizableCornerComponent> grip;
};

class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MO-T 11"; }
    const juce::String getApplicationVersion() override { return "0.1.1"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override
    {
        audio = std::make_unique<mot::Standalone>();
        window = std::make_unique<Window>(*audio);
    }
    void shutdown() override
    {
        window.reset();
        audio.reset();
    }
    void anotherInstanceStarted(const juce::String&) override
    {
        if (window != nullptr)
            window->toFront(true);
    }

private:
    std::unique_ptr<mot::Standalone> audio;
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION(Application)
