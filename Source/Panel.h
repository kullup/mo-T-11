#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mot
{
class Standalone;

class Panel final : public juce::Component
{
public:
    static constexpr int designWidth = 1440, designHeight = 684;
    explicit Panel(Standalone& app);
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
