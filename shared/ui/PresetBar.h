// Preset browser + save/delete + A/B compare strip
#pragma once

#include "PresetManager.h"
#include "HBLookAndFeel.h"

namespace hbui
{
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager& pm) : presets (pm)
    {
        addAndMakeVisible (presetBox);
        presetBox.setTextWhenNothingSelected ("Choose a preset");
        presetBox.onChange = [this]
        {
            const int idx = presetBox.getSelectedItemIndex();
            if (idx >= 0 && ! refreshing)
            {
                presets.load (names[idx]);
                deleteButton.setEnabled (presets.isUserPreset (names[idx]));
            }
        };

        for (auto* b : { &saveButton, &deleteButton, &aButton, &bButton, &copyButton })
            addAndMakeVisible (b);

        saveButton.onClick = [this] { askForName(); };
        deleteButton.onClick = [this]
        {
            const int idx = presetBox.getSelectedItemIndex();
            if (idx >= 0 && presets.remove (names[idx])) refresh();
        };

        aButton.setClickingTogglesState (false);
        bButton.setClickingTogglesState (false);
        aButton.onClick = [this] { presets.selectSlot (0); updateAB(); };
        bButton.onClick = [this] { presets.selectSlot (1); updateAB(); };
        copyButton.onClick = [this] { presets.copyActiveToOther(); };
        copyButton.setTooltip ("Copy the active setting to the other slot");

        refresh();
        updateAB();
    }

    void refresh()
    {
        const juce::ScopedValueSetter<bool> svs (refreshing, true);
        names = presets.getNames();
        presetBox.clear (juce::dontSendNotification);
        for (int i = 0; i < names.size(); ++i)
        {
            if (i > 0 && presets.isUserPreset (names[i]) && ! presets.isUserPreset (names[i - 1]))
                presetBox.addSeparator();
            presetBox.addItem (names[i], i + 1);
        }
        const int current = names.indexOf (presets.getCurrentName());
        presetBox.setSelectedItemIndex (current, juce::dontSendNotification);
        deleteButton.setEnabled (current >= 0 && presets.isUserPreset (names[current]));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        copyButton.setBounds (r.removeFromRight (50).reduced (2));
        bButton.setBounds (r.removeFromRight (30).reduced (2));
        aButton.setBounds (r.removeFromRight (30).reduced (2));
        r.removeFromRight (8);
        deleteButton.setBounds (r.removeFromRight (56).reduced (2));
        saveButton.setBounds (r.removeFromRight (50).reduced (2));
        presetBox.setBounds (r.reduced (2));
    }

private:
    void updateAB()
    {
        aButton.setToggleState (presets.getActiveSlot() == 0, juce::dontSendNotification);
        bButton.setToggleState (presets.getActiveSlot() == 1, juce::dontSendNotification);
    }

    void askForName()
    {
        dialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name this sound:", juce::MessageBoxIconType::NoIcon);
        dialog->setLookAndFeel (&getLookAndFeel());
        auto current = presets.getCurrentName();
        dialog->addTextEditor ("name", presets.isUserPreset (current) ? current.fromFirstOccurrenceOf (PresetManager::userPrefix, false, false) : juce::String());
        dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PresetBar> safeThis (this);
        dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis] (int result)
        {
            if (safeThis == nullptr || safeThis->dialog == nullptr || result != 1) return;
            const auto name = safeThis->dialog->getTextEditorContents ("name");
            if (safeThis->presets.save (name))
                safeThis->refresh();
        }), false);
    }

    PresetManager& presets;
    juce::StringArray names;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save" }, deleteButton { "Delete" }, aButton { "A" }, bButton { "B" }, copyButton { "Copy" };
    std::unique_ptr<juce::AlertWindow> dialog;
    bool refreshing = false;
};

} // namespace hbui
