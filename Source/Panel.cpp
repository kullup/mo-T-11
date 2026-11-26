#include "Panel.h"
#include "Standalone.h"
#include <BinaryData.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string_view>

namespace mot
{
namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using juce::String;

struct PanelPalette
{
    struct { Colour body, rim, edge, plate, highlight; } enclosure;
    struct { Colour ink, quiet, cool, warm; } legend;
    struct { Colour bezel, edge, glass, scanline, phosphor; bool shaded = false; } display;
    struct { Colour face, base, shade, rim, pointer, warm, cool, accentPointer, track, fill; } control;
    struct { Colour panel, header, active, panic, activeText; } button;
    struct { Colour shell, edge, ink, quiet, meter, clip, unlit; } header;
    struct { Colour white, whiteEdge, black, blackEdge, ink, pressedInk; } keys;
};

const PanelPalette classicPalette {
    { Colour(0xffd7d4ca), Colour(0xffeeebe0), Colour(0xffa9a79d), Colour(0xffe5e2d8), Colour(0xfff5f2e8) },
    { Colour(0xff282e2d), Colour(0xff58605b), Colour(0xff326675), Colour(0xffa84a25) },
    { Colour(0xff191c18), Colour(0xff45473a), Colour(0xff141812), Colour(0x0cbeb078), Colour(0xfff7b564) },
    { Colour(0xfff5f2e8), Colour(0xffada99c), Colour(0xffdcd8ca), Colour(0xfffcfaf2), Colour(0xffa84a25),
      Colour(0xffa84a25), Colour(0xff326675), Colour(0xfff5f2e8), Colour(0xff353b38), Colour(0xffb57148) },
    { Colour(0xfff5f2e8), Colour(0xff3b4643), Colour(0xff326675), Colour(0xff693a29), Colour(0xfff5f2e8) },
    { Colour(0xff262c2b), Colour(0xff505a54), Colour(0xfff5f2e8), Colour(0xffd0d0bb),
      Colour(0xffa9b48c), Colour(0xfff7b564), Colour(0xff465044) },
    { Colour(0xfff5f2e8), Colour(0xffb6b5aa), Colour(0xff262c2b), Colour(0xff141a19), Colour(0xff58605b), Colour(0xff58605b) }
};

const PanelPalette classicDarkPalette {
    { Colour(0xff292d2d), Colour(0xff515653), Colour(0xff5d6560), Colour(0xff353a38), Colour(0xff484e4b) },
    { Colour(0xfff1eee2), Colour(0xffbfc6ba), Colour(0xff93c7d4), Colour(0xffefaa74) },
    classicPalette.display,
    { Colour(0xff565c58), Colour(0xff202523), Colour(0xff3a413d), Colour(0xff6d776f), Colour(0xffefaa74),
      Colour(0xffa65335), Colour(0xff326675), Colour(0xfff5f2e8), Colour(0xff191e1c), Colour(0xffcc8656) },
    { Colour(0xff4c534e), Colour(0xff373e3b), Colour(0xff326675), Colour(0xff774331), Colour(0xfff5f2e8) },
    { Colour(0xff202624), Colour(0xff515954), Colour(0xfff5f2e8), Colour(0xffc8cabc),
      Colour(0xffa9b48c), Colour(0xfff7b564), Colour(0xff465044) },
    classicPalette.keys
};

const PanelPalette midnightPalette {
    { Colour(0xff171715), Colour(0xff777066), Colour(0xff514c44), Colour(0xff262522), Colour(0xff403c35) },
    { Colour(0xffeee4d4), Colour(0xffbbb1a0), Colour(0xffdcc096), Colour(0xffdb9359) },
    { Colour(0xff0c0b0a), Colour(0xff5a4c3c), Colour(0xff190f08), Colour(0x16eca355), Colour(0xffffb75e) },
    { Colour(0xff383733), Colour(0xff0b0b0a), Colour(0xff20201d), Colour(0xff71685b), Colour(0xfff2dec1),
      Colour(0xff694134), Colour(0xff555447), Colour(0xffffecd3), Colour(0xff10100e), Colour(0xffa76533) },
    { Colour(0xff35332e), Colour(0xff302e29), Colour(0xff58402b), Colour(0xff6d392c), Colour(0xffffd9a4) },
    { Colour(0xff1d1c19), Colour(0xff635a4d), Colour(0xffeee4d4), Colour(0xffbfb09b),
      Colour(0xffffb75e), Colour(0xffffb75e), Colour(0xff3a2719) },
    { Colour(0xffeee4cf), Colour(0xffa59b88), Colour(0xff23221f), Colour(0xff0c0b0a), Colour(0xff4c4235), Colour(0xff392317) }
};

const PanelPalette enamelPalette {
    { Colour(0xff131617), Colour(0xffefede5), Colour(0xff414844), Colour(0xffc6c2b8), Colour(0xffddd8cd) },
    { Colour(0xff161b18), Colour(0xff333a33), Colour(0xff29382c), Colour(0xff783329) },
    { Colour(0xff171a19), Colour(0xff424946), Colour(0xff0c1410), Colour(0x0c9abf88), Colour(0xffb5eb77), true },
    { Colour(0xffd6d2c8), Colour(0xff171b1c), Colour(0xff9e9f96), Colour(0xff4a5152), Colour(0xff131617),
      Colour(0xffc96a48), Colour(0xff71aeb4), Colour(0xff131617), Colour(0xff1c2323), Colour(0xff848e66) },
    { Colour(0xffe7e3d9), Colour(0xffe7e3d9), Colour(0xff303c20), Colour(0xffe29a7f), Colour(0xffeff8de) },
    { Colour(0xffc6c2b8), Colour(0xff494b44), Colour(0xff161b18), Colour(0xff333a33),
      Colour(0xffa6df69), Colour(0xfff37458), Colour(0xff303c30) },
    { Colour(0xffeeebe0), Colour(0xff94938a), Colour(0xff292d2d), Colour(0xff161a19), Colour(0xff414844), Colour(0xff1b2719) }
};

struct ThemeOption { PanelTheme theme; const char* label; };
constexpr ThemeOption themeOptions[] {
    { PanelTheme::classic, "Classic" },
    { PanelTheme::classicDark, "Classic Dark" },
    { PanelTheme::midnight, "Midnight" },
    { PanelTheme::enamel, "Enamel" }
};

constexpr float displayTextScale = 1.3f;
constexpr int woodCheekWidth = 32;
constexpr int headerInset = 19;
constexpr int headerBottom = 104;
constexpr int classicSideTrim = 20;

juce::Font panelFont(float size, bool bold = false)
{
    static const auto medium = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
    static const auto boldFace = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);
    return juce::Font(juce::FontOptions(bold ? boldFace : medium).withHeight(size)
                          .withKerningFactor(bold ? 0.055f : 0.015f));
}

void label(Graphics& g, const String& text, Rectangle<int> area, float size, Colour colour,
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

juce::ColourGradient panelLightGradient(Rectangle<float> bounds, Colour highlight, Colour shade)
{
    constexpr float horizontal = 0.74f;
    // Project the opposite corner onto the light direction so wide and tall parts share the same angle.
    const float span = (horizontal * bounds.getWidth() + bounds.getHeight()) / (1.0f + horizontal * horizontal);
    const auto topLeft = bounds.getTopLeft();
    return { highlight, topLeft, shade, topLeft.translated(horizontal * span, span), false };
}

void setRimLighting(Graphics& g, Rectangle<float> bounds, Colour highlight, Colour edge, Colour shade)
{
    auto light = panelLightGradient(bounds, highlight, shade);
    light.addColour(0.40, edge);
    g.setGradientFill(light);
}

void drawControlShadow(Graphics& g, Rectangle<float> bounds, float cornerRadius, Colour colour)
{
    juce::Path silhouette;
    silhouette.addRoundedRectangle(bounds, cornerRadius);
    juce::DropShadow(colour, 2, { 1, 2 }).drawForPath(g, silhouette);
}

const juce::Image& enamelGrain()
{
    static const auto grain = []
    {
        constexpr int size = 160;
        juce::Image image(juce::Image::ARGB, size, size, false);
        juce::Random random(311);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const int fleck = random.nextInt(29) - 14;
                const auto tone = fleck < 0 ? juce::Colours::black : juce::Colours::white;
                image.setPixelAt(x, y, tone.withAlpha(static_cast<juce::uint8>(std::abs(fleck))));
            }
        return image;
    }();
    return grain;
}

const juce::Image& brushedMetal()
{
    static const auto cap = []
    {
        constexpr int size = 160;
        constexpr float radius = size * 0.5f - 1.0f;
        juce::Image image(juce::Image::ARGB, size, size, true);
        juce::Random random(412);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const float dx = x + 0.5f - size * 0.5f;
                const float dy = y + 0.5f - size * 0.5f;
                const float distance = std::hypot(dx, dy);
                const float coverage = juce::jlimit(0.0f, 1.0f, radius + 0.5f - distance);
                if (coverage == 0.0f) continue;

                const float angle = std::atan2(dy, dx);
                const float light = std::cos(2.0f * (angle + 0.40f));
                const float rings = std::cos(distance * juce::MathConstants<float>::twoPi / 3.6f);
                const float contrast = juce::jmin(1.0f, distance / 5.0f);
                const float edge = juce::jlimit(0.0f, 1.0f, (distance - radius + 8.0f) / 8.0f);
                const auto grey = static_cast<juce::uint8>(juce::jlimit(0, 255, juce::roundToInt(
                    189.0f + contrast * (39.0f * light + (13.0f + 5.0f * light) * rings)
                    - 24.0f * edge + 7.0f * random.nextFloat())));
                image.setPixelAt(x, y, Colour(grey, grey, grey, coverage));
            }
        return image;
    }();
    return cap;
}

