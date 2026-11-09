juce_add_plugin (HallelujahBlue
    PRODUCT_NAME                  "Detroit Hallelujah Blue"
    COMPANY_NAME                  "Detroit Hallelujah Audio"
    BUNDLE_ID                     "com.detroithallelujah.blue"
    PLUGIN_MANUFACTURER_CODE      Dthb
    PLUGIN_CODE                   Hbvc
    FORMATS                       ${HB_FORMATS} Standalone
    VERSION                       ${PROJECT_VERSION}
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              FALSE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   FALSE
    VST3_CATEGORIES               Fx Dynamics EQ
    AU_MAIN_TYPE                  kAudioUnitType_Effect
    AAX_CATEGORY                  AAX_ePlugInCategory_Dynamics
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "The standalone app needs your microphone to process your voice."
    COPY_PLUGIN_AFTER_BUILD       ${HB_COPY_AFTER_BUILD})

target_sources (HallelujahBlue PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories (HallelujahBlue PRIVATE
    Source
    "${HB_SHARED_DIR}/dsp"
    "${HB_SHARED_DIR}/ui")

target_compile_definitions (HallelujahBlue PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries (HallelujahBlue
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
