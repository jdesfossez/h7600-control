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

juce_add_binary_data(H7600Data SOURCES
    ${PROJECT_SOURCE_DIR}/resources/presets.json
    ${PROJECT_SOURCE_DIR}/resources/fonts/VT323-Regular.ttf)
set_target_properties(H7600Data PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(H7600_PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/H7600Link.cpp
    Source/UserStore.cpp
    Source/ModuleDbStore.cpp
    Source/ui/BrowserView.cpp
    Source/ui/ParamsView.cpp)

target_sources(H7600Control PRIVATE ${H7600_PLUGIN_SOURCES})

# No web browser or curl: nothing here needs them (JUCE only turns them off by itself on Linux).
target_compile_definitions(H7600Control PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(H7600Control
    PRIVATE
        h7600_core
        H7600Data
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tool: renders the editor to PNG files (see tools/Snapshot.cpp for its modes).
juce_add_console_app(H7600Snapshot PRODUCT_NAME "H7600Snapshot")
target_sources(H7600Snapshot PRIVATE tools/Snapshot.cpp ${H7600_PLUGIN_SOURCES})
target_compile_definitions(H7600Snapshot PRIVATE
    JucePlugin_Name="H7600 Control" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(H7600Snapshot PRIVATE h7600_core H7600Data
    juce::juce_audio_utils juce::juce_recommended_config_flags)