void drawEnamelPlate(Graphics& g, Rectangle<float> bounds, Colour colour)
{
    if (bounds.isEmpty() || !g.clipRegionIntersects(bounds.getSmallestIntegerContainer())) return;

    constexpr float radius = 3.0f;
    g.setGradientFill(panelLightGradient(bounds, colour.brighter(0.10f), colour.darker(0.11f)));
    g.fillRoundedRectangle(bounds, radius);
    g.setTiledImageFill(enamelGrain(), 0, 0, 1.0f);
    g.fillRoundedRectangle(bounds, radius);
    setRimLighting(g, bounds, colour.brighter(0.30f), colour, colour.darker(0.46f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

void display(Graphics& g, Rectangle<int> bounds, const PanelPalette& palette)
{
    const auto& colours = palette.display;
    const auto bezel = bounds.toFloat();
    g.setColour(colours.bezel);
    g.fillRoundedRectangle(bezel, 5.0f);
    setRimLighting(g, bezel, colours.edge.interpolatedWith(juce::Colours::white, 0.24f),
                   colours.edge, colours.edge.darker(0.35f));
    g.drawRoundedRectangle(bezel.reduced(0.5f), 5.0f, 1.0f);

    const auto inner = bounds.reduced(4);
    const auto glass = inner.toFloat();
    const auto lip = glass.expanded(0.85f);
    g.setGradientFill(panelLightGradient(lip, juce::Colours::black.withAlpha(0.72f), juce::Colours::transparentBlack));
    g.drawRoundedRectangle(lip, 3.85f, 1.0f);
    if (colours.shaded)
        g.setGradientFill(panelLightGradient(glass, colours.glass.brighter(0.47f), colours.glass.darker(0.28f)));
    else
        g.setColour(colours.glass);
    g.fillRoundedRectangle(glass, 3.0f);
    g.setColour(colours.scanline);
    for (int y = inner.getY() + 3; y < inner.getBottom() - 2; y += 3)
        g.drawHorizontalLine(y, static_cast<float>(inner.getX() + 1), static_cast<float>(inner.getRight() - 1));
}

void drawScrew(Graphics& g, juce::Point<float> centre, float diameter, float angle)
{
    const auto well = Rectangle<float>(diameter, diameter).withCentre(centre);
    g.setColour(Colour(0xcf1a1009));
    g.fillEllipse(well.expanded(1.1f).translated(0.35f, 0.65f));
    g.setGradientFill(panelLightGradient(well, Colour(0xffa29681), Colour(0xff423c32)));
    g.fillEllipse(well);
    g.setColour(Colour(0xff271e17));
    g.drawLine({ centre.getPointOnCircumference(diameter * 0.35f, angle),
                 centre.getPointOnCircumference(diameter * 0.35f, angle + juce::MathConstants<float>::pi) }, 1.25f);
}

void drawWoodCheek(Graphics& g, Rectangle<float> area, bool right)
{
    if (!g.clipRegionIntersects(area.getSmallestIntegerContainer())) return;
    static const auto walnut = juce::ImageFileFormat::loadFrom(BinaryData::Walnut_jpg, BinaryData::Walnut_jpgSize);
    const float outer = right ? area.getRight() : area.getX();
    const float inner = right ? area.getX() : area.getRight();
    const float direction = right ? -1.0f : 1.0f;
    const float top = area.getY() + 1.0f, bottom = area.getBottom() - 1.0f;

    juce::Path board;
    board.startNewSubPath(inner, top);
    constexpr float chamfer = 6.0f;
    board.lineTo(outer + chamfer * direction, top);
    board.lineTo(outer, top + 8.0f);
    board.lineTo(outer, bottom - 8.0f);
    board.lineTo(outer + chamfer * direction, bottom);
    board.lineTo(inner, bottom);
    board.closeSubPath();
    {
        const juce::Graphics::ScopedSaveState clipped(g);
        g.reduceClipRegion(board);
        g.fillAll(Colour(0xff80512e));
        if (walnut.isValid())
        {
            const int halfWidth = walnut.getWidth() / 2;
            g.drawImage(walnut, static_cast<int>(area.getX()), static_cast<int>(area.getY()),
                        static_cast<int>(area.getWidth()), static_cast<int>(area.getHeight()),
                        right ? halfWidth : 0, 0, halfWidth, walnut.getHeight());
        }
        juce::ColourGradient varnish(Colour(0x86301509), outer, top, Colour(0xaf170b05), inner, top, false);
        varnish.addColour(0.13, Colour(0x36e5a974));
        varnish.addColour(0.23, Colour(0x0bf3bd81));
        varnish.addColour(0.82, Colour(0x25251106));
        g.setGradientFill(varnish);
        g.fillPath(board);
        g.setGradientFill({ Colour(0x00f4c491), area.getCentreX(), top, Colour(0x4825120a),
                            area.getCentreX(), bottom, false });
        g.fillPath(board);
        g.setColour(Colour(0x90ffd0a0));
        g.drawLine(outer + (chamfer + 1.0f) * direction, top + 2.0f, inner, top + 2.0f);
        g.setColour(Colour(0x7b381c0b));
        g.drawLine(outer + chamfer * direction, bottom - 1.0f, inner, bottom - 1.0f, 2.0f);
        g.drawLine(inner - 2.0f * direction, top, inner - 2.0f * direction, bottom, 2.0f);
    }
    const float x = area.getCentreX() + 2.0f * direction;
    drawScrew(g, { x, top + 27.0f }, 8.0f, right ? -0.7f : 0.65f);
    drawScrew(g, { x, bottom - 25.0f }, 8.0f, right ? 0.4f : -0.95f);
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
    void setTheme(PanelTheme next)
    {
        theme = next;
        const auto& c = palette();
        using UI = ColourScheme::UIColour;
        auto scheme = getDarkColourScheme();
        if (hasThemedMenus())
        {
            scheme.setUIColour(UI::menuBackground, c.header.shell);
            scheme.setUIColour(UI::menuText, c.header.ink);
            scheme.setUIColour(UI::highlightedFill, c.button.active);
            scheme.setUIColour(UI::highlightedText, c.button.activeText);
        }
        setColourScheme(scheme);
    }

    PanelTheme getTheme() const { return theme; }
    bool hasThemedMenus() const { return theme != PanelTheme::classic; }
    bool hasWoodCheeks() const { return theme == PanelTheme::classicDark || isMidnight(); }
    bool isMidnight() const { return theme == PanelTheme::midnight; }
    bool isEnamel() const { return theme == PanelTheme::enamel; }
    const PanelPalette& palette() const
    {
        switch (theme)
        {
            case PanelTheme::classicDark: return classicDarkPalette;
            case PanelTheme::midnight: return midnightPalette;
            case PanelTheme::enamel: return enamelPalette;
            case PanelTheme::classic: return classicPalette;
        }
        return classicPalette;
    }

    juce::Font getPopupMenuFont() override { return panelFont(17.0f); }

    void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float position,
                          float start, float end, juce::Slider& slider) override
    {
        const auto& c = palette();
        const bool hardware = isMidnight();
        const float diameter = static_cast<float>(std::min(w, h)) - 15.0f;
        const juce::Point<float> centre { x + w * 0.5f, y + h * 0.5f };
        const auto bounds = Rectangle<float>(diameter, diameter).withCentre(centre);
        const auto cap = hardware ? bounds.reduced(diameter * 0.096f).translated(0.0f, -0.6f) : bounds;
        const bool customFace = slider.isColourSpecified(juce::Slider::thumbColourId);
        const auto face = customFace ? slider.findColour(juce::Slider::thumbColourId) : c.control.face;
        const auto radialPoint = [&](float angle, float distance)
        {
            return centre.getPointOnCircumference(distance, angle);
        };

        if (isEnamel())
        {
            drawEnamelKnob(g, bounds, start, end, position, slider);
            return;
        }

        for (int i = 0; i <= 10; ++i)
        {
            const float angle = start + (end - start) * static_cast<float>(i) / 10.0f;
            const bool major = i == 0 || i == 5 || i == 10;
            const float inner = hardware && !major ? 4.0f : 3.0f;
            g.setColour(major ? c.legend.quiet : c.enclosure.edge);
            g.drawLine({ radialPoint(angle, diameter * 0.5f + inner), radialPoint(angle, diameter * 0.5f + 6) }, 1.0f);
        }
        drawControlShadow(g, bounds, diameter * 0.5f, hardware ? Colour(0x74000000) : Colour(0x490c120f));
        if (hardware)
        {
            const auto sidewall = bounds.translated(0.55f, 1.2f);
            g.setGradientFill(panelLightGradient(sidewall, face.brighter(0.18f), c.control.base));
            g.fillEllipse(sidewall);
            const int flutes = diameter < 30.0f ? 18 : 30;
            for (int i = 0; i < flutes; ++i)
            {
                const float angle = juce::MathConstants<float>::twoPi * i / flutes;
                g.setColour(i % 2 == 0 ? c.control.rim.withAlpha(0.48f) : c.control.base.withAlpha(0.8f));
                g.drawLine({ radialPoint(angle, diameter * 0.42f), radialPoint(angle, diameter * 0.49f) }, 1.1f);
            }
        }
        else
        {
            g.setColour(customFace ? face.darker(0.42f) : c.control.base);
            g.fillEllipse(bounds.translated(0.55f, 1.2f));
        }

        const auto shade = hardware ? face.darker(0.48f) : customFace ? face.darker(0.22f) : c.control.shade;
        g.setGradientFill(panelLightGradient(cap, face.interpolatedWith(juce::Colours::white, 0.11f), shade));
        g.fillEllipse(cap);
        const auto rim = face.interpolatedWith(c.control.rim, 0.25f);
        setRimLighting(g, cap, rim.interpolatedWith(juce::Colours::white, 0.43f),
                       rim.darker(0.16f), shade.darker(0.25f));
        g.drawEllipse(cap.reduced(0.55f), 1.0f);

        const bool focused = slider.hasKeyboardFocus(true);
        if (focused || slider.isMouseOverOrDragging())
        {
            g.setColour(c.legend.cool);
            g.drawEllipse(bounds, focused ? 1.55f : 0.9f);
        }
        const float angle = start + position * (end - start);
        const auto indicator = customFace ? c.control.accentPointer : c.control.pointer;
        g.setColour(slider.isEnabled() ? indicator : c.legend.quiet.withAlpha(0.48f));
        g.drawLine({ radialPoint(angle, diameter * (hardware ? 0.19f : 0.14f)),
                     radialPoint(angle, diameter * 0.38f) }, diameter > 70 ? 3.3f : 2.35f);
        if (!hardware) g.fillEllipse(Rectangle<float>(3.0f, 3.0f).withCentre(centre));
    }

    void drawLinearSlider(Graphics& g, int x, int y, int w, int h, float position, float, float,
                          juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const auto& c = palette();
        const float capRadius = isEnamel() || isMidnight() ? 1.5f : 3.5f;
        const float cx = x + w * 0.5f;
        const Rectangle<float> track(cx - 3.0f, static_cast<float>(y), 6.0f, static_cast<float>(h));
        for (int tick = 0; tick <= 8; ++tick)
        {
            const float ypos = y + static_cast<float>(h * tick) / 8.0f;
            g.setColour(isEnamel() ? c.legend.ink : tick == 4 ? c.legend.quiet : c.enclosure.edge);
            g.drawLine(cx - 11.0f, ypos, cx - 7.0f, ypos, 1.0f);
            g.drawLine(cx + 7.0f, ypos, cx + 11.0f, ypos, 1.0f);
        }
        g.setColour(c.control.track);
        g.fillRoundedRectangle(track, 3.0f);
        if (isEnamel() || isMidnight())
        {
            g.setColour(c.enclosure.edge);
            g.drawRoundedRectangle(track.expanded(0.55f), 3.5f, 0.8f);
        }
        else
        {
            const float reference = slider.getMinimum() < 0 ? y + h * 0.5f : static_cast<float>(y + h);
            g.setColour(c.control.fill);
            g.fillRect(cx - 1.0f, std::min(reference, position), 2.0f, std::abs(reference - position));
        }
        const auto cap = Rectangle<float>(static_cast<float>(std::min(w - 4, 25)), 15.0f).withCentre({ cx, position });
        if (isEnamel())
        {
            drawControlShadow(g, cap, capRadius, Colour(0x82000000));
            g.setColour(c.control.base);
            g.fillRoundedRectangle(cap, capRadius);
            const auto top = cap.reduced(2.0f, 2.2f);
            g.setGradientFill(panelLightGradient(top, c.enclosure.rim, c.control.shade));
            g.fillRoundedRectangle(top, 1.0f);
            g.setColour(c.control.pointer);
            g.drawLine(top.getX() + 2.0f, position, top.getRight() - 2.0f, position, 1.4f);
            if (slider.hasKeyboardFocus(true)) drawEnamelFocus(g, cap.expanded(1.5f), capRadius);
            return;
        }

        drawControlShadow(g, cap, capRadius, Colour(0x490c120f));
        const auto face = c.control.face;
        const auto shade = isMidnight() ? c.control.shade : face.darker(0.12f);
        g.setGradientFill(panelLightGradient(cap, face.interpolatedWith(juce::Colours::white, 0.11f), shade));
        g.fillRoundedRectangle(cap, capRadius);
        setRimLighting(g, cap, face.interpolatedWith(juce::Colours::white, 0.38f),
                       face.darker(0.20f), shade.darker(0.27f));
        g.drawRoundedRectangle(cap.reduced(0.5f), capRadius, 1.0f);
        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(c.legend.cool);
            g.drawRoundedRectangle(cap, capRadius, 1.0f);
        }
        g.setColour(c.control.pointer);
        g.drawLine(cap.getX() + 5.0f, position, cap.getRight() - 5.0f, position, 1.6f);
    }

    int getSliderThumbRadius(juce::Slider&) override { return 10; }

    void drawButtonBackground(Graphics& g, juce::Button& button, const Colour& colour,
                              bool highlighted, bool down) override
    {
        const auto& c = palette();
        const auto phosphor = c.display.phosphor;
        const bool enabled = button.isEnabled();
        const bool focused = enabled && button.hasKeyboardFocus(true);
        const float radius = isMidnight() ? 2.0f : 3.5f;
        const auto area = button.getLocalBounds().toFloat().reduced(1.5f).withTrimmedBottom(2.0f);
        if (colour.isTransparent())
        {
            if (enabled && (highlighted || focused || down))
            {
                g.setColour(phosphor.withAlpha(down ? 1.0f : focused ? 0.85f : 0.5f));
                g.drawRoundedRectangle(area, 4.0f, 1.0f);
            }
            return;
        }

        if (isEnamel())
        {
            drawEnamelButton(g, button, colour, area, highlighted, down);
            return;
        }

        const bool lit = enabled && button.getToggleState();
        const bool pressed = enabled && down;
        const float opacity = enabled ? 1.0f : 0.48f;
        const auto offColour = button.findColour(juce::TextButton::buttonColourId);
        const auto face = area.translated(0.0f, pressed ? 1.4f : 0.0f);
        const auto base = lit ? colour : offColour;
        const auto fill = pressed ? base.interpolatedWith(phosphor, 0.22f).darker(0.10f)
                                  : enabled && highlighted ? base.brighter(0.10f) : base;

        const float shadowOpacity = (isMidnight() ? 0.48f : 0.33f) * opacity;
        drawControlShadow(g, area.reduced(0.6f), radius - 0.6f,
                          Colour(0xff101611).withAlpha(shadowOpacity * (pressed ? 0.6f : 1.0f)));
        if (lit || pressed)
        {
            g.setColour(phosphor.withAlpha(lit ? 0.42f : 0.24f));
            g.fillRoundedRectangle(face.expanded(1.25f), radius + 1.0f);
        }

        const auto top = fill.interpolatedWith(juce::Colours::white, lit ? 0.28f : 0.14f);
        g.setGradientFill(panelLightGradient(face, top.withAlpha(opacity),
                                            fill.darker(lit ? 0.16f : 0.23f).withAlpha(opacity)));
        g.fillRoundedRectangle(face, radius);
        setRimLighting(g, face, top.interpolatedWith(juce::Colours::white, 0.34f).withAlpha(opacity),
                       fill.darker(0.21f).withAlpha(opacity), fill.darker(0.42f).withAlpha(opacity));
        g.drawRoundedRectangle(face.reduced(0.4f), radius, 0.8f);

        if (focused)
        {
            const auto ink = offColour.getPerceivedBrightness() > 0.55f ? c.legend.cool
                : c.legend.cool.interpolatedWith(juce::Colours::white, 0.45f);
            g.setColour(ink);
            g.drawRoundedRectangle(area.expanded(0.9f, 0.9f).withBottom(area.getBottom() + 2.7f), radius + 0.9f, 1.1f);
        }
    }

    void drawButtonText(Graphics& g, juce::TextButton& button, bool, bool down) override
    {
        const bool enabled = button.isEnabled();
        const bool active = enabled && button.getToggleState();
        const auto text = button.findColour(active ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
        auto area = button.getLocalBounds().reduced(4, 2).translated(0, enabled && down ? 1 : -1);
        if (isEnamel() && active && button.getWidth() >= 44) area = area.withTrimmedLeft(6);
        label(g, button.getButtonText(), area,
              button.getHeight() > 32 ? 14.0f : 12.0f, text.withMultipliedAlpha(enabled ? 1.0f : 0.40f),
              juce::Justification::centred, true);
    }

private:
    static void drawEnamelFocus(Graphics& g, Rectangle<float> bounds, float radius)
    {
        g.setColour(enamelPalette.control.base);
        g.drawRoundedRectangle(bounds, radius, 2.4f);
        g.setColour(enamelPalette.enclosure.rim);
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }

    static void drawEnamelKnob(Graphics& g, Rectangle<float> bounds, float start, float end,
                                float position, const juce::Slider& slider)
    {
        const auto& c = enamelPalette;
        const auto centre = bounds.getCentre();
        const auto radius = bounds.getWidth() * 0.5f;
        const bool enabled = slider.isEnabled();
        const auto point = [&](float angle, float distance) { return centre.getPointOnCircumference(distance, angle); };
        for (int i = 0; i <= 10; ++i)
        {
            const auto angle = start + (end - start) * i / 10.0f;
            const bool major = i % 5 == 0;
            g.setColour(c.legend.ink.withAlpha(enabled ? 0.95f : 0.35f));
            g.drawLine({ point(angle, radius + 1.9f), point(angle, radius + (major ? 6.2f : 5.0f)) },
                       major ? 1.5f : 1.0f);
        }

        drawControlShadow(g, bounds, radius, Colour(0x96000000));
        g.setColour(c.control.base.darker(0.65f));
        g.fillEllipse(bounds.translated(0.0f, 1.25f));
        g.setGradientFill(panelLightGradient(bounds, c.control.rim, c.control.base.darker(0.7f)));
        g.fillEllipse(bounds);
        g.setColour(c.control.base);
        g.drawEllipse(bounds.reduced(0.5f), 1.0f);

        const auto cap = bounds.reduced(radius * 0.23f);
        if (slider.isColourSpecified(juce::Slider::thumbColourId))
        {
            g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(0.83f));
            g.drawEllipse(cap.expanded(0.7f), 0.9f);
        }
        g.setOpacity(enabled ? 1.0f : 0.55f);
        g.drawImage(brushedMetal(), cap);

        const auto angle = start + position * (end - start);
        const auto indexStart = point(angle, radius * 0.28f);
        const auto indexEnd = point(angle, radius * 0.68f);
        g.setColour(c.control.pointer.withAlpha(enabled ? 1.0f : 0.55f));
        g.drawLine({ indexStart, indexEnd }, juce::jlimit(1.25f, 3.0f, radius * 0.12f));

        if (enabled && slider.hasKeyboardFocus(true))
            drawEnamelFocus(g, bounds, radius);
        else if (enabled && slider.isMouseOverOrDragging())
        {
            g.setColour(c.enclosure.rim.withAlpha(0.60f));
            g.drawEllipse(bounds.reduced(0.8f), 0.8f);
        }
    }

    static void drawEnamelButton(Graphics& g, const juce::Button& button, Colour colour, Rectangle<float> area,
                                  bool highlighted, bool down)
    {
        const auto& c = enamelPalette;
        const bool enabled = button.isEnabled();
        const bool pressed = enabled && down;
        const bool active = enabled && button.getToggleState();
        constexpr float radius = 2.0f;
        drawControlShadow(g, area, radius, Colour(0xff101413).withAlpha(pressed ? 0.34f : 0.60f));
        g.setGradientFill(panelLightGradient(area, c.control.rim, c.control.base.darker(0.50f)));
        g.fillRoundedRectangle(area, radius);
        auto cap = area.reduced(2.2f, 2.0f).withTrimmedBottom(1.0f).translated(0.0f, pressed ? 1.0f : 0.0f);
        const auto tone = !enabled ? button.findColour(juce::TextButton::buttonColourId).interpolatedWith(c.control.shade, 0.55f)
                        : pressed ? colour.darker(0.20f) : highlighted ? colour.brighter(0.16f) : colour;
        g.setGradientFill(panelLightGradient(cap, tone.brighter(0.14f), tone.darker(0.12f)));
        g.fillRoundedRectangle(cap, 1.0f);
        if (active)
        {
            auto indicator = button.getWidth() >= 44 ? cap.removeFromLeft(5.0f).reduced(0.5f, 0.75f)
                                                     : cap.removeFromBottom(2.0f).reduced(1.0f, 0.0f);
            g.setColour(c.header.meter);
            g.fillRoundedRectangle(indicator, 0.6f);
        }

        if (enabled && button.hasKeyboardFocus(true))
            drawEnamelFocus(g, area, radius);
    }

    PanelTheme theme = PanelTheme::classic;
};

struct MatrixRoute
{
    int source;
    Destination target;

    bool operator==(const MatrixRoute& other) const noexcept
    {
        return source == other.source && target == other.target;
    }
};

bool matrixTargetInUse(const Patch& patch, Destination target)
{
    return std::any_of(patch.matrix.begin(), patch.matrix.end(), [target](const auto& row)
        { return row[target].has_value(); });
}

String matrixAmountText(double amount)
{
    const int value = juce::roundToInt(amount * 100.0);
    return (value < 0 ? String("-") : String("+")) + String(std::abs(value)).paddedLeft('0', 2) + "%";
}

class MatrixDisplay final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit MatrixDisplay(const Skin& panelSkin) : skin(panelSkin)
    {
        list.setTitle("Modulation routes");
        list.setDescription("Select a route with the arrow keys. Press Enter to focus Amount.");
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setRowHeight(30);
        list.getViewport()->setScrollBarThickness(8);
        addAndMakeVisible(list);
    }

    void updateColours()
    {
        list.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,
                                              skin.palette().display.phosphor.withAlpha(0.62f));
    }

    void show(const Patch& patch, std::optional<MatrixRoute> selection, bool revealSelection)
    {
        const juce::ScopedValueSetter<bool> update(updating, true);
        rows.clear();
        int selectedRow = -1;
        for (int source = 0; source < sourceCount; ++source)
            for (int target = 0; target < destinationCount; ++target)
            {
                const auto& amount = patch.matrix[source][target];
                if (!amount) continue;
                const MatrixRoute route { source, static_cast<Destination>(target) };
                if (selection == route)
                    selectedRow = getNumRows();
                rows.push_back({ route, *amount });
            }

        list.updateContent();
        const bool selectionMoved = list.getLastRowSelected() != selectedRow;
        if (selectionMoved)
        {
            juce::SparseSet<int> selected;
            if (selectedRow >= 0)
                selected.addRange({ selectedRow, selectedRow + 1 });
            list.setSelectedRows(selected, juce::dontSendNotification);
        }
        if ((revealSelection || selectionMoved) && selectedRow >= 0)
            list.scrollToEnsureRowIsOnscreen(selectedRow);
        else if (revealSelection)
            list.getViewport()->setViewPosition(0, 0);
        list.repaint();
        repaint();
    }

    std::function<void(MatrixRoute)> onSelect;
    std::function<void()> onEditAmount, onClear;

