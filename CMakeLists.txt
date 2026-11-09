cmake_minimum_required(VERSION 3.25)
project(SkellCast VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
option(SKELLCAST_CORE_ONLY "Build protocol tests without downloading JUCE" OFF)
if(SKELLCAST_CORE_ONLY)
    add_executable(SkellCastTests Tests/core_tests.cpp)
    target_include_directories(SkellCastTests PRIVATE Source)
    add_test(NAME SkellCastCoreTests COMMAND SkellCastTests)
    return()
endif()

option(SKELLCAST_FETCH_JUCE "Fetch JUCE at configure time if JUCE_DIR isn't specified" ON)
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
elseif(SKELLCAST_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.16-security
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
else()
    message(FATAL_ERROR "Pass -DJUCE_DIR=path/to/JUCE or enable SKELLCAST_FETCH_JUCE")
endif()

juce_add_plugin(SkellCast
    COMPANY_NAME "Skelly"
    BUNDLE_ID "uno.skellyr.skellcast"
    PLUGIN_MANUFACTURER_CODE Skly
    PLUGIN_CODE SkCa
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "SkellCast")
juce_generate_juce_header(SkellCast)
target_sources(SkellCast PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h Source/BridgeProtocol.h)
target_compile_definitions(SkellCast PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SkellCast PRIVATE
    juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)

juce_add_gui_app(SkellCastReceiver
    COMPANY_NAME "Skelly"
    PRODUCT_NAME "SkellCastReceiver")
juce_generate_juce_header(SkellCastReceiver)
target_sources(SkellCastReceiver PRIVATE Source/ReceiverMain.cpp Source/BridgeProtocol.h)
target_compile_definitions(SkellCastReceiver PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SkellCastReceiver PRIVATE
    juce::juce_audio_utils juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)

if(BUILD_TESTING)
    add_executable(SkellCastTests Tests/core_tests.cpp)
    target_include_directories(SkellCastTests PRIVATE Source)
    add_test(NAME SkellCastCoreTests COMMAND SkellCastTests)
endif()
