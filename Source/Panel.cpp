#include "Panel.h"
#include "Standalone.h"
#include <BinaryData.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string_view>

namespace mot
{
namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using juce::String;

const Colour chassis(0xffd7d4ca), plate(0xffe5e2d8), line(0xffa9a79d), ink(0xff282e2d);
const Colour steel(0xfff5f2e8), quiet(0xff58605b), dark(0xff262c2b), bezel(0xff191c18);
const Colour screen(0xff141812), amber(0xfff7b564), mark(0xffa84a25), blue(0xff326675);

juce::Font panelFont(float size, bool bold = false)
{
    static const auto medium = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
    static const auto boldFace = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);
    return juce::Font(juce::FontOptions(bold ? boldFace : medium).withHeight(size)
                          .withKerningFactor(bold ? 0.055f : 0.015f));
}

void label(Graphics& g, const String& text, Rectangle<int> area, float size, Colour colour = ink,
           juce::Justification justify = juce::Justification::centredLeft, bool bold = false)
{
    g.setColour(colour);
    g.setFont(panelFont(size, bold));
    g.drawFittedText(text, area, justify, 1, 0.72f);
}

void pixels(Graphics& g, const String& source, float x, float y, float scale, Colour colour)
{
    static constexpr std::string_view chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-%.> /?*";
    static constexpr unsigned char glyphs[][7] {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
        {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
        {17,17,17,17,10,10,4}, {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}, {0,4,4,31,4,4,0}, {0,0,0,31,0,0,0},
        {25,25,2,4,8,19,19}, {0,0,0,0,0,12,12}, {16,8,4,2,4,8,16},
        {0,0,0,0,0,0,0}, {1,1,2,4,8,16,16}, {14,17,1,2,4,0,4}, {0,21,14,31,14,21,0}
    };
    static_assert(chars.size() == std::size(glyphs));
    g.setColour(colour);
    for (auto character : source.toUpperCase())
    {
        auto index = character < 128 ? chars.find(static_cast<char>(character)) : std::string_view::npos;
        const auto& glyph = glyphs[index == std::string_view::npos ? chars.find('?') : index];
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if ((glyph[row] & (16 >> column)) != 0)
                    g.fillRect(x + column * scale, y + row * scale, scale, scale);
        x += 6 * scale;
    }
}

void display(Graphics& g, Rectangle<int> bounds)
{
    g.setColour(bezel);
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
    g.setColour(Colour(0xff45473a));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 5.0f, 1.0f);
    const auto inner = bounds.reduced(4);
    g.setColour(screen);
    g.fillRoundedRectangle(inner.toFloat(), 3.0f);
    g.setColour(Colour(0x0cbeb078));
    for (int y = inner.getY() + 3; y < inner.getBottom() - 2; y += 3)
        g.drawHorizontalLine(y, static_cast<float>(inner.getX() + 1), static_cast<float>(inner.getRight() - 1));
}

String percent(double value) { return String(juce::roundToInt(value * 100.0)) + "%"; }

String signedValue(int value, const String& suffix = {})
{
    return (value > 0 ? String("+") : String()) + String(value) + suffix;
}

String hz(double value)
{
    return value >= 1000 ? String(value / 1000.0, 2) + " kHz" : String(juce::roundToInt(value)) + " Hz";
}

String seconds(double value)
{
    return value >= 1 ? String(value, 2) + " s" : String(juce::roundToInt(value * 1000.0)) + " ms";
}

class Skin final : public juce::LookAndFeel_V4
{
public:
    juce::Font getPopupMenuFont() override { return panelFont(17.0f); }

    void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float position,
                          float start, float end, juce::Slider& slider) override
    {
        const float diameter = static_cast<float>(std::min(w, h)) - 15.0f;
        const juce::Point<float> centre { x + w * 0.5f, y + h * 0.5f };
        const auto bounds = Rectangle<float>(diameter, diameter).withCentre(centre);
        const bool customFace = slider.isColourSpecified(juce::Slider::thumbColourId);
        const auto face = customFace ? slider.findColour(juce::Slider::thumbColourId) : steel;
        const auto radialPoint = [&](float angle, float distance)
        {
            return centre.getPointOnCircumference(distance, angle);
        };

        for (int i = 0; i <= 10; ++i)
        {
            const float angle = start + (end - start) * static_cast<float>(i) / 10.0f;
            g.setColour(i == 0 || i == 5 || i == 10 ? quiet : line);
            g.drawLine({ radialPoint(angle, diameter * 0.5f + 3), radialPoint(angle, diameter * 0.5f + 6) }, 1.0f);
        }
        g.setColour(Colour(0x290c120f));
        g.fillEllipse(bounds.expanded(1.0f).translated(0.0f, 3.4f));
        g.setColour(customFace ? face.darker(0.42f) : Colour(0xffada99c));
        g.fillEllipse(bounds.translated(0.0f, 1.5f));
        g.setGradientFill({ face, bounds.getTopLeft(), customFace ? face.darker(0.22f) : Colour(0xffdcd8ca),
                            bounds.getBottomRight(), false });
        g.fillEllipse(bounds);
        g.setColour(customFace ? face.brighter(0.35f) : Colour(0xfffcfaf2));
        g.drawEllipse(bounds.reduced(0.75f), 1.15f);
        g.setColour(slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true) ? blue : quiet.withAlpha(0.22f));
        g.drawEllipse(bounds, slider.hasKeyboardFocus(true) ? 1.65f : 1.0f);
        const float angle = start + position * (end - start);
        const auto indicator = customFace ? steel : mark;
        g.setColour(slider.isEnabled() ? indicator : quiet.withAlpha(0.48f));
        g.drawLine({ radialPoint(angle, diameter * 0.14f), radialPoint(angle, diameter * 0.39f) }, diameter > 70 ? 3.3f : 2.35f);
        g.fillEllipse(Rectangle<float>(3.0f, 3.0f).withCentre(centre));
    }

    void drawLinearSlider(Graphics& g, int x, int y, int w, int h, float position, float, float,
                          juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const float cx = x + w * 0.5f;
        const Rectangle<float> track(cx - 3.0f, static_cast<float>(y), 6.0f, static_cast<float>(h));
        for (int tick = 0; tick <= 8; ++tick)
        {
            const float ypos = y + static_cast<float>(h * tick) / 8.0f;
            g.setColour(tick == 4 ? quiet : line);
            g.drawLine(cx - 11.0f, ypos, cx - 7.0f, ypos, 1.0f);
            g.drawLine(cx + 7.0f, ypos, cx + 11.0f, ypos, 1.0f);
        }
        g.setColour(Colour(0xff353b38));
        g.fillRoundedRectangle(track, 3.0f);
        const float reference = slider.getMinimum() < 0 ? y + h * 0.5f : static_cast<float>(y + h);
        g.setColour(Colour(0xffb57148));
        g.fillRect(cx - 1.0f, std::min(reference, position), 2.0f, std::abs(reference - position));
        auto cap = Rectangle<float>(static_cast<float>(std::min(w - 4, 25)), 15.0f).withCentre({ cx, position });
        g.setColour(Colour(0x410c120f));
        g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 3.5f);
        g.setColour(steel);
        g.fillRoundedRectangle(cap, 3.5f);
        g.setColour(slider.hasKeyboardFocus(true) ? blue : line.darker(0.2f));
        g.drawRoundedRectangle(cap, 3.5f, 1.0f);
        g.setColour(mark);
        g.drawLine(cap.getX() + 5.0f, position, cap.getRight() - 5.0f, position, 1.6f);
    }

    int getSliderThumbRadius(juce::Slider&) override { return 10; }

    void drawButtonBackground(Graphics& g, juce::Button& button, const Colour& colour,
                              bool highlighted, bool down) override
    {
        const auto area = button.getLocalBounds().toFloat().reduced(0.75f).withTrimmedBottom(2.0f);
        if (colour.isTransparent())
        {
            if (highlighted || button.hasKeyboardFocus(true))
            {
                g.setColour(amber.withAlpha(button.hasKeyboardFocus(true) ? 0.9f : 0.55f));
                g.drawRoundedRectangle(area, 4.0f, 1.0f);
            }
            return;
        }
        g.setColour(Colour(0x350e1510));
        g.fillRoundedRectangle(area.translated(0.0f, 2.0f), 3.5f);
        g.setColour(down ? colour.darker(0.14f) : highlighted ? colour.brighter(0.08f) : colour);
        g.fillRoundedRectangle(area.translated(0.0f, down ? 1.2f : 0.0f), 3.5f);
        g.setColour(button.hasKeyboardFocus(true) ? amber : colour.contrasting(0.22f));
        g.drawRoundedRectangle(area, 3.5f, button.hasKeyboardFocus(true) ? 1.65f : 0.85f);
    }

    void drawButtonText(Graphics& g, juce::TextButton& button, bool, bool down) override
    {
        const auto text = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                  : juce::TextButton::textColourOffId);
        label(g, button.getButtonText(), button.getLocalBounds().reduced(4, 2).translated(0, down ? 1 : -1),
              button.getHeight() > 32 ? 14.0f : 12.0f, text, juce::Justification::centred, true);
    }
};

