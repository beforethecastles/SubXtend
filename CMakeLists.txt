cmake_minimum_required(VERSION 3.22)
project(SubXtend VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# Use a local JUCE checkout if present, otherwise download it.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SUBX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SUBX_FORMATS AU)
endif()

juce_add_plugin(SubXtend
    COMPANY_NAME "SubXtend"
    PLUGIN_MANUFACTURER_CODE Sbxt
    PLUGIN_CODE Sbx1
    FORMATS ${SUBX_FORMATS}
    PRODUCT_NAME "SubXtend"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(SubXtend PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SubXtend PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SubXtend
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# DSP unit test (no JUCE)
add_executable(engine_test tests/engine_test.cpp)
target_include_directories(engine_test PRIVATE Source)
enable_testing()
add_test(NAME engine_test COMMAND engine_test)
