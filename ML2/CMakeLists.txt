cmake_minimum_required(VERSION 3.22)
project(ML2 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ML2
    PRODUCT_NAME              "Мартин лох ML-2"
    BUNDLE_ID                 com.oxideaudio.martinloh.ml2
    COMPANY_NAME              "Oxide Audio"
    PLUGIN_MANUFACTURER_CODE  Oxau
    PLUGIN_CODE               OxGl
    FORMATS                   VST3 AU Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    VST3_CATEGORIES           Fx
    AU_MAIN_TYPE              kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD   FALSE)

target_sources(ML2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ML2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# кирилиця в рядках коду
target_compile_options(ML2 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(ML2
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
