cmake_minimum_required(VERSION 3.22)
project(PastelDrumSynth VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ModernDrumSynth
    COMPANY_NAME "Pastel Drum Synth"
    BUNDLE_ID "com.pasteldrumsynth.pasteldrumsynth"
    DESCRIPTION "Modern analog-style 12-part drum synthesizer with per-part and global FX"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Past
    PLUGIN_CODE PaD1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Pastel Drum Synth"
)

juce_generate_juce_header(ModernDrumSynth)

target_sources(ModernDrumSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetBrowser.cpp
    Source/dsp/DrumEngine.cpp
    Source/dsp/Effects.cpp
    Source/Presets.cpp
    Source/PresetFiles.cpp
)

target_include_directories(ModernDrumSynth PRIVATE Source Source/dsp)

target_compile_definitions(ModernDrumSynth PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ModernDrumSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

add_executable(SmokeTest TestSmoke.cpp Source/dsp/DrumEngine.cpp Source/dsp/Effects.cpp Source/Presets.cpp)
target_include_directories(SmokeTest PRIVATE Source Source/dsp
    ${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/JuceLibraryCode)
target_link_libraries(SmokeTest PRIVATE juce::juce_dsp juce::juce_audio_basics juce::juce_graphics juce::juce_data_structures juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags curl)

# PresetDump: recipe -> Factory/*.xml data-file generator (never shipped)
add_executable(PresetDump PresetDump.cpp Source/dsp/DrumEngine.cpp Source/dsp/Effects.cpp Source/Presets.cpp)
target_include_directories(PresetDump PRIVATE Source Source/dsp
    ${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/JuceLibraryCode)
target_link_libraries(PresetDump PRIVATE juce::juce_dsp juce::juce_audio_basics juce::juce_graphics juce::juce_data_structures juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags curl)

# E2ETest: real processor + APVTS path headless (no editor created)
add_executable(E2ETest TestE2E.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PresetBrowser.cpp
    Source/dsp/DrumEngine.cpp Source/dsp/Effects.cpp Source/Presets.cpp Source/PresetFiles.cpp)
target_include_directories(E2ETest PRIVATE Source Source/dsp
    ${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/JuceLibraryCode)
target_link_libraries(E2ETest PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_dsp
    juce::juce_audio_basics juce::juce_graphics juce::juce_data_structures juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags curl)
target_compile_definitions(E2ETest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

# Ship factory presets inside the VST3 bundle + next to the standalone
add_custom_target(PastelFactory ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/$<CONFIG>/VST3/Pastel Drum Synth.vst3/Contents/Resources/Factory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Factory"
        "${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/$<CONFIG>/VST3/Pastel Drum Synth.vst3/Contents/Resources/Factory"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/$<CONFIG>/Standalone/Factory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Factory"
        "${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/$<CONFIG>/Standalone/Factory"
)
add_dependencies(PastelFactory ModernDrumSynth)

# KitProbe: renders factory recipes through the real processor, prints
# per-part metrics + writes audition WAVs (dev tool, never shipped)
add_executable(KitProbe KitProbe.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PresetBrowser.cpp
    Source/dsp/DrumEngine.cpp Source/dsp/Effects.cpp Source/Presets.cpp Source/PresetFiles.cpp)
target_include_directories(KitProbe PRIVATE Source Source/dsp
    ${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/JuceLibraryCode)
target_link_libraries(KitProbe PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics juce::juce_dsp
    juce::juce_audio_basics juce::juce_graphics juce::juce_data_structures juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags curl)
target_compile_definitions(KitProbe PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

# UiShot: opens the real editor offscreen and writes PNG snapshots of every
# view/tab (dev tool, never shipped)
add_executable(UiShot UiShot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PresetBrowser.cpp
    Source/dsp/DrumEngine.cpp Source/dsp/Effects.cpp Source/Presets.cpp Source/PresetFiles.cpp)
target_include_directories(UiShot PRIVATE Source Source/dsp
    ${CMAKE_CURRENT_BINARY_DIR}/ModernDrumSynth_artefacts/JuceLibraryCode)
target_link_libraries(UiShot PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_dsp
    juce::juce_audio_basics juce::juce_graphics juce::juce_data_structures juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags curl)
target_compile_definitions(UiShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)
