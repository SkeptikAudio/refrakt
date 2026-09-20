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

# Regression suite for CurveMath.h — the pure DSP/curve math that produced
# several bugs this project only ever caught by eye (pivot spike, ping-pong
# jerk, bass-bucket starvation). Standalone executable, no plugin/DAW needed,
# just juce_core for the math helpers CurveMath.h itself depends on.
enable_testing()
add_executable(RefraktTests Tests/CurveMathTests.cpp)
target_compile_definitions(RefraktTests PRIVATE JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(RefraktTests PRIVATE juce::juce_core)
add_test(NAME CurveMathTests COMMAND RefraktTests)

# Headless sample-rate / buffer-size sweep, driving the real RefraktProcessor
# (not a reimplementation) directly through processBlock — no DAW, no
# editor window. Compiles PluginProcessor.cpp/PluginEditor.cpp/Presets.cpp
# same as the plugin itself (createEditor() ties them together at link time)
# so this mirrors production code exactly, it just never constructs the
# editor at runtime.
add_executable(RefraktAudioTests
    Tests/AudioProcessingTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
)
target_link_libraries(RefraktAudioTests PRIVATE RefraktData)
target_compile_definitions(RefraktAudioTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(RefraktAudioTests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_processors
)
if(WIN32)
    # juce_add_plugin(... NEEDS_WEBVIEW2 TRUE) wires this into the Refrakt
    # target automatically (including putting FindWebView2.cmake on the
    # module path); a plain add_executable needs both done by hand since
    # PluginEditor.cpp (WebBrowserComponent) is compiled into this too.
    list(APPEND CMAKE_MODULE_PATH "${juce_SOURCE_DIR}/extras/Build/CMake")
    find_package(WebView2 MODULE)
    if(TARGET juce::juce_webview2)
        target_link_libraries(RefraktAudioTests PRIVATE juce::juce_webview2)
    endif()
endif()
add_test(NAME AudioProcessingTests COMMAND RefraktAudioTests)
# GUI fit regression check: loads the real index.html in headless Edge (in
# embedded mode, with a stubbed JUCE bridge) at many window sizes and JUCE/WebView2
# DPI ratios, and asserts the GUI is never cropped, never mis-scaled and never
# shows a scrollbar (see the header of Tests/gui_fit_check.js). Skips itself
# (exit 0) where Edge isn't installed, and is only registered when Node is available.
find_program(REFRAKT_NODE node)
if(REFRAKT_NODE)
    add_test(NAME GuiFitCheck COMMAND ${REFRAKT_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/Tests/gui_fit_check.js)
endif()