class MatrixTargetButton final : public juce::Button
{
public:
    MatrixTargetButton() : Button("Matrix target")
    {
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void show(int column, Destination nextTarget, bool selected)
    {
        target = nextTarget;
        const auto title = "Matrix target " + String(column + 1) + ", " + destinationNames[target];
        setTitle(title);
        setTooltip(title + ". Click to change.");
        setToggleState(selected, juce::dontSendNotification);
        repaint();
    }

    void paintButton(Graphics& g, bool over, bool down) override
    {
        static constexpr const char* legends[destinationCount][2] {
            { "POS", "A" }, { "POS", "B" }, { "PCH", "A" }, { "PCH", "B" }, { "CUT", "" },
            { "RES", "" }, { "PAN", "" }, { "POS", "C" }, { "PCH", "C" },
            { "LVL", "A" }, { "LVL", "B" }, { "LVL", "C" },
            { "PAN", "A" }, { "PAN", "B" }, { "PAN", "C" }, { "DRV", "" }, { "AMP", "" }
        };
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const bool focused = hasKeyboardFocus(true);
        const bool lit = getToggleState() || over || focused;
        g.setColour(amber.withAlpha(down ? 0.26f : lit ? 0.15f : 0.05f));
        g.fillRoundedRectangle(bounds, 2.0f);
        if (lit)
        {
            g.setColour(amber.withAlpha(over || focused ? 0.85f : 0.55f));
            g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
        }

        const auto draw = [&](const String& text, float y)
        {
            const float width = text.length() * 6.0f - 1.0f;
            pixels(g, text, std::floor((getWidth() - width) * 0.5f), y, 1.0f,
                   amber.withAlpha(lit ? 1.0f : 0.77f));
        };
        const auto& legend = legends[target];
        const bool twoLines = legend[1][0] != '\0';
        draw(legend[0], twoLines ? 4.0f : 9.0f);
        if (twoLines)
            draw(legend[1], 14.0f);
    }

private:
    Destination target = positionA;
};

class MatrixCell final : public juce::Button
{
public:
    MatrixCell() : Button("Matrix route")
    {
        setWantsKeyboardFocus(true);
    }

    void show(const String& name, float nextAmount, bool nextSelected)
    {
        amount = nextAmount;
        setTitle(name);
        setToggleState(nextSelected, juce::dontSendNotification);
        setTooltip(name + "  " + signedValue(juce::roundToInt(amount * 100.0f), "%"));
        repaint();
    }

    void paintButton(Graphics& g, bool over, bool down) override
    {
        const auto face = getLocalBounds().toFloat().reduced(1.0f).withTrimmedBottom(2.5f);
        const bool selected = getToggleState();
        const auto lit = amount < 0 ? blue : mark;
        g.setColour(Colour(0xffa39e90));
        g.fillRoundedRectangle(face.translated(0.0f, 2.0f), 4.0f);
        g.setColour(down ? steel.darker(0.12f) : over ? steel.interpolatedWith(amber, 0.21f) : steel);
        g.fillRoundedRectangle(face, 4.0f);
        g.setColour(selected ? ink : line);
        g.drawRoundedRectangle(face, 4.0f, selected ? 1.8f : 0.8f);
        if (selected || amount != 0)
        {
            const auto lamp = Rectangle<float>(10.0f, 4.0f).withCentre(face.getCentre());
            g.setColour(amount != 0 ? lit : line);
            g.fillRoundedRectangle(lamp, 1.1f);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(blue);
            g.drawRoundedRectangle(face.reduced(2.8f), 2.25f, 1.0f);
        }
    }

private:
    float amount = 0;
};

class PerformanceKeys final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PerformanceKeys(Standalone& instrument) : audio(instrument)
    {
        setTitle("Audition keyboard");
        setDescription("Click or drag across keys, or play A W S E D F T G Y H U J K in the synth window.");
        setTooltip(getDescription());
        setWantsKeyboardFocus(true);
    }

    void setOctave(int next)
    {
        release();
        octave = next;
        repaint();
    }

    void release()
    {
        holdMouse(-1);
        for (unsigned i = 0; i < computer.size(); ++i)
            setComputer(i, false);
    }

