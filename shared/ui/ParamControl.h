// A control that builds itself from an APVTS parameter: a knob for floats,
// a drop-down for choices and a switch for on/off parameters.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "HBLookAndFeel.h"

namespace hbui
{
class ParamControl : public juce::Component
{
public:
    enum class Kind { Knob, Choice, Toggle };

    ParamControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
        : label (labelText)
    {
        auto* param = apvts.getParameter (paramID);
        jassert (param != nullptr);   // unknown parameter ID

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            kind = Kind::Choice;
            combo.addItemList (choice->choices, 1);
            addAndMakeVisible (combo);
            comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, combo);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
        {
            kind = Kind::Toggle;
            toggle.setButtonText (labelText);
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, toggle);
        }
        else
        {
            kind = Kind::Knob;
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                        juce::MathConstants<float>::pi * 2.75f, true);
            slider.setTooltip (param->getName (64));
            addAndMakeVisible (slider);
            sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
            slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        }
    }

    Kind getKind() const noexcept { return kind; }

    /** Preferred height for drop-downs and switches (knobs fill their grid cell). */
    int getRowHeight() const noexcept { return kind == Kind::Choice ? 40 : 24; }

    void paint (juce::Graphics& g) override
    {
        if (kind == Kind::Toggle)
            return;
        g.setColour (colours::textDim);
        g.setFont (font (11.5f, true));
        g.drawText (label.toUpperCase(), getLocalBounds().removeFromTop (15), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        switch (kind)
        {
            case Kind::Knob:   r.removeFromTop (15); slider.setBounds (r); break;
            case Kind::Choice: r.removeFromTop (15); combo.setBounds (r.removeFromTop (24)); break;
            case Kind::Toggle: toggle.setBounds (r); break;
        }
    }

private:
    Kind kind = Kind::Knob;
    juce::String label;
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamControl)
};

/** Lays its children out in one row of equal widths. */
class ButtonRow : public juce::Component
{
public:
    void resized() override
    {
        auto r = getLocalBounds();
        const int n = getNumChildComponents();
        if (n == 0) return;
        const int w = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
            getChildComponent (i)->setBounds (i == n - 1 ? r.reduced (1) : r.removeFromLeft (w).reduced (1));
    }
};

/** A titled group of controls. Each control can be marked "advanced only". */
class Section : public juce::Component
{
public:
    explicit Section (juce::String t) : title (std::move (t)) {}

    ParamControl& add (juce::AudioProcessorValueTreeState& apvts, const juce::String& pid,
                       const juce::String& label, bool advancedOnly = false)
    {
        auto* c = controls.add (new ParamControl (apvts, pid, label));
        advanced.add (advancedOnly);
        addAndMakeVisible (c);
        return *c;
    }

    /** Extra component (e.g. a row of buttons) placed at the bottom of the section. */
    void setFooter (juce::Component* c, int height) { footer = c; footerHeight = height; if (c) addAndMakeVisible (c); }

    void setAdvanced (bool shouldShowAdvanced)
    {
        showAdvanced = shouldShowAdvanced;
        for (int i = 0; i < controls.size(); ++i)
            controls[i]->setVisible (showAdvanced || ! advanced[i]);
        resized();
    }

    void paint (juce::Graphics& g) override { drawPanel (g, getLocalBounds().toFloat().reduced (3.0f), title); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        r.removeFromTop (22);
        if (footer != nullptr)
            footer->setBounds (r.removeFromBottom (footerHeight));

        juce::Array<ParamControl*> knobs;
        for (auto* c : controls)
        {
            if (! c->isVisible()) continue;
            if (c->getKind() == ParamControl::Kind::Knob) knobs.add (c);
            else c->setBounds (r.removeFromTop (c->getRowHeight()));
        }
        if (knobs.isEmpty()) return;

        // two knobs sit side by side in a wide section, stacked in a tall one
        const int cols = knobs.size() <= 2 ? (r.getWidth() > r.getHeight() ? knobs.size() : 1) : 2;
        const int rows = (knobs.size() + cols - 1) / cols;
        const int cellW = r.getWidth() / cols;
        const int cellH = juce::jmin (r.getHeight() / rows, 150);
        const int top = r.getY() + (r.getHeight() - cellH * rows) / 2;
        for (int i = 0; i < knobs.size(); ++i)
        {
            const int col = i % cols, row = i / cols;
            // centre a lone knob on the last row
            const int xOff = (cols == 2 && i == knobs.size() - 1 && knobs.size() % 2 == 1) ? cellW / 2 : 0;
            knobs[i]->setBounds (r.getX() + col * cellW + xOff, top + row * cellH, cellW, cellH);
        }
    }

private:
    juce::String title;
    juce::OwnedArray<ParamControl> controls;
    juce::Array<bool> advanced;
    juce::Component* footer = nullptr;
    int footerHeight = 0;
    bool showAdvanced = false;
};

} // namespace hbui