private:
    struct Row { MatrixRoute route; float amount; };

    void resized() override { list.setBounds(getLocalBounds().reduced(12, 8)); }

    static void centredPixels(Graphics& g, const String& text, int centreX, int centreY, Colour colour)
    {
        const float width = (text.length() * 6 - 1) * displayTextScale;
        drawTextLine(g, text, std::round(centreX - width * 0.5f), centreY, colour);
    }

    static void drawTextLine(Graphics& g, const String& text, float x, int centreY, Colour colour)
    {
        const float height = 7.0f * displayTextScale;
        pixels(g, text, x, std::round(centreY - height * 0.5f), displayTextScale, colour);
    }

    void paint(Graphics& g) override
    {
        const auto& palette = skin.palette();
        display(g, getLocalBounds(), palette);
        const auto phosphor = palette.display.phosphor;
        if (rows.empty())
        {
            centredPixels(g, "NO ROUTES", getWidth() / 2, getHeight() / 2 - 8, phosphor);
            centredPixels(g, "USE ADD ROUTE", getWidth() / 2, getHeight() / 2 + 8, phosphor.withAlpha(0.65f));
        }
    }

    int getNumRows() override { return static_cast<int>(rows.size()); }

    String getNameForRow(int index) override
    {
        if (!juce::isPositiveAndBelow(index, getNumRows())) return {};
        const auto& [route, amount] = rows[index];
        return String(sourceNames[route.source]) + " to " + destinationNames[route.target] + ", " + matrixAmountText(amount);
    }

    void paintListBoxItem(int index, Graphics& g, int width, int height, bool selected) override
    {
        if (!juce::isPositiveAndBelow(index, getNumRows())) return;
        const auto& [route, amount] = rows[index];
        const int divider = width * 2 / 3;
        const auto phosphor = skin.palette().display.phosphor;
        const auto colour = selected ? phosphor.brighter(0.12f) : phosphor;
        if (selected)
        {
            g.fillAll(phosphor.withAlpha(0.12f));
            g.setColour(colour);
            g.fillRect(1, 1, 3, height - 2);
        }
        g.setColour(phosphor.withAlpha(0.54f));
        g.drawRect(0, 0, width, height, 1);
        g.drawVerticalLine(divider, 0.0f, static_cast<float>(height));

        constexpr float textInset = 12.0f;
        drawTextLine(g, sourceNames[route.source], textInset, height / 2 - 6, colour);
        drawTextLine(g, destinationNames[route.target], textInset, height / 2 + 6, colour);
        centredPixels(g, matrixAmountText(amount), (divider + width) / 2, height / 2, colour);
    }

    void selectedRowsChanged(int index) override
    {
        if (!updating && juce::isPositiveAndBelow(index, getNumRows()) && onSelect)
            onSelect(rows[index].route);
    }

    void returnKeyPressed(int) override { if (onEditAmount) onEditAmount(); }
    void deleteKeyPressed(int) override { if (onClear) onClear(); }

    const Skin& skin;
    bool updating = false;
    std::vector<Row> rows;
    juce::ListBox list { "Modulation routes", this };
};

