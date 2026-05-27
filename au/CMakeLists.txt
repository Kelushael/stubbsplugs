cmake_minimum_required(VERSION 3.22)
project(NeonForge VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

# Use bundled JUCE on the USB; fall back to FetchContent if not found
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(NeonForge
    PRODUCT_NAME "NeonForge"
    COMPANY_NAME "NeonForge"
    BUNDLE_ID com.neonforge.neonforge
    PLUGIN_MANUFACTURER_CODE Nfrg
    PLUGIN_CODE Nf01
    FORMATS AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(NeonForge PRIVATE
    Source/Profile.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(NeonForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NeonForge PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
