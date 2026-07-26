cmake_minimum_required(VERSION 3.22)
project(Refrakt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C:/Projects/JUCE only ever exists on this one dev machine — anywhere
# else (CI, another contributor, a fresh clone) needs JUCE fetched fresh.
# Reuses the local checkout when it's present (fast, no network) and falls
# back to cloning the exact same tagged version otherwise, so CI and local
# builds compile against identical JUCE code.
include(FetchContent)
if(EXISTS "C:/Projects/JUCE/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "C:/Projects/JUCE")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14
        GIT_SHALLOW TRUE
    )
endif()
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Refrakt
    COMPANY_NAME "SkeptikAudio"
    PLUGIN_MANUFACTURER_CODE Skat
    PLUGIN_CODE Rfkt
    FORMATS VST3 AU
    PRODUCT_NAME "Refrakt"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    JUCE_GENERATE_JUCE_HEADER ON
    VST3_CATEGORIES "Fx" "Spatial"
    NEEDS_WEBVIEW2 TRUE
)

target_sources(Refrakt PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
)

# Ships the actual UI inside the plugin binary instead of loading it from a
# hardcoded dev-machine path (C:/Projects/Refrakt/...) — that path only
# ever existed on this machine, so any other machine (tester, Mac build)
# would just get a blank editor window without this. The dev-time file://
# hot-reload path (see PluginEditor.cpp) still works unchanged when that
# path happens to exist; this is what every other machine falls back to.
juce_add_binary_data(RefraktData SOURCES
    Source/ui/public/index.html
    Source/ui/public/REFRAKT_small_trans.svg
)
target_link_libraries(Refrakt PRIVATE RefraktData)

target_compile_definitions(Refrakt PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Refrakt PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_processors
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)