class PerformanceKeys final : public juce::Component, public juce::SettableTooltipClient
{
public:
    PerformanceKeys(Standalone& instrument, const Skin& panelSkin) : audio(instrument), skin(panelSkin)
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
            g.setColour(skin.palette().display.phosphor);
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
        const auto& c = skin.palette();
        const bool down = note == mouseNote || computerHeld(note);
        const bool black = isBlack(note);
        const auto area = keyArea(note).reduced(0.7f, 0.0f);
        g.setColour(down ? c.display.phosphor : black ? c.keys.blackEdge : c.keys.whiteEdge);
        g.fillRoundedRectangle(area, 3.0f);
        const auto face = area.withTrimmedBottom(black ? 4.0f : 3.0f);
        const auto keyColour = down ? c.display.phosphor : black ? c.keys.black : c.keys.white;
        if (skin.isEnamel() || skin.isMidnight())
            g.setGradientFill(panelLightGradient(face, keyColour.brighter(black ? 0.28f : 0.12f),
                                                 keyColour.darker(black ? 0.23f : 0.07f)));
        else
            g.setColour(keyColour);
        g.fillRoundedRectangle(face, 2.75f);
        if (note % 12 == 0)
            label(g, "C" + String(octave + note / 12), area.getSmallestIntegerContainer().removeFromBottom(19),
                  11.5f, down ? c.keys.pressedInk : c.keys.ink, juce::Justification::centred, true);
    }

    Standalone& audio;
    const Skin& skin;
    int octave = 4, mouseNote = -1;
    std::array<bool, mapping.size()> computer {};
};

class WindowButton final : public juce::TextButton
{
public:
    WindowButton(WindowControlKind control, const String& name)
        : TextButton(name), kind(control)
    {
        setTitle(name);
        setTooltip(name);
    }

    void setMaximised(bool isMaximised)
    {
        if (maximised == isMaximised) return;
        maximised = isMaximised;
        const auto* name = maximised ? "Restore window" : "Maximize window";
        setButtonText(name);
        setTitle(name);
        setTooltip(name);
    }

private:
    void paintButton(Graphics& g, bool highlighted, bool down) override
    {
        getLookAndFeel().drawButtonBackground(g, *this, findColour(buttonColourId), highlighted, down);
        g.setColour(findColour(textColourOffId));
        const auto centre = getLocalBounds().toFloat().getCentre().translated(0.0f, down ? 1.0f : -1.0f);
        const auto icon = Rectangle<float>(11.0f, 11.0f).withCentre(centre);
        switch (kind)
        {
            case WindowControlKind::minimise:
                g.fillRect(icon.withY(centre.y + 2.0f).withHeight(1.5f));
                break;
            case WindowControlKind::maximise:
                if (maximised)
                {
                    const auto rear = icon.withTrimmedBottom(3.0f).withTrimmedLeft(3.0f);
                    juce::Path outline;
                    outline.startNewSubPath(rear.getTopLeft());
                    outline.lineTo(rear.getTopRight());
                    outline.lineTo(rear.getBottomRight());
                    g.strokePath(outline, juce::PathStrokeType(1.3f));
                    g.drawRect(icon.withTrimmedTop(3.0f).withTrimmedRight(3.0f), 1.3f);
                }
                else
                    g.drawRect(icon, 1.4f);
                break;
            case WindowControlKind::close:
                g.drawLine({ icon.getTopLeft(), icon.getBottomRight() }, 1.5f);
                g.drawLine({ icon.getBottomLeft(), icon.getTopRight() }, 1.5f);
                break;
            default: break;
        }
    }

    const WindowControlKind kind;
    bool maximised = false;
};
}