    void paint(Graphics& g) override
    {
        for (int i = 0; i < noteCount; ++i)
            if (!isBlack(i)) drawKey(g, i);
        for (int i = 0; i < noteCount; ++i)
            if (isBlack(i)) drawKey(g, i);
        if (hasKeyboardFocus(false))
        {
            g.setColour(amber);
            g.drawRect(getLocalBounds().reduced(1), 1);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { grabKeyboardFocus(); holdMouse(noteAt(e.position)); }
    void mouseDrag(const juce::MouseEvent& e) override { holdMouse(noteAt(e.position)); }
    void mouseUp(const juce::MouseEvent&) override { holdMouse(-1); }
    void visibilityChanged() override { if (!isVisible()) holdMouse(-1); }
    void focusLost(FocusChangeType) override { repaint(); }
    void focusGained(FocusChangeType) override { repaint(); }

    bool playComputerKey(const juce::KeyPress& press)
    {
        if (press.getKeyCode() == juce::KeyPress::escapeKey)
        {
            release();
            audio.panic();
            return true;
        }
        const auto character = press.getTextCharacter() != 0 ? press.getTextCharacter() : press.getKeyCode();
        const auto ascii = juce::CharacterFunctions::toLowerCase(character);
        const auto index = ascii < 128 ? mapping.find(static_cast<char>(ascii)) : std::string_view::npos;
        if (index == std::string_view::npos || press.getModifiers().isCommandDown() || press.getModifiers().isAltDown())
            return false;
        setComputer(static_cast<unsigned>(index), true);
        return true;
    }

    bool releaseComputerKeys()
    {
        bool changed = false;
        for (unsigned i = 0; i < computer.size(); ++i)
            if (computer[i] && !juce::KeyPress::isKeyCurrentlyDown(juce::CharacterFunctions::toUpperCase(mapping[i])))
            {
                setComputer(i, false);
                changed = true;
            }
        return changed;
    }

private:
    static constexpr int noteCount = 25;
    static constexpr std::string_view mapping = "awsedftgyhujk";
    static bool isBlack(int note) { return (0x54a & (1 << (note % 12))) != 0; }
    int midi(int note) const { return (octave + 1) * 12 + note; }

    Rectangle<float> keyArea(int note) const
    {
        const float step = getWidth() / 15.0f;
        int white = 0;
        for (int n = 0; n < note; ++n)
            if (!isBlack(n)) ++white;
        if (isBlack(note))
            return { step * (white - 0.31f), 0, step * 0.62f, getHeight() * 0.61f };
        return { step * white, 0, step, static_cast<float>(getHeight()) };
    }

    int noteAt(juce::Point<float> point) const
    {
        for (int i = 0; i < noteCount; ++i)
            if (isBlack(i) && keyArea(i).contains(point)) return i;
        for (int i = 0; i < noteCount; ++i)
            if (!isBlack(i) && keyArea(i).contains(point)) return i;
        return -1;
    }

    bool computerHeld(int note) const { return note >= 0 && note < static_cast<int>(computer.size()) && computer[note]; }

    void holdMouse(int next)
    {
        if (mouseNote == next) return;
        const auto play = [this](int note, bool pressed)
        {
            if (note >= 0 && !computerHeld(note))
                audio.play(midi(note), pressed);
        };
        const bool poly = audio.patch().sound.voicing.mode == PlayMode::poly;
        // A mono drag overlaps the keys so the gate stays open across the transition.
        if (poly) play(mouseNote, false);
        play(next, true);
        if (!poly) play(mouseNote, false);
        mouseNote = next;
        repaint();
    }

    void setComputer(unsigned note, bool next)
    {
        if (computer[note] == next) return;
        computer[note] = next;
        if (mouseNote != static_cast<int>(note)) audio.play(midi(static_cast<int>(note)), next);
        repaint();
    }

    void drawKey(Graphics& g, int note) const
    {
        const bool down = note == mouseNote || computerHeld(note);
        const bool black = isBlack(note);
        const auto area = keyArea(note).reduced(0.7f, 0.0f);
        g.setColour(down ? amber : black ? Colour(0xff141a19) : Colour(0xffb6b5aa));
        g.fillRoundedRectangle(area, 3.0f);
        g.setColour(down ? amber : black ? dark : steel);
        g.fillRoundedRectangle(area.withTrimmedBottom(black ? 4.0f : 3.0f), 2.75f);
        if (note % 12 == 0)
            label(g, "C" + String(octave + note / 12), area.getSmallestIntegerContainer().removeFromBottom(19),
                  11.5f, quiet, juce::Justification::centred, true);
    }

    Standalone& audio;
    int octave = 4, mouseNote = -1;
    std::array<bool, mapping.size()> computer {};
};
}

class Panel::Surface final : public juce::Component, private juce::Timer
{
public:
    Surface(Standalone& instrument, std::function<void(int)> onDesignHeightChanged)
        : audio(instrument), designHeightChanged(std::move(onDesignHeightChanged)), keys(audio)
    {
        setLookAndFeel(&skin);
        setSize(designWidth, designHeight);
        createHeader();
        createOscillators();
        createFilter();
        createEnvelopes();
        createLfos();
        createMatrix();
        createVoicing();
        createKeyboard();
        syncControls();

        const auto& presets = factoryPresets();
        for (int slot = 0; slot < presetCount; ++slot)
            if (encodePatch(audio.patch()) == encodePatch(presets[slot]))
                presetSlot = slot;
        edited = presetSlot < 0;
        startTimerHz(25);
    }

    ~Surface() override
    {
        stopTimer();
        keys.release();
        setLookAndFeel(nullptr);
    }

    bool playComputerKey(const juce::KeyPress& press) { return keys.playComputerKey(press); }
    bool releaseComputerKeys() { return keys.releaseComputerKeys(); }
    void releasePerformanceKeys() { keys.release(); }

    void paint(Graphics& g) override
    {
        g.fillAll(chassis);
        g.setColour(Colour(0xffeeebe0));
        g.drawRect(getLocalBounds().reduced(6), 1);
        g.setColour(line);
        g.drawRect(getLocalBounds().reduced(7), 1);
        paintHeader(g);
        paintOscillators(g);
        paintPlate(g, filterPanel, "FILTER");
        paintPlate(g, envPanel, "ENVELOPES");
        paintPlate(g, lfoPanel, "LFO");
        paintPlate(g, matrixPanel, "MOD MATRIX");
        paintLfoAndEnvelope(g);
        paintMatrix(g);
        paintPlate(g, voicingPanel, "VOICING");
        label(g, "PLAY MODE", { 36, 583, 246, 15 }, 11.5f, quiet, juce::Justification::centredLeft, true);
        paintPlate(g, effectsPanel, {});
        if (keys.isVisible()) paintPerformance(g);
        for (const auto& entry : sliders)
        {
            if (entry.caption.isEmpty() || !entry.control->isVisible()) continue;
            const auto area = entry.control->getBounds();
            const auto width = std::max(area.getWidth(), entry.fader ? 41 : 58);
            const int centre = area.getCentreX();
            const int top = area.getBottom() + 1;
            label(g, entry.caption, { centre - width / 2, top, width, 15 }, 12.0f, ink,
                  juce::Justification::centred, true);
            label(g, entry.control->getTextFromValue(entry.control->getValue()),
                  { centre - width / 2, top + 14, width, 14 }, 11.5f, quiet, juce::Justification::centred);
        }
    }

private:
    using Read = std::function<double(const Patch&)>;
    using Write = std::function<void(Patch&, double)>;
    using Format = std::function<String(double)>;
    struct BoundSlider
    {
        std::unique_ptr<juce::Slider> control;
        String caption;
        Read read;
        bool fader;
    };

    inline static const Rectangle<int> oscPanel { 22, 111, 475, 425 };
    inline static const Rectangle<int> filterPanel { 507, 111, 138, 425 };
    inline static const Rectangle<int> envPanel { 655, 111, 210, 425 };
    inline static const Rectangle<int> lfoPanel { 875, 111, 169, 425 };
    inline static const Rectangle<int> matrixPanel { 1054, 111, 364, 425 };
    inline static const Rectangle<int> voicingPanel { 22, 550, 623, 104 };
    inline static const Rectangle<int> effectsPanel { 655, 550, 763, 104 };
    inline static const int compactHeight = 676;
    inline static const Rectangle<int> patchDisplay { 492, 35, 278, 48 };
    inline static const std::array<const char*, oscillatorCount> oscillatorNames { "A", "B", "C" };
    enum OscillatorColumn { positionColumn, tuneColumn, fineColumn, volumeColumn };
    inline static const std::array<const char*, 4> oscillatorColumns { "POS", "TUNE", "FINE", "VOL" };
    inline static const Rectangle<int> matrixDisplay { 1069, 156, 334, 88 };
    inline static const Rectangle<int> performanceStatus { 117, 683, 267, 84 };
    inline static const Rectangle<int> headerNotice { 839, 78, 554, 15 };
    inline static const std::array<const char*, 3> modeNames { "POLY", "MONO", "LEGATO" };

    static int oscillatorColumnX(int column) { return 218 + column * 72; }
    static Rectangle<int> oscillatorDisplay(int row) { return { 36, 169 + row * 100, 146, 92 }; }
    static Rectangle<int> oscillatorKnob(int row, int column)
    {
        const Rectangle<int> area { oscillatorColumnX(column) - 34, oscillatorDisplay(row).getY() + 4, 68, 68 };
        return area.reduced(column == fineColumn ? 5 : 0, 0);
    }
    static int matrixColumnX(int column) { return 1139 + column * 23; }

