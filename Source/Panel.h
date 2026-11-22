#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace mot
{
class Standalone;

class Panel final : public juce::Component
{
public:
    static constexpr int designWidth = 1408, designHeight = 800, compactHeight = 676;
    Panel(Standalone& app, std::function<void(int)> onDesignHeightChanged);
    ~Panel() override;
    bool playComputerKey(const juce::KeyPress&);
    bool releaseComputerKeys();
    void releasePerformanceKeys();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Surface;
    std::unique_ptr<Surface> surface;
};
}