class Panel::Surface final : public juce::Component, private juce::Timer
{
public:
    Surface(Standalone& instrument, std::function<void()> onDesignSizeChanged,
            const std::function<void(WindowControlKind)>& onWindowControl)
        : audio(instrument), designSizeChanged(std::move(onDesignSizeChanged)),
          integratedCaption(static_cast<bool>(onWindowControl)), keys(audio, skin)
    {
        setLookAndFeel(&skin);
        createHeader();
        if (integratedCaption)
            createWindowControls(onWindowControl);
        createOscillators();
        createFilter();
        createEnvelopes();
        createLfos();
        createMatrix();
        createVoicing();
        createEffects();
        createArpeggiator();
        createKeyboard();
        applyTheme(audio.panelTheme());
        setPerformanceVisible(false);
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
    bool hasWoodCheeks() const { return skin.hasWoodCheeks(); }
    bool hasTopRail() const { return skin.isMidnight(); }

    WindowControlKind windowControlAt(juce::Point<float> point) const
    {
        if (!integratedCaption || !headerBounds.toFloat().contains(point) || meterBounds.toFloat().contains(point))
            return WindowControlKind::client;

        for (const auto& [control, style, kind] : buttons)
            if (control->isVisible() && control->getBounds().toFloat().contains(point))
                return kind;

        return WindowControlKind::caption;
    }

    void setWindowMaximised(bool maximised)
    {
        if (maximiseControl != nullptr)
            maximiseControl->setMaximised(maximised);
    }

    Rectangle<int> faceplateBounds() const
    {
        const auto bounds = getLocalBounds();
        return hasTopRail() ? bounds : bounds.withTrimmedTop(headerInset).reduced(classicSideTrim, 0);
    }

    void paintChassis(Graphics& g, Rectangle<float> bounds) const
    {
        const auto& c = skin.palette();
        auto face = bounds;
        g.fillAll(c.enclosure.body);
        if (skin.isEnamel())
        {
            paintEnamelChassis(g, face);
            return;
        }

        if (hasWoodCheeks())
        {
            drawWoodCheek(g, face.removeFromLeft(woodCheekWidth), false);
            drawWoodCheek(g, face.removeFromRight(woodCheekWidth), true);
        }

        if (skin.isMidnight())
            g.setGradientFill({ c.enclosure.plate, face.getTopLeft(), c.enclosure.body, face.getBottomLeft(), false });
        else
            g.setColour(c.enclosure.plate);
        g.fillRect(face);

        const float headerTop = hasTopRail() ? static_cast<float>(headerInset) : face.getY();
        g.setColour(c.header.shell);
        g.fillRect(face.withY(headerTop).withHeight(headerBottom - headerTop));
        g.setColour(c.header.edge);
        g.fillRect(face.withY(headerBottom).withHeight(1.0f));

        if (!skin.isMidnight()) return;
        for (const auto rail : { face.withHeight(17.0f), face.withY(face.getBottom() - 15.0f).withHeight(15.0f) })
        {
            g.setGradientFill({ c.enclosure.highlight, rail.getTopLeft(), c.header.shell, rail.getBottomLeft(), false });
            g.fillRect(rail);
            g.setColour(c.enclosure.rim.withAlpha(0.58f));
            g.fillRect(rail.withTrimmedTop(2.0f).withHeight(1.0f));
            g.setColour(c.control.base);
            g.fillRect(rail.withY(rail.getBottom() - 2.0f).withHeight(2.0f));
            g.setColour(c.enclosure.edge);
            g.fillRect(rail.withY(rail.getBottom() - 6.0f).withHeight(1.0f));
        }
        for (const float x : { face.getX() + 10.0f, face.getRight() - 10.0f })
        {
            const float angle = x < face.getCentreX() ? 0.8f : -0.45f;
            drawScrew(g, { x, face.getY() + 30.0f }, 6.0f, angle);
            drawScrew(g, { x, face.getBottom() - 30.0f }, 6.0f, -angle);
        }
    }

    void paint(Graphics& g) override
    {
        const auto& c = skin.palette();
        if (!skin.isEnamel())
            paintSectionDividers(g);
        paintHeader(g);
        paintOscillators(g);
        paintSectionHeading(g, filterPanel, "FILTER");
        paintSectionHeading(g, envPanel, "ENVELOPES");
        paintSectionHeading(g, lfoPanel, "LFO");
        paintSectionHeading(g, matrixPanel, "MOD TABLE");
        paintLfoAndEnvelope(g);
        const auto amountCaption = matrixAmountBounds.withY(matrixAmountBounds.getBottom() + 1).withHeight(15);
        label(g, "AMOUNT", amountCaption, 12.0f, c.legend.ink, juce::Justification::centred, true);
        paintSectionHeading(g, voicingPanel, "VOICING");
        paintEffects(g);
        paintSectionHeading(g, arpPanel, "ARPEGGIATOR");
        if (keys.isVisible()) paintPerformance(g);
        for (const auto& entry : sliders)
        {
            if (entry.caption.isEmpty() || !entry.control->isVisible()) continue;
            const auto area = entry.control->getBounds();
            const auto width = std::max(area.getWidth(), entry.fader ? 41 : 58);
            const int centre = area.getCentreX();
            const int top = area.getBottom() + 1;
            label(g, entry.caption, { centre - width / 2, top, width, 15 }, 12.0f, c.legend.ink,
                  juce::Justification::centred, true);
            label(g, entry.control->getTextFromValue(entry.control->getValue()),
                  { centre - width / 2, top + 14, width, 14 }, 11.5f, c.legend.quiet, juce::Justification::centred);
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
    enum class ButtonStyle { panel, header, display, panic };
    struct PanelButton
    {
        std::unique_ptr<juce::TextButton> control;
        ButtonStyle style;
        WindowControlKind windowControl = WindowControlKind::client;
    };

    static constexpr int panelInset = 22, panelRight = designWidth - panelInset;
    inline static const Rectangle<int> headerBounds { panelInset, headerInset, panelRight - panelInset,
                                                       headerBottom - headerInset };
    inline static const Rectangle<int> oscPanel { panelInset, 111, 475, 425 };
    inline static const Rectangle<int> filterPanel { 507, 111, 138, 425 };
    inline static const Rectangle<int> envPanel { 655, 111, 210, 425 };
    inline static const Rectangle<int> lfoPanel { 875, 111, 169, 425 };
    inline static const Rectangle<int> matrixPanel { 1054, 111, panelRight - 1054, 425 };
    static constexpr int lowerPanelY = 550, voicingWidth = 403, effectsX = panelInset + voicingWidth + 10;
    static constexpr int arpX = 1184, tapeX = effectsX + 13, reverbDivider = 815, reverbX = reverbDivider + 16;
    inline static const Rectangle<int> voicingPanel { panelInset, lowerPanelY, voicingWidth, 104 };
    inline static const Rectangle<int> effectsPanel { effectsX, lowerPanelY, arpX - effectsX - 10, 104 };
    inline static const Rectangle<int> arpPanel { arpX, lowerPanelY, panelRight - arpX, 104 };
    inline static const Rectangle<int> patchDisplay { 492, 35, 278, 48 };
    inline static const std::array<const char*, oscillatorCount> oscillatorNames { "A", "B", "C" };
    enum OscillatorColumn { positionColumn, tuneColumn, fineColumn, volumeColumn };
    inline static const std::array<const char*, 4> oscillatorColumns { "POS", "TUNE", "FINE", "VOL" };
    static constexpr int lfoKnobSize = 61;
    inline static const Rectangle<int> matrixScreenBounds = matrixPanel.reduced(15).withTop(145).withWidth(218);
    inline static const Rectangle<int> matrixAmountBounds = Rectangle<int>(lfoKnobSize, lfoKnobSize).withCentre({
        (matrixScreenBounds.getRight() + matrixPanel.getRight()) / 2, matrixScreenBounds.getY() + lfoKnobSize / 2 });
    inline static const Rectangle<int> performanceStatus { 117, 683, 267, 84 };
    inline static const std::array<const char*, 3> modeNames { "POLY", "MONO", "LEGATO" };
    inline static const std::array<const char*, 3> arpDirections { "UP", "DOWN", "UP/DOWN" };

    static int oscillatorColumnX(int column) { return 218 + column * 72; }
    static Rectangle<int> oscillatorDisplay(int row) { return { 36, 169 + row * 100, 146, 92 }; }
    static Rectangle<int> oscillatorKnob(int row, int column)
    {
        const Rectangle<int> area { oscillatorColumnX(column) - 34, oscillatorDisplay(row).getY() + 4, 68, 68 };
        return area.reduced(column == fineColumn ? 5 : 0, 0);
    }
    juce::TextButton& button(const String& caption, const String& name, Rectangle<int> area,
                             std::function<void()> action, ButtonStyle style = ButtonStyle::panel)
    {
        auto control = std::make_unique<juce::TextButton>(caption);
        auto& b = *control;
        b.setTitle(name);
        b.setTooltip(name);
        b.setWantsKeyboardFocus(true);
        b.onClick = std::move(action);
        addAndMakeVisible(b);
        b.setBounds(area);
        buttons.push_back({ std::move(control), style });
        return b;
    }

    void applyTheme(PanelTheme theme)
    {
        skin.setTheme(theme);
        const auto& c = skin.palette();
        for (const auto& [control, style, kind] : buttons)
        {
            const auto face = style == ButtonStyle::display ? juce::Colours::transparentBlack
                            : style == ButtonStyle::panic ? c.button.panic
                            : style == ButtonStyle::header ? c.button.header : c.button.panel;
            control->setColour(juce::TextButton::buttonColourId, face);
            const auto onFace = skin.isEnamel() ? c.button.active : c.display.phosphor;
            control->setColour(juce::TextButton::buttonOnColourId,
                               style == ButtonStyle::display ? face : onFace);
            control->setColour(juce::TextButton::textColourOffId,
                               style == ButtonStyle::panel ? c.legend.ink : c.header.ink);
            control->setColour(juce::TextButton::textColourOnId,
                               skin.isEnamel() ? c.button.activeText : c.display.glass);
        }
        for (auto& row : oscillatorKnobs)
            row[volumeColumn]->setColour(juce::Slider::thumbColourId, c.control.warm);
        tapeMixKnob->setColour(juce::Slider::thumbColourId, c.control.warm);
        reverbMixKnob->setColour(juce::Slider::thumbColourId, c.control.cool);
        matrixScreen.updateColours();

        for (const auto& option : themeOptions)
        {
            if (option.theme != theme) continue;
            const auto title = String("Panel theme: ") + option.label;
            themeButton->setTitle(title);
            themeButton->setTooltip(title);
            break;
        }
        sendLookAndFeelChange();
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
    void edit(T change, bool revealMatrixSelection = false)
    {
        auto next = audio.patch();
        change(next.sound);
        audio.apply(std::move(next));
        edited = true;
        syncControls(revealMatrixSelection);
    }

    void syncControls(bool revealMatrixSelection = false)
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
        viewButton->setToggleState(stacked[engine], juce::dontSendNotification);
        phaseButton->setButtonText(osc.resetPhase ? "RESET" : "FREE");
        phaseButton->setToggleState(osc.resetPhase, juce::dontSendNotification);
        for (std::size_t i = 0; i < modeButtons.size(); ++i)
            modeButtons[i]->setToggleState(static_cast<PlayMode>(i) == p.voicing.mode, juce::dontSendNotification);
        tapeSwitch->setButtonText(p.tapeDelay.enabled ? "ON" : "BYPASS");
        tapeSwitch->setToggleState(p.tapeDelay.enabled, juce::dontSendNotification);
        reverbSwitch->setButtonText(p.reverb.enabled ? "ON" : "BYPASS");
        reverbSwitch->setToggleState(p.reverb.enabled, juce::dontSendNotification);
        arpSwitch->setButtonText(p.arp.enabled ? "ON" : "OFF");
        arpSwitch->setToggleState(p.arp.enabled, juce::dontSendNotification);
        const auto direction = arpDirections[static_cast<std::size_t>(p.arp.direction)];
        arpDirectionButton->setButtonText(direction);
        arpDirectionButton->setTitle(String("Arpeggiator direction: ") + direction);
        static constexpr const char* modes[] { "LP  /  LOW PASS", "BP  /  BAND PASS", "HP  /  HIGH PASS" };
        filterMode->setButtonText(modes[static_cast<int>(p.filterMode)]);
        static constexpr const char* shapes[] { "SINE", "TRI", "RAMP", "SQR" };
        for (int i = 0; i < 2; ++i)
        {
            shapeButtons[i]->setButtonText(shapes[static_cast<int>(p.lfos[i].shape)]);
            lfoModeButtons[i]->setButtonText(p.lfos[i].restart ? "KEY" : "FREE");
            lfoModeButtons[i]->setToggleState(p.lfos[i].restart, juce::dontSendNotification);
        }
        matrixScreen.show(p, matrixSelection, revealMatrixSelection);
        matrixAmountKnob->setEnabled(matrixSelection.has_value());
        matrixClear->setEnabled(matrixSelection.has_value());
        matrixTarget->setEnabled(matrixSelection.has_value());
        presetMenu->setTitle("Factory presets. Current patch " + String::fromUTF8(audio.patch().name.c_str()));
        repaint();
    }

    void createHeader()
    {
        themeButton = &button("THEME", "Panel theme", { 245, 44, 77, 30 },
                              [this] { showThemes(); }, ButtonStyle::header);
        performanceButton = &button({}, {}, { 330, 44, 102, 30 }, [this]
        {
            setPerformanceVisible(!keys.isVisible());
            designSizeChanged();
        }, ButtonStyle::header);
        button("<", "Previous factory patch", { 448, 44, 36, 30 }, [this] { stepPreset(-1); }, ButtonStyle::header);
        button(">", "Next factory patch", { 778, 44, 36, 30 }, [this] { stepPreset(1); }, ButtonStyle::header);
        presetMenu = &button({}, "Factory presets", patchDisplay, [this] { showPresets(); }, ButtonStyle::display);
        button("OPEN", "Open patch file", { 839, 44, 65, 30 }, [this] { chooseFile(false); }, ButtonStyle::header);
        button("SAVE", "Save patch file", { 912, 44, 65, 30 }, [this] { chooseFile(true); }, ButtonStyle::header);
        const auto audioBounds = integratedCaption ? Rectangle<int>(984, 44, 70, 30)
                                                   : Rectangle<int>(1004, 44, 75, 30);
        audioButton = &button("AUDIO", "Select audio output", audioBounds,
                              [this] { showOutputs(); }, ButtonStyle::header);
        button("PANIC", "Stop all notes", panicBounds, [this]
        {
            keys.release();
            audio.panic();
            notice("All notes cleared");
        }, ButtonStyle::panic);
    }

    void createWindowControls(const std::function<void(WindowControlKind)>& action)
    {
        int x = 1270;
        for (const auto& [kind, name] : { std::pair { WindowControlKind::minimise, "Minimize window" },
                                         std::pair { WindowControlKind::maximise, "Maximize window" },
                                         std::pair { WindowControlKind::close, "Close window" } })
        {
            auto control = std::make_unique<WindowButton>(kind, name);
            control->onClick = [action, kind] { action(kind); };
            control->setBounds(x, 44, 34, 30);
            addAndMakeVisible(*control);
            if (kind == WindowControlKind::maximise)
                maximiseControl = control.get();
            buttons.push_back({ std::move(control), ButtonStyle::header, kind });
            x += 36;
        }
    }

    void showThemes()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&skin);
        for (int i = 0; i < static_cast<int>(std::size(themeOptions)); ++i)
            menu.addItem(i + 1, themeOptions[i].label, true, themeOptions[i].theme == skin.getTheme());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(themeButton),
            [safe = SafePointer<Surface>(this)](int id)
            {
                if (safe == nullptr || !juce::isPositiveAndBelow(id - 1, std::size(themeOptions))) return;
                const auto theme = themeOptions[id - 1].theme;
                if (theme == safe->skin.getTheme()) return;
                safe->audio.setPanelTheme(theme);
                safe->applyTheme(theme);
                safe->designSizeChanged();
            });
    }

