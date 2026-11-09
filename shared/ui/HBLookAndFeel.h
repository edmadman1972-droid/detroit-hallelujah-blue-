// Detroit Hallelujah - shared look and feel (deep Detroit blue)
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace hbui
{
namespace colours
{
    const juce::Colour background   { 0xff070f22 };
    const juce::Colour panel        { 0xff0e1c3d };
    const juce::Colour panelEdge    { 0xff1d3266 };
    const juce::Colour accent       { 0xff2f7bff };   // Hallelujah blue
    const juce::Colour accentBright { 0xff7fb2ff };
    const juce::Colour gold         { 0xfff2c14e };   // EQ curve, highlights
    const juce::Colour text         { 0xffdce6ff };
    const juce::Colour textDim      { 0xff7f93c6 };
    const juce::Colour meterGreen   { 0xff38d39f };
    const juce::Colour meterYellow  { 0xfff2c14e };
    const juce::Colour meterRed     { 0xffff5a5f };
}

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, colours::background);
        setColour (Slider::textBoxTextColourId, colours::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.4f));
        setColour (Label::textColourId, colours::text);
        setColour (ComboBox::backgroundColourId, colours::background);
        setColour (ComboBox::outlineColourId, colours::panelEdge);
        setColour (ComboBox::textColourId, colours::text);
        setColour (ComboBox::arrowColourId, colours::accentBright);
        setColour (PopupMenu::backgroundColourId, colours::panel);
        setColour (PopupMenu::textColourId, colours::text);
        setColour (PopupMenu::highlightedBackgroundColourId, colours::accent);
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (TextButton::buttonColourId, colours::panel);
        setColour (TextButton::buttonOnColourId, colours::accent);
        setColour (TextButton::textColourOffId, colours::text);
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (ToggleButton::textColourId, colours::text);
        setColour (ToggleButton::tickColourId, colours::accentBright);
        setColour (ToggleButton::tickDisabledColourId, colours::textDim);
        setColour (TextEditor::backgroundColourId, colours::background);
        setColour (TextEditor::textColourId, colours::text);
        setColour (TextEditor::outlineColourId, colours::panelEdge);
        setColour (AlertWindow::backgroundColourId, colours::panel);
        setColour (AlertWindow::textColourId, colours::text);
        setColour (AlertWindow::outlineColourId, colours::accent);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace juce;
        auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float track = jmax (2.5f, radius * 0.12f);
        const bool enabled = slider.isEnabled();

        // background arc
        Path bg;
        bg.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::panelEdge);
        g.strokePath (bg, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

        // value arc (bipolar params start from the middle)
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float from = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                   : startAngle;
        Path value;
        value.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f,
                             jmin (from, angle), jmax (from, angle), true);
        g.setColour (enabled ? colours::accent : colours::textDim);
        g.strokePath (value, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

        // knob body
        const float bodyR = radius - track * 2.2f;
        ColourGradient grad (colours::panel.brighter (0.35f), centre.x, centre.y - bodyR,
                             colours::background, centre.x, centre.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (colours::panelEdge.brighter (0.2f));
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        // pointer
        Path pointer;
        const float pw = jmax (2.0f, bodyR * 0.09f);
        pointer.addRoundedRectangle (-pw * 0.5f, -bodyR * 0.92f, pw, bodyR * 0.45f, pw * 0.5f);
        pointer.applyTransform (AffineTransform::rotation (angle).translated (centre));
        g.setColour (enabled ? colours::accentBright : colours::textDim);
        g.fillPath (pointer);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bgColour;
        if (down) c = c.brighter (0.2f);
        else if (highlighted) c = c.brighter (0.1f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (b.getToggleState() ? colours::accentBright : colours::panelEdge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int height) override
    {
        return font (juce::jmin (13.0f, (float) height * 0.55f), true);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.5f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }
};

/** Rounded section panel with a title, used to group controls. */
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (colours::accentBright);
        g.setFont (font (12.5f, true));
        g.drawText (title, r.removeFromTop (24.0f).reduced (10.0f, 0.0f), juce::Justification::centredLeft);
    }
}

} // namespace hbui
