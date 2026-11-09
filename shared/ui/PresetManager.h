// Factory + user presets, and A/B compare, for any APVTS-based plug-in
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace hbui
{
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // parameter ID -> real (not normalised) value
};

class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& s, juce::String productFolder, std::vector<FactoryPreset> factoryList)
        : apvts (s), folderName (std::move (productFolder)), factory (std::move (factoryList)) {}

    static constexpr const char* userPrefix = "User: ";

    juce::File getUserFolder() const
    {
       #if JUCE_MAC
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
       #else
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #endif
        return base.getChildFile ("Detroit Hallelujah").getChildFile (folderName).getChildFile ("Presets");
    }

    juce::StringArray getNames() const
    {
        juce::StringArray names;
        for (auto& p : factory) names.add (p.name);
        for (auto& f : getUserFolder().findChildFiles (juce::File::findFiles, false, "*.xml"))
            names.add (userPrefix + f.getFileNameWithoutExtension());
        return names;
    }

    bool isUserPreset (const juce::String& name) const { return name.startsWith (userPrefix); }

    void load (const juce::String& name)
    {
        if (isUserPreset (name))
        {
            auto file = getUserFolder().getChildFile (name.fromFirstOccurrenceOf (userPrefix, false, false) + ".xml");
            if (auto xml = juce::XmlDocument::parse (file))
            {
                auto tree = juce::ValueTree::fromXml (*xml);
                if (tree.hasType (apvts.state.getType()))
                    applyTree (tree);
            }
        }
        else
        {
            for (auto& p : factory)
                if (p.name == name)
                {
                    resetToDefaults();
                    for (auto& [pid, value] : p.values)
                        setParam (pid, value);
                }
        }
        currentName = name;
    }

    bool save (const juce::String& rawName)
    {
        auto clean = juce::File::createLegalFileName (rawName.trim());
        if (clean.isEmpty()) return false;
        auto folder = getUserFolder();
        if (! folder.createDirectory()) return false;
        if (auto xml = apvts.copyState().createXml())
        {
            if (xml->writeTo (folder.getChildFile (clean + ".xml")))
            {
                currentName = userPrefix + clean;
                return true;
            }
        }
        return false;
    }

    bool remove (const juce::String& name)
    {
        if (! isUserPreset (name)) return false;
        auto ok = getUserFolder().getChildFile (name.fromFirstOccurrenceOf (userPrefix, false, false) + ".xml").deleteFile();
        if (ok && currentName == name) currentName.clear();
        return ok;
    }

    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }

    // ---- A/B compare -------------------------------------------------------------
    int getActiveSlot() const noexcept { return activeSlot; }

    void selectSlot (int slot)
    {
        if (slot == activeSlot) return;
        slots[(size_t) activeSlot] = apvts.copyState();
        activeSlot = slot;
        if (slots[(size_t) slot].isValid()) applyTree (slots[(size_t) slot]);
        else slots[(size_t) slot] = apvts.copyState();
    }

    /** Copies the active setting into the other slot. */
    void copyActiveToOther()
    {
        slots[(size_t) (1 - activeSlot)] = apvts.copyState();
    }

private:
    void setParam (const juce::String& pid, float value)
    {
        if (auto* p = apvts.getParameter (pid))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    void resetToDefaults()
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
    }

    /** Applies every stored parameter value so the host is notified of each change. */
    void applyTree (const juce::ValueTree& tree)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                auto child = tree.getChildWithProperty ("id", rp->getParameterID());
                if (child.isValid() && child.hasProperty ("value"))
                {
                    rp->beginChangeGesture();
                    rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
                    rp->endChangeGesture();
                }
            }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String folderName;
    std::vector<FactoryPreset> factory;
    juce::String currentName;
    std::array<juce::ValueTree, 2> slots;
    int activeSlot = 0;
};

} // namespace hbui