    juce::TextButton& button(const String& caption, const String& name, Rectangle<int> area,
                             std::function<void()> action, bool onHeader = false)
    {
        auto control = std::make_unique<juce::TextButton>(caption);
        auto& b = *control;
        b.setTitle(name);
        b.setTooltip(name);
        b.setWantsKeyboardFocus(true);
        b.setColour(juce::TextButton::buttonColourId, onHeader ? Colour(0xff3b4643) : steel);
        b.setColour(juce::TextButton::buttonOnColourId, blue);
        b.setColour(juce::TextButton::textColourOffId, onHeader ? steel : ink);
        b.setColour(juce::TextButton::textColourOnId, steel);
        b.onClick = std::move(action);
        addAndMakeVisible(b);
        b.setBounds(area);
        buttons.push_back(std::move(control));
        return b;
    }

    juce::Slider& slider(const String& name, const String& caption, Rectangle<int> area, bool fader,
                         double min, double max, double interval, Read read, Write write, Format format,
                         double midpoint = 0.0)
    {
        auto control = std::make_unique<juce::Slider>();
        auto& s = *control;
        s.setName(name);
        s.setTitle(name);
        s.setTooltip(name);
        s.setWantsKeyboardFocus(true);
        s.setSliderStyle(fader ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(min, max, interval);
        if (midpoint > min && midpoint < max) s.setSkewFactorFromMidPoint(midpoint);
        s.setDoubleClickReturnValue(true, read(factoryPresets().front().sound));
        s.setMouseDragSensitivity(165);
        s.setRotaryParameters(3.14159265f * 1.24f, 3.14159265f * 2.76f, true);
        s.textFromValueFunction = std::move(format);
        s.onValueChange = [this, &s, write = std::move(write)]
        {
            edit([&](Patch& patch) { write(patch, s.getValue()); });
            s.setTooltip(s.getName() + "  " + s.getTextFromValue(s.getValue()));
        };
        addAndMakeVisible(s);
        s.setBounds(area);
        sliders.push_back({ std::move(control), caption, std::move(read), fader });
        return s;
    }

    template <typename T, typename F>
    juce::Slider& oscSlider(T Oscillator::* member, F select, const String& name, Rectangle<int> area,
                           double min, double max, double interval, Format format)
    {
        return slider(name, {}, area, false, min, max, interval,
                      [=](const Patch& p) { return p.osc[select()].*member; },
                      [=](Patch& p, double v) { p.osc[select()].*member = static_cast<T>(v); }, std::move(format));
    }

    template <typename T>
    void edit(T change)
    {
        auto next = audio.patch();
        change(next.sound);
        audio.apply(std::move(next));
        edited = true;
        syncControls();
    }

    void syncControls()
    {
        const auto& p = audio.patch().sound;
        const auto& osc = p.osc[engine];
        for (const auto& entry : sliders)
        {
            auto& control = *entry.control;
            const auto amount = entry.read(p);
            control.setValue(amount, juce::dontSendNotification);
            control.setTooltip(control.getName() + "  " + control.getTextFromValue(amount));
            control.setDoubleClickReturnValue(true, entry.read(factoryPresets().front().sound));
        }
        for (int i = 0; i < oscillatorCount; ++i)
        {
            engineButtons[i]->setToggleState(engine == i, juce::dontSendNotification);
            const auto& current = p.osc[i];
            const String title = "Oscillator " + String(oscillatorNames[i]) + " wavetable: " + tableNames[current.table];
            tableButtons[i]->setTitle(title);
            tableButtons[i]->setTooltip(title + ". Click to choose.");
            oscillatorKnobs[i][positionColumn]->setEnabled(!isFixedTable(current.table));
        }
        viewButton->setButtonText(stacked[engine] ? "STACK" : "CYCLE");
        phaseButton->setButtonText(osc.resetPhase ? "RESET" : "FREE");
        phaseButton->setToggleState(osc.resetPhase, juce::dontSendNotification);
        for (std::size_t i = 0; i < modeButtons.size(); ++i)
            modeButtons[i]->setToggleState(static_cast<PlayMode>(i) == p.voicing.mode, juce::dontSendNotification);
        static constexpr const char* modes[] { "LP  /  LOW PASS", "BP  /  BAND PASS", "HP  /  HIGH PASS" };
        filterMode->setButtonText(modes[static_cast<int>(p.filterMode)]);
        static constexpr const char* shapes[] { "SINE", "TRI", "RAMP", "SQR" };
        for (int i = 0; i < 2; ++i)
        {
            shapeButtons[i]->setButtonText(shapes[static_cast<int>(p.lfos[i].shape)]);
            lfoModeButtons[i]->setButtonText(p.lfos[i].restart ? "KEY" : "FREE");
            lfoModeButtons[i]->setToggleState(p.lfos[i].restart, juce::dontSendNotification);
        }
        for (int col = 0; col < matrixColumnCount; ++col)
        {
            const auto target = p.matrixTargets[col];
            matrixTargets[col].show(col, target, matrixCol == col);
            for (int row = 0; row < sourceCount; ++row)
                matrix[row][col]->show(String(sourceNames[row]) + " to " + destinationNames[target],
                                       p.matrix[row][col], matrixRow == row && matrixCol == col);
        }
        presetMenu->setTitle("Factory presets. Current patch " + String::fromUTF8(audio.patch().name.c_str()));
        repaint();
    }

    void createHeader()
    {
        performanceButton = &button("HIDE KEYS", "Hide performance panel", { 330, 44, 102, 30 },
                                    [this] { togglePerformance(); }, true);
        button("<", "Previous factory patch", { 448, 44, 36, 30 }, [this] { stepPreset(-1); }, true);
        button(">", "Next factory patch", { 778, 44, 36, 30 }, [this] { stepPreset(1); }, true);
        auto& presets = button({}, "Factory presets", patchDisplay, [this] { showPresets(); }, true);
        presets.setColour(juce::TextButton::buttonColourId, Colour(0x00000000));
        presetMenu = &presets;
        button("OPEN", "Open patch file", { 839, 44, 65, 30 }, [this] { chooseFile(false); }, true);
        button("SAVE", "Save patch file", { 912, 44, 65, 30 }, [this] { chooseFile(true); }, true);
        audioButton = &button("AUDIO", "Select audio output", { 1004, 44, 75, 30 }, [this] { showOutputs(); }, true);
        auto& panicButton = button("PANIC", "Stop all notes", { 1307, 44, 86, 30 }, [this]
        {
            keys.release();
            audio.panic();
            notice("All notes cleared");
        }, true);
        panicButton.setColour(juce::TextButton::buttonColourId, Colour(0xff693a29));
    }

    void createOscillators()
    {
        for (int i = 0; i < oscillatorCount; ++i)
        {
            const String tag(oscillatorNames[i]);
            const String name = "Oscillator " + tag;
            const auto row = [i] { return i; };
            auto& table = button({}, name + " wavetable", oscillatorDisplay(i), [this, i] { showTables(i); });
            table.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            tableButtons[i] = &table;

            auto& knobs = oscillatorKnobs[i];
            knobs[positionColumn] = &oscSlider(&Oscillator::position, row, name + " wavetable position",
                                               oscillatorKnob(i, positionColumn), 0, 1, 0.001, [this, i](double v)
            {
                const auto selectedTable = audio.patch().sound.osc[i].table;
                if (isFixedTable(selectedTable)) return String("FIXED");
                return percent(selectedTable == pulseTable ? pulseDuty(static_cast<float>(v)) : v);
            });
            knobs[tuneColumn] = &oscSlider(&Oscillator::semitone, row, name + " tune in semitones",
                                           oscillatorKnob(i, tuneColumn), -maxOscillatorSemitones, maxOscillatorSemitones,
                                           1, [](double v) { return signedValue(juce::roundToInt(v), " st"); });
            knobs[fineColumn] = &oscSlider(&Oscillator::fine, row, name + " fine tune in cents",
                                           oscillatorKnob(i, fineColumn), -50, 50, 0.1, [](double v)
            {
                return String(v > 0.0 ? "+" : "") + String(v, 1) + " ct";
            });
            auto& volume = oscSlider(&Oscillator::level, row, name + " volume",
                                     oscillatorKnob(i, volumeColumn), 0, 1, 0.001, percent);
            volume.setColour(juce::Slider::thumbColourId, mark);
            knobs[volumeColumn] = &volume;

            engineButtons[i] = &button(tag, name + " setup", { 38 + 32 * i, 498, 28, 23 }, [this, i]
            {
                engine = i;
                syncControls();
            });
        }

        phaseButton = &button("RESET", "Toggle note-triggered or free oscillator phase", { 142, 498, 53, 23 }, [this]
        {
            edit([&](Patch& p) { p.osc[engine].resetPhase = !p.osc[engine].resetPhase; });
        });
        viewButton = &button("STACK", "Toggle stacked or single-cycle waveform view", { 209, 498, 53, 23 }, [this]
        {
            stacked[engine] = !stacked[engine];
            syncControls();
        });

        const auto selected = [this] { return engine; };
        setupKnobs[0] = &oscSlider(&Oscillator::octave, selected, "Oscillator octave", { 282, 475, 41, 37 },
                                   -2, 2, 1, [](double v) { return signedValue(juce::roundToInt(v)); });
        setupKnobs[1] = &oscSlider(&Oscillator::pan, selected, "Oscillator stereo pan", { 351, 475, 41, 37 },
                                   -1, 1, 0.01, [](double v)
        {
            if (std::abs(v) < 0.005) return String("C");
            return String(v < 0 ? "L " : "R ") + percent(std::abs(v));
        });
        setupKnobs[2] = &oscSlider(&Oscillator::phase, selected, "Oscillator start phase in reset mode", { 420, 475, 41, 37 },
                                   0, 1, 0.005, [](double v) { return String(juce::roundToInt(v * 360)); });
    }

    void createFilter()
    {
        filterMode = &button("LP", "Cycle filter mode", { 519, 150, 114, 26 }, [this]
        {
            edit([](Patch& p) { p.filterMode = static_cast<FilterMode>((static_cast<int>(p.filterMode) + 1) % 3); });
        });
        slider("Filter cutoff", "CUTOFF", { 524, 187, 104, 104 }, false, 20, 20000, 1,
               [](const Patch& p) { return p.cutoff; }, [](Patch& p, double v) { p.cutoff = static_cast<float>(v); }, hz, 1200);
        slider("Filter resonance", "RESO", { 545, 326, 62, 62 }, false, 0, 1, 0.001,
               [](const Patch& p) { return p.resonance; }, [](Patch& p, double v) { p.resonance = static_cast<float>(v); }, percent);
        slider("Filter drive", "DRIVE", { 545, 433, 62, 62 }, false, 0, 1, 0.001,
               [](const Patch& p) { return p.drive; }, [](Patch& p, double v) { p.drive = static_cast<float>(v); }, percent);
    }

    void createEnvelopes()
    {
        static constexpr float Envelope::* fields[] { &Envelope::attack, &Envelope::decay, &Envelope::sustain, &Envelope::release };
        static constexpr const char* captions[] { "A", "D", "S", "R" };
        static constexpr const char* names[] { "attack", "decay", "sustain", "release" };
        for (int row = 0; row < 2; ++row)
        {
            auto member = row == 0 ? &Patch::filterEnv : &Patch::ampEnv;
            const String name = row == 0 ? "Filter envelope " : "Amp envelope ";
            for (int col = 0; col < 4; ++col)
            {
                auto field = fields[col];
                const bool sustain = col == 2;
                slider(name + names[col], captions[col], { 674 + 47 * col, 196 + 185 * row, 31, 98 },
                       true, sustain ? 0 : 0.002, sustain ? 1 : col == 3 ? 8 : 6, 0.001,
                       [=](const Patch& p) { return (p.*member).*field; },
                       [=](Patch& p, double v) { (p.*member).*field = static_cast<float>(v); },
                       sustain ? percent : seconds, sustain ? 0.0 : 0.25);
            }
        }
    }

    void createLfos()
    {
        for (int i = 0; i < 2; ++i)
        {
            const int dy = 184 * i;
            const String name = "LFO " + String(i + 1);
            slider(name + " rate", "RATE", { 890, 190 + dy, 61, 61 }, false, 0.05, 20, 0.01,
                   [i](const Patch& p) { return p.lfos[i].rate; },
                   [i](Patch& p, double v) { p.lfos[i].rate = static_cast<float>(v); },
                   [](double v) { return String(v, 2) + " Hz"; }, 1.5);
            slider(name + " depth", "DEPTH", { 967, 190 + dy, 61, 61 }, false, 0, 1, 0.001,
                   [i](const Patch& p) { return p.lfos[i].depth; },
                   [i](Patch& p, double v) { p.lfos[i].depth = static_cast<float>(v); }, percent);
            shapeButtons[i] = &button("SINE", name + " waveform", { 888, 291 + dy, 62, 27 }, [this, i]
            {
                edit([i](Patch& p) { p.lfos[i].shape = static_cast<Shape>((static_cast<int>(p.lfos[i].shape) + 1) % 4); });
            });
            lfoModeButtons[i] = &button("FREE", name + " free-running or note restart", { 965, 291 + dy, 64, 27 }, [this, i]
            {
                edit([i](Patch& p) { p.lfos[i].restart = !p.lfos[i].restart; });
            });
        }
    }

    void createMatrix()
    {
        for (int col = 0; col < matrixColumnCount; ++col)
        {
            auto& target = matrixTargets[col];
            target.setBounds(matrixColumnX(col), 215, 22, 25);
            target.onClick = [this, col] { showMatrixTargets(col); };
            addAndMakeVisible(target);
        }
        for (int row = 0; row < sourceCount; ++row)
            for (int col = 0; col < matrixColumnCount; ++col)
            {
                auto cell = std::make_unique<MatrixCell>();
                cell->onClick = [this, row, col]
                {
                    matrixRow = row;
                    matrixCol = col;
                    syncControls();
                };
                addAndMakeVisible(*cell);
                cell->setBounds(matrixColumnX(col), 248 + row * 32, 22, 26);
                matrix[row][col] = std::move(cell);
            }
        const auto read = [this](const Patch& p) { return p.matrix[matrixRow][matrixCol]; };
        const auto write = [this](Patch& p, double v) { p.matrix[matrixRow][matrixCol] = static_cast<float>(v); };
        const auto format = [](double v) { return signedValue(juce::roundToInt(v * 100), "%"); };
        slider("Matrix amount fader", {}, { 1359, 267, 34, 215 }, true, -1, 1, 0.01, read, write, format);
        button("CLEAR", "Clear selected matrix route", { 1347, 493, 56, 26 }, [this, write]
        {
            edit([&](Patch& p) { write(p, 0.0); });
        });
    }

    void showMatrixTargets(int col)
    {
        matrixCol = col;
        syncControls();
        const auto& targets = audio.patch().sound.matrixTargets;
        juce::PopupMenu menu;
        menu.setLookAndFeel(&skin);
        const auto addTarget = [&](Destination target)
        {
            const bool selected = target == targets[col];
            const bool available = std::find(targets.begin(), targets.end(), target) == targets.end();
            menu.addItem(target + 1, destinationNames[target], selected || available, selected);
        };
        menu.addSectionHeader("Oscillators");
        for (const auto& group : { positionDestinations, pitchDestinations, levelDestinations, panDestinations })
            for (auto target : group)
                addTarget(target);
        menu.addSectionHeader("Filter");
        for (auto target : { cutoff, resonance, filterDrive })
            addTarget(target);
        menu.addSectionHeader("Amplifier");
        addTarget(ampGain);
        addTarget(pan);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&matrixTargets[col]).withMaximumNumColumns(1),
            [safe = SafePointer<Surface>(this), col](int id)
        {
            if (safe == nullptr || id <= 0)
                return;
            const auto target = static_cast<Destination>(id - 1);
            const auto& current = safe->audio.patch().sound.matrixTargets;
            if (std::find(current.begin(), current.end(), target) == current.end())
                safe->edit([col, target](Patch& p) { p.matrixTargets[col] = target; });
        });
    }