    void createOscillators()
    {
        for (int i = 0; i < oscillatorCount; ++i)
        {
            const String tag(oscillatorNames[i]);
            const String name = "Oscillator " + tag;
            const auto row = [i] { return i; };
            tableButtons[i] = &button({}, name + " wavetable", oscillatorDisplay(i),
                                      [this, i] { showTables(i); }, ButtonStyle::display);

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
            knobs[volumeColumn] = &oscSlider(&Oscillator::level, row, name + " volume",
                                            oscillatorKnob(i, volumeColumn), 0, 1, 0.001, percent);

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
            slider(name + " rate", "RATE", { 890, 190 + dy, lfoKnobSize, lfoKnobSize }, false, 0.05, 20, 0.01,
                   [i](const Patch& p) { return p.lfos[i].rate; },
                   [i](Patch& p, double v) { p.lfos[i].rate = static_cast<float>(v); },
                   [](double v) { return String(v, 2) + " Hz"; }, 1.5);
            slider(name + " depth", "DEPTH", { 967, 190 + dy, lfoKnobSize, lfoKnobSize }, false, 0, 1, 0.001,
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
        matrixScreen.setBounds(matrixScreenBounds);
        matrixScreen.onSelect = [this](MatrixRoute route) { selectMatrixRoute(route); };
        matrixScreen.onEditAmount = [this] { matrixAmountKnob->grabKeyboardFocus(); };
        matrixScreen.onClear = [this] { clearMatrixRoute(); };
        addAndMakeVisible(matrixScreen);

        constexpr int buttonWidth = 78, buttonHeight = 29, buttonStep = buttonHeight + 8;
        const Rectangle<int> addRouteBounds { matrixAmountBounds.getCentreX() - buttonWidth / 2,
                                              matrixAmountBounds.getBottom() + 26, buttonWidth, buttonHeight };
        matrixAdd = &button("ADD ROUTE", "Add a modulation route at 0%", addRouteBounds,
                            [this] { showMatrixAddMenu(); });
        matrixTarget = &button("TARGET", "Change the destination for all routes that use this target",
                              addRouteBounds.translated(0, buttonStep), [this] { showMatrixTargets(); });
        matrixClear = &button("CLEAR", "Clear selected modulation route", addRouteBounds.translated(0, 2 * buttonStep),
                             [this] { clearMatrixRoute(); });
        const auto read = [this](const Patch& p)
        {
            return matrixSelection ? p.matrix[matrixSelection->source][matrixSelection->target].value_or(0.0f) : 0.0f;
        };
        matrixAmountKnob = &slider("Modulation amount", {}, matrixAmountBounds, false, -1, 1, 0.01, read,
            [this](Patch& p, double value)
            {
                if (matrixSelection)
                    p.matrix[matrixSelection->source][matrixSelection->target] = static_cast<float>(value);
            }, matrixAmountText);
    }

    void selectMatrixRoute(MatrixRoute route)
    {
        matrixSelection = route;
        syncControls(true);
    }

    void clearMatrixRoute()
    {
        if (!matrixSelection) return;
        const auto route = *matrixSelection;
        matrixSelection.reset();
        edit([route](Patch& p) { p.matrix[route.source][route.target].reset(); });
    }

    template <typename AddTarget>
    static void addMatrixDestinations(juce::PopupMenu& menu, AddTarget addTarget)
    {
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
    }

    void showMatrixAddMenu()
    {
        keys.release();
        const auto& p = audio.patch().sound;
        juce::PopupMenu menu;
        menu.setLookAndFeel(&skin);
        menu.addSectionHeader("Modulation source");
        for (int source = 0; source < sourceCount; ++source)
        {
            juce::PopupMenu destinations;
            addMatrixDestinations(destinations, [&](Destination target)
            {
                destinations.addItem(1 + source * destinationCount + target, destinationNames[target],
                                     true, p.matrix[source][target].has_value());
            });
            menu.addSubMenu(sourceNames[source], destinations);
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(matrixAdd),
            [safe = SafePointer<Surface>(this)](int id)
        {
            if (safe == nullptr || id <= 0) return;
            const MatrixRoute route { (id - 1) / destinationCount, static_cast<Destination>((id - 1) % destinationCount) };
            const auto& patch = safe->audio.patch().sound;
            if (patch.matrix[route.source][route.target])
            {
                safe->selectMatrixRoute(route);
                return;
            }
            safe->matrixSelection = route;
            safe->edit([route](Patch& next) { next.matrix[route.source][route.target] = 0.0f; }, true);
        });
    }

    void showMatrixTargets()
    {
        if (!matrixSelection) return;
        keys.release();
        const auto route = *matrixSelection;
        const auto& p = audio.patch().sound;
        juce::PopupMenu menu;
        menu.setLookAndFeel(&skin);
        addMatrixDestinations(menu, [&](Destination target)
        {
            const bool selected = target == route.target;
            menu.addItem(target + 1, destinationNames[target], selected || !matrixTargetInUse(p, target), selected);
        });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(matrixTarget).withMaximumNumColumns(1),
            [safe = SafePointer<Surface>(this), route](int id)
        {
            if (safe == nullptr || id <= 0 || !(safe->matrixSelection == route)) return;
            const auto target = static_cast<Destination>(id - 1);
            const auto& patch = safe->audio.patch().sound;
            if (!patch.matrix[route.source][route.target] || matrixTargetInUse(patch, target))
                return;
            safe->matrixSelection = MatrixRoute { route.source, target };
            safe->edit([route, target](Patch& next)
            {
                for (auto& row : next.matrix)
                    std::swap(row[route.target], row[target]);
            }, true);
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
            const Rectangle<int> bounds { panelInset + 13, lowerPanelY + 31 + 22 * static_cast<int>(i), 68, 20 };
            modeButtons[i] = &button(modeNames[i], descriptions[i], bounds, [this, mode]
            {
                if (audio.patch().sound.voicing.mode != mode)
                    edit([mode](Patch& p) { p.voicing.mode = mode; });
            });
        }
        slider("Glide time between overlapping Mono or Legato notes", "GLIDE", lowerKnob(118),
               false, 0, maxGlide, 0.001,
               [](const Patch& p) { return p.voicing.glide; },
               [](Patch& p, double v) { p.voicing.glide = static_cast<float>(v); },
               [](double v) { return v == 0.0 ? String("Off") : seconds(v); }, 0.15);
        slider("Unison layers per note", "UNISON", lowerKnob(194), false, 1, maxUnison, 1,
               [](const Patch& p) { return p.voicing.unison; },
               [](Patch& p, double v) { p.voicing.unison = static_cast<int>(v); },
               [](double v) { return String(static_cast<int>(v)) + "x"; });
        slider("Unison detune in cents", "DETUNE", lowerKnob(270), false, 0, maxUnisonDetune, 1,
               [](const Patch& p) { return p.voicing.detune; },
               [](Patch& p, double v) { p.voicing.detune = static_cast<float>(v); },
               [](double v) { return String(juce::roundToInt(v)) + " ct"; });
        slider("Unison stereo width", "WIDTH", lowerKnob(346), false, 0, 1, 0.01,
               [](const Patch& p) { return p.voicing.width; },
               [](Patch& p, double v) { p.voicing.width = static_cast<float>(v); }, percent);
    }

    static Rectangle<int> lowerKnob(int x) { return { x, lowerPanelY + 8, 62, 62 }; }

    template <typename Settings>
    juce::Slider& effectKnob(Settings Patch::* effect, float Settings::* member, const String& name,
                            const String& caption, int x, double min = 0.0, double max = 1.0,
                            double step = 0.01, Format format = percent, double midpoint = 0.0)
    {
        return slider(name, caption, lowerKnob(x), false, min, max, step,
                      [=](const Patch& p) { return (p.*effect).*member; },
                      [=](Patch& p, double v) { (p.*effect).*member = static_cast<float>(v); },
                      std::move(format), midpoint);
    }

    void createEffects()
    {
        tapeSwitch = &button({}, "Toggle tape delay", { tapeX, lowerPanelY + 59, 78, 27 }, [this]
        {
            edit([](Patch& p) { p.tapeDelay.enabled = !p.tapeDelay.enabled; });
        });
        effectKnob(&Patch::tapeDelay, &TapeDelaySettings::time, "Tape delay time", "TIME",
                   542, minTapeTime, maxTapeTime, 0.001, seconds, 0.32);
        effectKnob(&Patch::tapeDelay, &TapeDelaySettings::feedback, "Tape delay feedback", "FEEDBACK",
                   608, 0, maxTapeFeedback);
        effectKnob(&Patch::tapeDelay, &TapeDelaySettings::wear, "Tape wear, pitch drift and tone", "WEAR", 674);
        tapeMixKnob = &effectKnob(&Patch::tapeDelay, &TapeDelaySettings::mix, "Tape delay wet mix", "MIX", 740);

        reverbSwitch = &button({}, "Toggle plate reverb", { reverbX, lowerPanelY + 59, 78, 27 }, [this]
        {
            edit([](Patch& p) { p.reverb.enabled = !p.reverb.enabled; });
        });
        effectKnob(&Patch::reverb, &ReverbSettings::decay, "Plate reverb decay time", "DECAY", 924,
                   0, 1, 0.01, [](double v) { return seconds(plateDecaySeconds(static_cast<float>(v))); });
        effectKnob(&Patch::reverb, &ReverbSettings::damping, "Plate reverb high-frequency damping", "DAMP", 1005);
        reverbMixKnob = &effectKnob(&Patch::reverb, &ReverbSettings::mix, "Plate reverb wet mix", "MIX", 1086);
    }

    void createArpeggiator()
    {
        arpSwitch = &button({}, "Toggle arpeggiator", { arpX + 13, lowerPanelY + 35, 84, 24 }, [this]
        {
            edit([](Patch& p) { p.arp.enabled = !p.arp.enabled; });
        });
        arpDirectionButton = &button({}, "Cycle arpeggiator direction", { arpX + 13, lowerPanelY + 64, 84, 24 }, [this]
        {
            edit([](Patch& p)
            {
                const auto next = (static_cast<std::size_t>(p.arp.direction) + 1) % arpDirections.size();
                p.arp.direction = static_cast<ArpDirection>(next);
            });
        });
        slider("Arpeggiator tempo", "TEMPO", lowerKnob(arpX + 120), false, minArpTempo, maxArpTempo, 1,
               [](const Patch& p) { return p.arp.tempo; },
               [](Patch& p, double value) { p.arp.tempo = static_cast<int>(value); },
               [](double value) { return String(static_cast<int>(value)) + " BPM"; });
    }

    void createKeyboard()
    {
        masterVolume = &slider("Master output volume", "VOLUME", { 42, 681, 62, 62 }, false, 0, 1, 0.001,
                               [](const Patch& p) { return p.volume; },
                               [](Patch& p, double v) { p.volume = static_cast<float>(v); }, percent);
        octaveDown = &button("-", "Lower keyboard octave", { 405, 723, 31, 29 }, [this] { changeOctave(-1); });
        octaveUp = &button("+", "Raise keyboard octave", { 451, 723, 31, 29 }, [this] { changeOctave(1); });
        addChildComponent(keys);
        keys.setBounds(502, 686, panelRight - panelInset - 502, 74);
    }

    void setPerformanceVisible(bool show)
    {
        keys.setVisible(show);
        masterVolume->setVisible(show);
        octaveDown->setVisible(show);
        octaveUp->setVisible(show);
        const String action = show ? "Hide performance panel" : "Show performance panel";
        performanceButton->setButtonText(show ? "HIDE KEYS" : "SHOW KEYS");
        performanceButton->setToggleState(show, juce::dontSendNotification);
        performanceButton->setTitle(action);
        performanceButton->setTooltip(action);
        setSize(designWidth, show ? designHeight : compactHeight);
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
        if (skin.hasThemedMenus()) menu.setLookAndFeel(&skin);
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
        matrixSelection.reset();
        syncControls(true);
    }

    void showPresets()
    {
        keys.release();
        juce::PopupMenu menu;
        if (skin.hasThemedMenus()) menu.setLookAndFeel(&skin);
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
        if (!saving) matrixSelection.reset();
        syncControls(!saving);
    }

    void showOutputs()
    {
        keys.release();
        auto outputs = audio.outputs();
        juce::PopupMenu menu;
        if (skin.hasThemedMenus()) menu.setLookAndFeel(&skin);
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
        repaint(meterBounds);
        repaint(keys.isVisible() ? performanceStatus : headerNotice);
    }

    void paintSectionDividers(Graphics& g) const
    {
        const std::array upperSections { oscPanel, filterPanel, envPanel, lfoPanel, matrixPanel };
        const auto top = static_cast<float>(oscPanel.getY() + 8);
        const auto bottom = static_cast<float>(oscPanel.getBottom() - 8);
        g.setColour(skin.palette().enclosure.edge);
        for (std::size_t i = 1; i < upperSections.size(); ++i)
            g.drawVerticalLine((upperSections[i - 1].getRight() + upperSections[i].getX()) / 2, top, bottom);
        g.drawHorizontalLine((oscPanel.getBottom() + lowerPanelY) / 2, panelInset, panelRight);
        g.drawVerticalLine((voicingPanel.getRight() + effectsPanel.getX()) / 2, lowerPanelY + 8.0f,
                           static_cast<float>(voicingPanel.getBottom() - 8));
        g.drawVerticalLine((effectsPanel.getRight() + arpPanel.getX()) / 2, lowerPanelY + 8.0f,
                           static_cast<float>(arpPanel.getBottom() - 8));
        if (keys.isVisible())
            g.drawHorizontalLine(660, panelInset, panelRight);
    }

    void paintEnamelChassis(Graphics& g, Rectangle<float> viewport) const
    {
        constexpr float outset = 2.0f;
        constexpr float gap = 6.0f;
        const auto edge = viewport.expanded(4.0f, 0.0f);
        const auto& c = skin.palette();
        const float upperTop = oscPanel.getY() - outset;
        const float lowerTop = oscPanel.getBottom() + outset + gap;
        const float lowerBottom = voicingPanel.getBottom() + outset;
        drawEnamelPlate(g, edge.withBottom(upperTop - gap), c.header.shell);
        drawEnamelPlate(g, edge.withTop(lowerBottom + gap), c.enclosure.plate);

        const Colour olive(0xff919e55), blue(0xff7bb5c1), ochre(0xffdeb35e);
        const Colour grey(0xffc6c5be), redOrange(0xffda805d);
        for (const auto& [area, tone] : { std::pair { oscPanel, olive }, std::pair { filterPanel, blue },
                                        std::pair { envPanel, ochre }, std::pair { lfoPanel, blue },
                                        std::pair { matrixPanel, grey }, std::pair { voicingPanel, grey },
                                        std::pair { effectsPanel, redOrange }, std::pair { arpPanel, ochre } })
        {
            auto plate = area.toFloat().expanded(outset);
            if (area.getY() == lowerPanelY) plate = plate.withTop(lowerTop);
            if (area.getX() == panelInset) plate = plate.withLeft(edge.getX());
            if (area.getRight() == panelRight) plate = plate.withRight(edge.getRight());
            drawEnamelPlate(g, plate, tone);
        }
    }

    void paintSectionHeading(Graphics& g, Rectangle<int> area, const String& heading) const
    {
        label(g, heading, { area.getX() + 13, area.getY() + 10, area.getWidth() - 26, 20 }, 17.0f,
              skin.palette().legend.ink, juce::Justification::centredLeft, true);
    }

    void paintEffects(Graphics& g) const
    {
        const auto& c = skin.palette();
        paintSectionHeading(g, effectsPanel, "EFFECTS");
        g.setColour(c.enclosure.edge);
        g.drawVerticalLine(reverbDivider, lowerPanelY + 13.0f, effectsPanel.getBottom() - 13.0f);
        const auto bankLabel = [&](const String& name, int x)
        {
            label(g, name, { x, lowerPanelY + 35, 108, 20 }, 14.5f, c.legend.ink,
                  juce::Justification::centredLeft, true);
        };
        bankLabel("TAPE DELAY", tapeX);
        bankLabel("PLATE REVERB", reverbX);
    }

    void paintHeader(Graphics& g) const
    {
        const auto& c = skin.palette();
        const auto phosphor = c.display.phosphor;
        const bool enamel = skin.isEnamel();
        const bool meterDisplay = enamel || skin.isMidnight();
        if (enamel)
        {
            g.setColour(c.display.bezel);
            g.fillRoundedRectangle({ 41, 39, 41, 39 }, 3.0f);
            g.setColour(phosphor);
        }
        else if (skin.isMidnight())
        {
            g.setColour(c.legend.cool);
            g.drawRect(41, 39, 41, 39, 1);
        }
        else
        {
            g.setColour(phosphor);
            g.fillRoundedRectangle({ 41, 39, 41, 39 }, 4.0f);
            g.setColour(c.header.shell);
        }
        for (int i = 0; i < 5; ++i)
        {
            const int height = 8 + (2 - std::abs(2 - i)) * 6;
            g.fillRect(48 + i * 6, 58 - height / 2, 3, height);
        }
        label(g, "MO-T 11", { 95, 28, 144, 40 }, 36.0f, c.header.ink, juce::Justification::centredLeft, true);
        label(g, "WAVETABLE SYNTHESIZER", { 97, 66, 142, 16 }, 12.5f, c.header.quiet,
              juce::Justification::centredLeft, true);

        if (enamel)
        {
            g.setColour(c.header.edge);
            g.drawRoundedRectangle({ 440.0f, 29.0f, 382.0f, 60.0f }, 3.0f, 1.3f);
        }
        display(g, patchDisplay, c);
        const auto id = presetSlot < 0 ? String("USER") : String(presetSlot + 1).paddedLeft('0', 2) + "  FACTORY";
        pixels(g, id + (edited ? "  / EDIT" : ""), 505, 43, 1, phosphor.withAlpha(0.68f));
        pixels(g, String::fromUTF8(audio.patch().name.c_str()).substring(0, 21), 505, 58, 2, phosphor);
        if (!keys.isVisible() && juce::Time::getMillisecondCounterHiRes() < messageEnd)
            label(g, message, headerNotice, 12.5f,
                  messageIsError ? (enamel ? c.legend.warm : c.header.clip) : c.header.ink);

        if (meterDisplay)
        {
            display(g, meterBounds, c);
            pixels(g, "OUT", meterBounds.getX() + 10.0f, 44, 1, phosphor.withAlpha(0.75f));
        }
        else
            label(g, "OUT", { meterBounds.getX() + 9, 39, 54, 19 }, 12.0f, c.header.ink,
                  juce::Justification::centredLeft, true);
        const auto amount = audio.outputPeak();
        const float db = 20.0f * std::log10(std::max(0.0001f, amount));
        constexpr int segments = 13;
        const float step = integratedCaption ? (meterBounds.getWidth() - 20.0f) / segments
                                              : (meterDisplay ? 11.0f : 12.0f);
        const float ledWidth = step - (meterDisplay ? 2.5f : 3.0f);
        for (int i = 0; i < segments; ++i)
        {
            const bool lit = db > -54.0f + 4.5f * i;
            const auto colour = skin.isMidnight() ? phosphor.withAlpha(lit ? (i >= 11 ? 1.0f : 0.8f) : 0.15f)
                                                  : lit ? (i >= 11 ? c.header.clip : c.header.meter) : c.header.unlit;
            g.setColour(colour);
            const float x = meterBounds.getX() + 10.0f + step * i;
            const Rectangle<float> led { x, 61.0f, ledWidth, 9.0f };
            g.fillRoundedRectangle(led, meterDisplay ? 0.75f : 1.5f);
        }
    }

    void paintOscillators(Graphics& g) const
    {
        if (!g.clipRegionIntersects(oscPanel)) return;
        const auto& c = skin.palette();
        paintSectionHeading(g, oscPanel, "TABLE OSC");
        for (int column = 0; column < static_cast<int>(oscillatorColumns.size()); ++column)
        {
            const int x = oscillatorColumnX(column);
            label(g, oscillatorColumns[column], { x - 31, 143, 62, 20 }, 14.0f, c.legend.ink,
                  juce::Justification::centred, true);
        }
        for (int row = 0; row < oscillatorCount; ++row)
        {
            const auto area = oscillatorDisplay(row);
            g.setColour(skin.isMidnight() ? c.enclosure.edge : c.legend.quiet.withAlpha(skin.isEnamel() ? 0.35f : 0.65f));
            g.drawHorizontalLine(oscillatorKnob(row, positionColumn).getCentreY(), static_cast<float>(area.getRight()),
                                 static_cast<float>(oscillatorColumnX(volumeColumn)));
            paintWavetable(g, row, area);
            for (auto* knob : oscillatorKnobs[row])
            {
                const auto bounds = knob->getBounds();
                const Rectangle<int> readout { bounds.getX() + 8, bounds.getBottom() + 1, bounds.getWidth() - 16, 16 };
                label(g, knob->getTextFromValue(knob->getValue()), readout, 12.0f,
                      knob->isEnabled() ? c.legend.ink : c.legend.quiet, juce::Justification::centred);
            }
        }

        g.setColour(c.enclosure.edge);
        g.drawHorizontalLine(470, 37.0f, 482.0f);
        label(g, "OSC SETUP", { 38, 477, 90, 18 }, 11.0f, c.legend.quiet, juce::Justification::centredLeft, true);
        label(g, "PHASE MODE", { 132, 477, 73, 18 }, 11.0f, c.legend.quiet, juce::Justification::centred, true);
        label(g, "DISPLAY", { 201, 477, 69, 18 }, 11.0f, c.legend.quiet, juce::Justification::centred, true);
        static constexpr const char* captions[] { "OCT", "PAN", "PHASE" };
        for (int i = 0; i < static_cast<int>(setupKnobs.size()); ++i)
        {
            auto& knob = *setupKnobs[i];
            const auto text = String(captions[i]) + " " + knob.getTextFromValue(knob.getValue());
            label(g, text, { knob.getBounds().getCentreX() - 33, 511, 66, 15 }, 11.0f,
                  c.legend.quiet, juce::Justification::centred);
        }
    }

    void paintWavetable(Graphics& g, int row, Rectangle<int> area) const
    {
        const auto& palette = skin.palette();
        const auto phosphor = palette.display.phosphor;
        display(g, area, palette);
        const auto& osc = audio.patch().sound.osc[row];
        const auto x = static_cast<float>(area.getX() + 11), end = static_cast<float>(area.getRight() - 11);
        const auto top = static_cast<float>(area.getY());
        pixels(g, "OSC " + String(oscillatorNames[row]), x, top + 11, 1, phosphor.withAlpha(row == engine ? 1.0f : 0.65f));
        pixels(g, tableNames[osc.table], x, top + 26, displayTextScale, phosphor);
        g.setColour(phosphor.withAlpha(0.7f));
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
            g.setColour(phosphor.withAlpha(strength));
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
            g.setColour(phosphor.withAlpha(0.19f));
            g.drawHorizontalLine(area.getY() + 65, x, end);
            plot(osc.position, top + 65.0f, 21.0f, 0.95f, 1.65f);
        }
    }

    void paintLfoAndEnvelope(Graphics& g) const
    {
        const auto& c = skin.palette();
        g.setColour(c.enclosure.edge);
        for (const auto& r : { envPanel, lfoPanel })
            g.drawHorizontalLine(336, static_cast<float>(r.getX() + 12), static_cast<float>(r.getRight() - 12));
        for (int i = 0; i < 2; ++i)
        {
            label(g, i == 0 ? "FILTER ADSR" : "AMP ADSR", { 669, 155 + 190 * i, 181, 22 }, 12.5f, c.legend.cool,
                  juce::Justification::centredLeft, true);
            label(g, "LFO " + String(i + 1), { 889, 155 + 190 * i, 137, 22 }, 12.5f, c.legend.cool,
                  juce::Justification::centredLeft, true);
        }
    }

    void paintPerformance(Graphics& g) const
    {
        const auto& c = skin.palette();
        const bool statusDisplay = skin.isEnamel() || skin.isMidnight();
        if (statusDisplay)
        {
            display(g, { 120, 709, 259, 52 }, c);
            const auto keyBed = keys.getBounds().toFloat().expanded(5.0f, 4.0f);
            g.setColour(c.control.base);
            g.fillRoundedRectangle(keyBed, 2.0f);
            g.setColour(c.enclosure.edge);
            g.drawRoundedRectangle(keyBed, 2.0f, 1.0f);
        }
        g.setColour(c.legend.quiet.withAlpha(0.5f));
        g.drawVerticalLine(113, 686.0f, 758.0f);
        g.drawVerticalLine(385, 686.0f, 758.0f);
        label(g, "PERFORMANCE", { 128, 687, 244, 20 }, 13.0f, c.legend.ink, juce::Justification::centredLeft, true);
        const auto info = "MIDI " + String(audio.inputCount()) + " IN  /  " + String(audio.voiceCount()).paddedLeft('0', 2) + " VOICES";
        pixels(g, info, 128, 718, 1, statusDisplay ? c.display.phosphor : c.legend.cool.darker(0.2f));
        g.setColour(statusDisplay ? c.display.phosphor.withAlpha(audio.hasRecentMidiNote() ? 1.0f : 0.22f)
                                  : audio.hasRecentMidiNote() ? c.legend.warm : c.legend.quiet.withAlpha(0.25f));
        g.fillEllipse(134.0f + 6.0f * info.length(), 719.0f, 5.0f, 5.0f);
        const bool showNotice = juce::Time::getMillisecondCounterHiRes() < messageEnd;
        const auto status = showNotice ? message : audio.unavailableInputs() > 0
            ? String(audio.unavailableInputs()) + " MIDI input(s) unavailable" : audio.audioStatus();
        const auto statusInk = statusDisplay ? c.display.phosphor.withAlpha(showNotice && messageIsError ? 1.0f : 0.76f)
                                            : showNotice && messageIsError ? c.legend.warm : c.legend.quiet;
        label(g, status, { 128, 736, 247, 19 }, 12.5f, statusInk);
        label(g, "KEY OCT " + String(octave), { 399, 689, 89, 25 }, 12.0f, c.legend.ink,
              juce::Justification::centred, true);
    }

    Standalone& audio;
    const std::function<void()> designSizeChanged;
    const bool integratedCaption;
    const Rectangle<int> meterBounds { integratedCaption ? Rectangle<int>(1062, 35, 126, 48)
                                                         : Rectangle<int>(1092, 35, 164, 48) };
    const Rectangle<int> panicBounds { integratedCaption ? Rectangle<int>(1196, 44, 64, 30)
                                                         : Rectangle<int>(panelRight - 111, 44, 86, 30) };
    const Rectangle<int> headerNotice { 839, 78, panicBounds.getRight() - 839, 15 };
    Skin skin;
    PerformanceKeys keys;
    juce::TooltipWindow tooltips { this, 450 };
    std::vector<PanelButton> buttons;
    std::vector<BoundSlider> sliders;
    MatrixDisplay matrixScreen { skin };
    std::optional<MatrixRoute> matrixSelection;
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
    juce::TextButton* themeButton = nullptr;
    juce::TextButton* audioButton = nullptr;
    juce::TextButton* performanceButton = nullptr;
    WindowButton* maximiseControl = nullptr;
    juce::TextButton* octaveDown = nullptr;
    juce::TextButton* octaveUp = nullptr;
    juce::TextButton* matrixAdd = nullptr;
    juce::TextButton* matrixTarget = nullptr;
    juce::TextButton* matrixClear = nullptr;
    juce::TextButton* tapeSwitch = nullptr;
    juce::TextButton* reverbSwitch = nullptr;
    juce::TextButton* arpSwitch = nullptr;
    juce::TextButton* arpDirectionButton = nullptr;
    juce::Slider* masterVolume = nullptr;
    juce::Slider* matrixAmountKnob = nullptr;
    juce::Slider* tapeMixKnob = nullptr;
    juce::Slider* reverbMixKnob = nullptr;
    int engine = 0, octave = 4, presetSlot = -1;
    bool edited = false, messageIsError = false;
    String message;
    double messageEnd = 0.0;
};

Panel::Panel(Standalone& audio, std::function<void()> onDesignSizeChanged,
             std::function<void(WindowControlKind)> onWindowControl)
    : surface(std::make_unique<Surface>(audio, [this, onDesignSizeChanged = std::move(onDesignSizeChanged)]
      {
          onDesignSizeChanged();
          resized();
          repaint();
      }, onWindowControl))
{
    setOpaque(true);
    addAndMakeVisible(*surface);
    const auto design = designSize();
    setSize(design.x, design.y);
}

Panel::~Panel() = default;

juce::Point<int> Panel::designSize() const
{
    const auto face = surface->faceplateBounds();
    return { face.getWidth() + (surface->hasWoodCheeks() ? 2 * woodCheekWidth : 0), face.getHeight() };
}

bool Panel::playComputerKey(const juce::KeyPress& press) { return surface->playComputerKey(press); }
bool Panel::releaseComputerKeys() { return surface->releaseComputerKeys(); }
void Panel::releasePerformanceKeys() { surface->releasePerformanceKeys(); }
Panel::WindowControlKind Panel::windowControlAt(juce::Point<float> point) const
{
    return surface->windowControlAt(surface->getLocalPoint(this, point));
}

void Panel::setWindowMaximised(bool maximised) { surface->setWindowMaximised(maximised); }

void Panel::paint(Graphics& g)
{
    const auto transform = surface->getTransform();
    g.addTransform(transform);
    surface->paintChassis(g, getLocalBounds().toFloat().transformedBy(transform.inverted()));
}

void Panel::resized()
{
    if (getLocalBounds().isEmpty()) return;
    const auto design = designSize();
    const float scale = std::min(getWidth() / static_cast<float>(design.x), getHeight() / static_cast<float>(design.y));
    const float y = surface->hasTopRail() ? (getHeight() - design.y * scale) * 0.5f
                                          : -surface->faceplateBounds().getY() * scale;
    surface->setTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth() - designWidth * scale) * 0.5f, y));
}
}
