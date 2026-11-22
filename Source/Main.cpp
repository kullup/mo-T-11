#include "Panel.h"
#include "Standalone.h"

namespace
{
class Window final : public juce::DocumentWindow, private juce::AsyncUpdater
{
public:
    explicit Window(mot::Standalone& audio)
        : DocumentWindow("MO-T 11", juce::Colour(0xff252829), allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new mot::Panel(audio, [this](int height) { updatePanelHeight(height); }), true);
        setResizable(true, false);
        setPanelAspectRatio(mot::Panel::designHeight);
        grip = std::make_unique<juce::ResizableCornerComponent>(this, getConstrainer());
        grip->setAlwaysOnTop(true);
        addAndMakeVisible(*grip);
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        int width = mot::Panel::designWidth;
        if (display != nullptr)
            width = juce::jmax(minWidth, juce::jmin(width, display->userArea.getWidth() - 60,
                (display->userArea.getHeight() - 90) * mot::Panel::designWidth / mot::Panel::designHeight));
        centreWithSize(width, heightForWidth(width, mot::Panel::designHeight));
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
        const bool maximized = isFullScreen();
        if (grip != nullptr)
        {
            grip->setVisible(!maximized);
            grip->setBounds(getWidth() - 22, getHeight() - 22, 22, 22);
        }
        if (!maximized && pendingPanelHeight != 0)
            triggerAsyncUpdate();
    }

private:
    static constexpr int minWidth = mot::Panel::designWidth * 3 / 4, maxWidth = 2400;

    static int heightForWidth(int width, int designHeight)
    {
        return juce::roundToInt(width * static_cast<double>(designHeight) / mot::Panel::designWidth);
    }

    void setPanelAspectRatio(int designHeight)
    {
        if (auto* constrainer = getConstrainer())
        {
            constrainer->setSizeLimits(minWidth, heightForWidth(minWidth, designHeight),
                                       maxWidth, heightForWidth(maxWidth, designHeight));
            constrainer->setFixedAspectRatio(static_cast<double>(mot::Panel::designWidth) / designHeight);
        }
    }

    void updatePanelHeight(int designHeight)
    {
        if (isFullScreen())
        {
            // Let the native window restore its previous bounds before changing its aspect ratio.
            pendingPanelHeight = designHeight;
            return;
        }

        pendingPanelHeight = 0;
        setPanelAspectRatio(designHeight);
        auto bounds = getBounds();
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(bounds))
        {
            const auto frameSize = getPeer()->getFrameSizeIfPresent();
            const auto frame = frameSize ? *frameSize : juce::BorderSize<int>();
            const auto available = frame.subtractedFrom(display->userArea);
            const int fittingWidth = juce::jmin(available.getWidth(),
                available.getHeight() * mot::Panel::designWidth / designHeight);
            bounds.setWidth(juce::jmin(bounds.getWidth(), juce::jmax(minWidth, fittingWidth)));
            bounds.setHeight(heightForWidth(bounds.getWidth(), designHeight));
            bounds = bounds.constrainedWithin(available);
        }
        else
            bounds.setHeight(heightForWidth(bounds.getWidth(), designHeight));
        setBounds(bounds);
    }

    void handleAsyncUpdate() override
    {
        if (pendingPanelHeight != 0)
            updatePanelHeight(pendingPanelHeight);
    }

    mot::Panel* performancePanel() const noexcept { return static_cast<mot::Panel*>(getContentComponent()); }
    std::unique_ptr<juce::ResizableCornerComponent> grip;
    int pendingPanelHeight = 0;
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