    void createVoicing()
    {
        const std::array<const char*, 3> descriptions {
            "Polyphonic play", "Mono with envelope retrigger", "Mono legato with continuous envelopes"
        };
        for (std::size_t i = 0; i < modeButtons.size(); ++i)
        {
            const auto mode = static_cast<PlayMode>(i);
            modeButtons[i] = &button(modeNames[i], descriptions[i], { 36 + 86 * static_cast<int>(i), 601, 77, 31 },
                                     [this, mode]
            {
                if (audio.patch().sound.voicing.mode != mode)
                    edit([mode](Patch& p) { p.voicing.mode = mode; });
            });
        }
        slider("Glide time between overlapping Mono or Legato notes", "GLIDE", { 317, 558, 62, 62 },
               false, 0, maxGlide, 0.001,
               [](const Patch& p) { return p.voicing.glide; },
               [](Patch& p, double v) { p.voicing.glide = static_cast<float>(v); },
               [](double v) { return v == 0.0 ? String("Off") : seconds(v); }, 0.15);
        slider("Unison layers per note", "UNISON", { 398, 558, 62, 62 }, false, 1, maxUnison, 1,
               [](const Patch& p) { return p.voicing.unison; },
               [](Patch& p, double v) { p.voicing.unison = static_cast<int>(v); },
               [](double v) { return String(static_cast<int>(v)) + "x"; });
        slider("Unison detune in cents", "DETUNE", { 479, 558, 62, 62 }, false, 0, maxUnisonDetune, 1,
               [](const Patch& p) { return p.voicing.detune; },
               [](Patch& p, double v) { p.voicing.detune = static_cast<float>(v); },
               [](double v) { return String(juce::roundToInt(v)) + " ct"; });
        slider("Unison stereo width", "WIDTH", { 560, 558, 62, 62 }, false, 0, 1, 0.01,
               [](const Patch& p) { return p.voicing.width; },
               [](Patch& p, double v) { p.voicing.width = static_cast<float>(v); }, percent);
    }

