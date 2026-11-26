juce_add_plugin(H7600Control
    PRODUCT_NAME "H7600 Control"
    COMPANY_NAME "h7600-control"
    BUNDLE_ID "org.h7600control.plugin"
    PLUGIN_MANUFACTURER_CODE H7ct
    PLUGIN_CODE H76c
    FORMATS AU VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Tools)

target_sources(H7600Control PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/H7600Link.cpp)

target_compile_definitions(H7600Control PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(H7600Control
    PRIVATE
        h7600_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