    void createKeyboard()
    {
        masterVolume = &slider("Master output volume", "VOLUME", { 42, 681, 62, 62 }, false, 0, 1, 0.001,
                               [](const Patch& p) { return p.volume; },
                               [](Patch& p, double v) { p.volume = static_cast<float>(v); }, percent);
        octaveDown = &button("-", "Lower keyboard octave", { 405, 723, 31, 29 }, [this] { changeOctave(-1); });
        octaveUp = &button("+", "Raise keyboard octave", { 451, 723, 31, 29 }, [this] { changeOctave(1); });
        addAndMakeVisible(keys);
        keys.setBounds(502, 686, 894, 74);
    }

    void togglePerformance()
    {
        const bool show = !keys.isVisible();
        keys.setVisible(show);
        masterVolume->setVisible(show);
        octaveDown->setVisible(show);
        octaveUp->setVisible(show);
        const String action = show ? "Hide performance panel" : "Show performance panel";
        performanceButton->setButtonText(show ? "HIDE KEYS" : "SHOW KEYS");
        performanceButton->setTitle(action);
        performanceButton->setTooltip(action);
        setSize(designWidth, show ? designHeight : compactHeight);
        designHeightChanged(getHeight());
    }

    void changeOctave(int step)
    {
        octave = juce::jlimit(0, 7, octave + step);
        octaveDown->setEnabled(octave > 0);
        octaveUp->setEnabled(octave < 7);
        keys.setOctave(octave);
        repaint();
    }

    void showTables(int row)
    {
        engine = row;
        syncControls();
        const auto current = audio.patch().sound.osc[row].table;
        juce::PopupMenu menu;
        menu.addSectionHeader("Classic analog");
        for (int table = sineTable; table < tableCount; ++table)
            menu.addItem(table + 1, tableNames[table], true, table == current);
        menu.addSeparator();
        menu.addSectionHeader("Morphing tables");
        for (int table = basicShapes; table < sineTable; ++table)
            menu.addItem(table + 1, tableNames[table], true, table == current);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(tableButtons[row]),
            [safe = SafePointer<Surface>(this), row](int id)
        {
            if (safe != nullptr && id > 0 && safe->audio.patch().sound.osc[row].table != id - 1)
                safe->edit([row, id](Patch& p) { p.osc[row].table = id - 1; });
        });
    }

    void stepPreset(int step)
    {
        usePreset(presetSlot < 0 ? (step < 0 ? presetCount - 1 : 0) : (presetSlot + step + presetCount) % presetCount);
    }

    void usePreset(int slot)
    {
        presetSlot = slot;
        edited = false;
        audio.apply(factoryPresets()[slot]);
        syncControls();
    }

    void showPresets()
    {
        keys.release();
        juce::PopupMenu menu;
        for (int i = 0; i < presetCount; ++i)
            menu.addItem(i + 1, String(i + 1).paddedLeft('0', 2) + "   " + String(factoryPresets()[i].name),
                         true, i == presetSlot && !edited);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetMenu),
                           [safe = SafePointer<Surface>(this)](int id) { if (safe && id > 0) safe->usePreset(id - 1); });
    }

    void chooseFile(bool saving)
    {
        keys.release();
        const auto directory = fileDirectory.isDirectory() ? fileDirectory
            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        const auto name = juce::File::createLegalFileName(String::fromUTF8(audio.patch().name.c_str()));
        const auto initial = saving ? directory.getChildFile(name + ".motpatch") : directory;
        chooser = std::make_unique<juce::FileChooser>(saving ? "Save MO-T 11 patch" : "Open MO-T 11 patch",
                                                     initial, "*.motpatch", false);
        const int mode = juce::FileBrowserComponent::canSelectFiles | (saving
            ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
            : juce::FileBrowserComponent::openMode);
        chooser->launchAsync(mode, [safe = SafePointer<Surface>(this), saving](const juce::FileChooser& dialog)
        {
            if (safe == nullptr || dialog.getResults().isEmpty()) return;
            const auto file = dialog.getResult();
            if (saving && !file.hasFileExtension("motpatch"))
            {
                const auto withExtension = file.getSiblingFile(file.getFileName() + ".motpatch");
                if (withExtension.exists())
                {
                    safe->notice("Include .motpatch in the filename to replace the existing patch", true);
                    return;
                }
                safe->finishPatchFile(withExtension, true);
            }
            else
                safe->finishPatchFile(file, saving);
        });
    }

    void finishPatchFile(const juce::File& file, bool saving)
    {
        if (!(saving ? audio.save(file) : audio.load(file)))
        {
            notice(saving ? "Could not save patch" : "Could not open this patch", true);
            return;
        }
        fileDirectory = file.getParentDirectory();
        presetSlot = -1;
        edited = false;
        notice((saving ? "Saved " : "Opened ") + file.getFileName());
        syncControls();
    }

    void showOutputs()
    {
        keys.release();
        auto outputs = audio.outputs();
        juce::PopupMenu menu;
        menu.addSectionHeader("Audio output");
        if (outputs.empty()) menu.addItem(-1, "No devices available", false);
        String lastType;
        juce::PopupMenu submenu;
        for (int i = 0; i < static_cast<int>(outputs.size()); ++i)
        {
            const auto& output = outputs[i];
            if (output.type != lastType)
            {
                if (lastType.isNotEmpty()) menu.addSubMenu(lastType, submenu);
                lastType = output.type;
                submenu = juce::PopupMenu();
            }
            submenu.addItem(i + 1, output.name, true, output.selected);
        }
        if (lastType.isNotEmpty()) menu.addSubMenu(lastType, submenu);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(audioButton),
            [safe = SafePointer<Surface>(this), outputs = std::move(outputs)](int id)
            {
                if (safe == nullptr || id <= 0 || static_cast<std::size_t>(id) > outputs.size()) return;
                const auto error = safe->audio.selectOutput(outputs[static_cast<std::size_t>(id - 1)]);
                if (error.isNotEmpty()) safe->notice("Audio output: " + error, true);
                else safe->notice("Audio output selected");
            });
    }

    void notice(String text, bool error = false)
    {
        message = std::move(text);
        messageIsError = error;
        messageEnd = juce::Time::getMillisecondCounterHiRes() + 4000.0;
        repaint(keys.isVisible() ? performanceStatus : headerNotice);
    }

    void timerCallback() override
    {
        repaint(1100, 35, 185, 55);
        repaint(keys.isVisible() ? performanceStatus : headerNotice);
    }

    void paintPlate(Graphics& g, Rectangle<int> area, const String& heading) const
    {
        g.setColour(Colour(0x190c120f));
        g.fillRoundedRectangle(area.toFloat().translated(0, 2), 6.0f);
        g.setColour(plate);
        g.fillRoundedRectangle(area.toFloat(), 6.0f);
        g.setColour(line);
        g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 6.0f, 1.0f);
        g.setColour(steel);
        g.drawHorizontalLine(area.getY() + 1, static_cast<float>(area.getX() + 6), static_cast<float>(area.getRight() - 6));
        if (heading.isNotEmpty())
            label(g, heading, { area.getX() + 13, area.getY() + 10, area.getWidth() - 26, 20 }, 17.0f, ink,
                  juce::Justification::centredLeft, true);
    }

    void paintHeader(Graphics& g) const
    {
        const Rectangle<float> shell { 22, 19, 1396, 79 };
        g.setColour(Colour(0x430a100d));
        g.fillRoundedRectangle(shell.translated(0, 2), 6.0f);
        g.setColour(dark);
        g.fillRoundedRectangle(shell, 6.0f);
        g.setColour(Colour(0xff505a54));
        g.drawRoundedRectangle(shell.reduced(0.5f), 6.0f, 1.0f);
        g.setColour(amber);
        g.fillRoundedRectangle({ 41, 39, 41, 39 }, 4.0f);
        g.setColour(dark);
        for (int i = 0; i < 5; ++i)
        {
            const int height = 8 + (2 - std::abs(2 - i)) * 6;
            g.fillRect(48 + i * 6, 58 - height / 2, 3, height);
        }
        label(g, "MO-T 11", { 95, 28, 206, 40 }, 36.0f, steel, juce::Justification::centredLeft, true);
        label(g, "WAVETABLE SYNTHESIZER", { 97, 66, 256, 16 }, 12.5f, Colour(0xffd0d0bb),
              juce::Justification::centredLeft, true);

        display(g, patchDisplay);
        const auto id = presetSlot < 0 ? String("USER") : String(presetSlot + 1).paddedLeft('0', 2) + "  FACTORY";
        pixels(g, id + (edited ? "  / EDIT" : ""), 505, 43, 1, amber.withAlpha(0.68f));
        pixels(g, String::fromUTF8(audio.patch().name.c_str()).substring(0, 21), 505, 58, 2, amber);
        if (!keys.isVisible() && juce::Time::getMillisecondCounterHiRes() < messageEnd)
            label(g, message, headerNotice, 12.5f, messageIsError ? amber : steel);

        label(g, "OUT", { 1101, 39, 54, 19 }, 12.0f, steel, juce::Justification::centredLeft, true);
        const auto amount = audio.outputPeak();
        const float db = 20.0f * std::log10(std::max(0.0001f, amount));
        for (int i = 0; i < 13; ++i)
        {
            const bool lit = db > -54.0f + 4.5f * i;
            g.setColour(lit ? (i >= 11 ? amber : Colour(0xffa9b48c)) : Colour(0xff465044));
            g.fillRoundedRectangle({ static_cast<float>(1102 + 13 * i), 61.0f, 9.0f, 9.0f }, 1.5f);
        }
    }

    void paintOscillators(Graphics& g) const
    {
        if (!g.clipRegionIntersects(oscPanel)) return;
        paintPlate(g, oscPanel, "TABLE OSC");
        for (int column = 0; column < static_cast<int>(oscillatorColumns.size()); ++column)
        {
            const int x = oscillatorColumnX(column);
            label(g, oscillatorColumns[column], { x - 31, 143, 62, 20 }, 14.0f, ink,
                  juce::Justification::centred, true);
        }
        for (int row = 0; row < oscillatorCount; ++row)
        {
            const auto area = oscillatorDisplay(row);
            g.setColour(quiet.withAlpha(0.65f));
            g.drawHorizontalLine(oscillatorKnob(row, positionColumn).getCentreY(), static_cast<float>(area.getRight()),
                                 static_cast<float>(oscillatorColumnX(volumeColumn)));
            paintWavetable(g, row, area);
            for (auto* knob : oscillatorKnobs[row])
            {
                const auto bounds = knob->getBounds();
                const Rectangle<int> readout { bounds.getX() + 8, bounds.getBottom() + 1, bounds.getWidth() - 16, 16 };
                g.setColour(plate);
                g.fillRect(readout);
                label(g, knob->getTextFromValue(knob->getValue()), readout, 12.0f,
                      knob->isEnabled() ? ink : quiet, juce::Justification::centred);
            }
        }

        g.setColour(line);
        g.drawHorizontalLine(470, 37.0f, 482.0f);
        label(g, "OSC SETUP", { 38, 477, 90, 18 }, 11.0f, quiet, juce::Justification::centredLeft, true);
        label(g, "PHASE MODE", { 132, 477, 73, 18 }, 11.0f, quiet, juce::Justification::centred, true);
        label(g, "DISPLAY", { 201, 477, 69, 18 }, 11.0f, quiet, juce::Justification::centred, true);
        static constexpr const char* captions[] { "OCT", "PAN", "PHASE" };
        for (int i = 0; i < static_cast<int>(setupKnobs.size()); ++i)
        {
            auto& knob = *setupKnobs[i];
            const auto text = String(captions[i]) + " " + knob.getTextFromValue(knob.getValue());
            label(g, text, { knob.getBounds().getCentreX() - 33, 511, 66, 15 }, 11.0f, quiet, juce::Justification::centred);
        }
    }

    void paintWavetable(Graphics& g, int row, Rectangle<int> area) const
    {
        display(g, area);
        const auto& osc = audio.patch().sound.osc[row];
        const auto x = static_cast<float>(area.getX() + 11), end = static_cast<float>(area.getRight() - 11);
        const auto top = static_cast<float>(area.getY());
        pixels(g, "OSC " + String(oscillatorNames[row]), x, top + 11, 1, amber.withAlpha(row == engine ? 1.0f : 0.65f));
        pixels(g, tableNames[osc.table], x, top + 26, 1.3f, amber);
        g.setColour(amber.withAlpha(0.7f));
        g.drawLine(end - 7, top + 13, end - 3, top + 17, 1.2f);
        g.drawLine(end - 3, top + 17, end + 1, top + 13, 1.2f);
        const auto plot = [&](float frame, float y, float extent, float strength, float stroke)
        {
            juce::Path path;
            for (int i = 0; i <= 128; ++i)
            {
                const float phase = i / 128.0f;
                const float value = audio.waves().display(osc.table, frame, phase);
                const float px = x + (end - x) * phase;
                const float py = y - value * extent;
                if (i == 0) path.startNewSubPath(px, py);
                else path.lineTo(px, py);
            }
            g.setColour(amber.withAlpha(strength));
            g.strokePath(path, juce::PathStrokeType(stroke));
        };
        if (stacked[row])
        {
            const float position = isFixedTable(osc.table) ? 0.5f : osc.position;
            for (int frame = 4; frame >= 0; --frame)
                plot(frame / 4.0f, top + 79.0f - frame * 7.0f, 6.0f, 0.44f, 0.8f);
            plot(position, top + 79.0f - position * 28.0f, 6.0f, 0.99f, 1.9f);
        }
        else
        {
            g.setColour(amber.withAlpha(0.19f));
            g.drawHorizontalLine(area.getY() + 65, x, end);
            plot(osc.position, top + 65.0f, 21.0f, 0.95f, 1.65f);
        }
    }

    void paintLfoAndEnvelope(Graphics& g) const
    {
        for (const auto& r : { envPanel, lfoPanel })
        {
            g.setColour(line);
            g.drawHorizontalLine(336, static_cast<float>(r.getX() + 12), static_cast<float>(r.getRight() - 12));
            g.setColour(steel);
            g.drawHorizontalLine(337, static_cast<float>(r.getX() + 12), static_cast<float>(r.getRight() - 12));
        }
        for (int i = 0; i < 2; ++i)
        {
            label(g, i == 0 ? "FILTER ADSR" : "AMP ADSR", { 669, 155 + 190 * i, 181, 22 }, 12.5f, blue,
                  juce::Justification::centredLeft, true);
            label(g, "LFO " + String(i + 1), { 889, 155 + 190 * i, 137, 22 }, 12.5f, blue,
                  juce::Justification::centredLeft, true);
        }
    }

    void paintMatrix(Graphics& g) const
    {
        display(g, matrixDisplay);
        const auto& p = audio.patch().sound;
        const int value = juce::roundToInt(p.matrix[matrixRow][matrixCol] * 100.0f);
        pixels(g, "ROUTE", 1083, 169, 1, amber.withAlpha(0.64f));
        pixels(g, String(sourceNames[matrixRow]) + " > " + destinationNames[p.matrixTargets[matrixCol]],
               1083, 187, 1, amber);
        const auto amount = (value >= 0 ? String("+") : String("-")) + String(std::abs(value)).paddedLeft('0', 2) + "%";
        pixels(g, amount, 1389 - amount.length() * 18, 176, 3, amber);
        g.setColour(amber.withAlpha(0.24f));
        g.drawHorizontalLine(209, 1078.0f, 1394.0f);
        pixels(g, "TARGETS", 1083, 224, 1, amber.withAlpha(0.64f));
        for (int i = 0; i < sourceCount; ++i)
            label(g, String(sourceNames[i]).toUpperCase(), { 1069, 248 + i * 32, 68, 23 }, 12.5f,
                  i == matrixRow ? mark : quiet, juce::Justification::centredLeft, i == matrixRow);
        label(g, "AMOUNT", { 1341, 248, 68, 15 }, 10.5f, quiet, juce::Justification::centred, true);
    }

    void paintPerformance(Graphics& g) const
    {
        const Rectangle<float> area { 22, 666, 1396, 112 };
        g.setColour(Colour(0xffc4c1b6));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(line.darker(0.1f));
        g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
        g.setColour(quiet.withAlpha(0.5f));
        g.drawVerticalLine(113, 686.0f, 758.0f);
        g.drawVerticalLine(385, 686.0f, 758.0f);
        label(g, "PERFORMANCE", { 128, 687, 244, 20 }, 13.0f, ink, juce::Justification::centredLeft, true);
        const auto info = "MIDI " + String(audio.inputCount()) + " IN  /  " + String(audio.voiceCount()).paddedLeft('0', 2) + " VOICES";
        pixels(g, info, 128, 718, 1, blue.darker(0.2f));
        g.setColour(audio.hasRecentMidiNote() ? mark : quiet.withAlpha(0.25f));
        g.fillEllipse(134.0f + 6.0f * info.length(), 719.0f, 5.0f, 5.0f);
        const bool showNotice = juce::Time::getMillisecondCounterHiRes() < messageEnd;
        const auto status = showNotice ? message : audio.unavailableInputs() > 0
            ? String(audio.unavailableInputs()) + " MIDI input(s) unavailable" : audio.audioStatus();
        label(g, status, { 128, 736, 247, 19 }, 12.5f, showNotice && messageIsError ? mark : quiet);
        label(g, "KEY OCT " + String(octave), { 399, 689, 89, 25 }, 12.0f, ink, juce::Justification::centred, true);
    }

    Standalone& audio;
    const std::function<void(int)> designHeightChanged;
    Skin skin;
    PerformanceKeys keys;
    juce::TooltipWindow tooltips { this, 450 };
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<BoundSlider> sliders;
    std::array<MatrixTargetButton, matrixColumnCount> matrixTargets;
    std::array<std::array<std::unique_ptr<MatrixCell>, matrixColumnCount>, sourceCount> matrix;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File fileDirectory;
    std::array<juce::TextButton*, oscillatorCount> engineButtons {}, tableButtons {};
    std::array<std::array<juce::Slider*, oscillatorColumns.size()>, oscillatorCount> oscillatorKnobs {};
    std::array<juce::Slider*, 3> setupKnobs {};
    std::array<bool, oscillatorCount> stacked { true, true, true };
    std::array<juce::TextButton*, 2> shapeButtons {}, lfoModeButtons {};
    std::array<juce::TextButton*, 3> modeButtons {};
    juce::TextButton* viewButton = nullptr;
    juce::TextButton* phaseButton = nullptr;
    juce::TextButton* filterMode = nullptr;
    juce::TextButton* presetMenu = nullptr;
    juce::TextButton* audioButton = nullptr;
    juce::TextButton* performanceButton = nullptr;
    juce::TextButton* octaveDown = nullptr;
    juce::TextButton* octaveUp = nullptr;
    juce::Slider* masterVolume = nullptr;
    int engine = 0, matrixRow = 0, matrixCol = 0, octave = 4, presetSlot = -1;
    bool edited = false, messageIsError = false;
    String message;
    double messageEnd = 0.0;
};

Panel::Panel(Standalone& audio, std::function<void(int)> onDesignHeightChanged)
    : surface(std::make_unique<Surface>(audio, [this, onDesignHeightChanged = std::move(onDesignHeightChanged)](int height)
      {
          onDesignHeightChanged(height);
          resized();
      }))
{
    addAndMakeVisible(*surface);
    setSize(designWidth, designHeight);
}

Panel::~Panel() = default;

bool Panel::playComputerKey(const juce::KeyPress& press) { return surface->playComputerKey(press); }
bool Panel::releaseComputerKeys() { return surface->releaseComputerKeys(); }
void Panel::releasePerformanceKeys() { surface->releasePerformanceKeys(); }

void Panel::paint(Graphics& g) { g.fillAll(dark); }

void Panel::resized()
{
    const int height = surface->getHeight();
    const float scale = std::min(getWidth() / static_cast<float>(designWidth), getHeight() / static_cast<float>(height));
    surface->setTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth() - designWidth * scale) * 0.5f, (getHeight() - height * scale) * 0.5f));
}
}
