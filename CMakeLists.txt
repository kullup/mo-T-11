cmake_minimum_required(VERSION 3.22)
project(MOT11 VERSION 0.1.1 LANGUAGES C CXX)

option(MOT_BUILD_APP "Build the JUCE desktop instrument" ON)
set(MOT_JUCE_DIR "" CACHE PATH "Path to an optional local JUCE 8.0.12 checkout")

add_library(mot_core STATIC Source/Effects.cpp Source/Patch.cpp Source/Synth.cpp)
target_include_directories(mot_core PUBLIC Source)
target_compile_features(mot_core PUBLIC cxx_std_17)
set_target_properties(mot_core PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
    add_executable(mot_tests Tests/SynthTests.cpp)
    target_link_libraries(mot_tests PRIVATE mot_core)
    add_test(NAME mot_headless COMMAND mot_tests)
endif()

if(MOT_BUILD_APP)
    if(MOT_JUCE_DIR)
        add_subdirectory("${MOT_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850)
        FetchContent_MakeAvailable(juce)
    endif()

    juce_add_binary_data(mot_assets SOURCES
        Assets/Fonts/BarlowSemiCondensed-Medium.ttf
        Assets/Fonts/BarlowSemiCondensed-Bold.ttf
        Assets/Fonts/OFL.txt
        Assets/Walnut.jpg)

    juce_add_gui_app(MOT11 PRODUCT_NAME "MO-T 11" BUNDLE_ID "org.mot11.synth")
    target_sources(MOT11 PRIVATE Source/Main.cpp Source/Standalone.cpp Source/Panel.cpp)
    target_compile_definitions(MOT11 PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(WIN32)
        target_compile_definitions(MOT11 PRIVATE JUCE_USE_WINRT_MIDI=1)
    endif()
    target_link_libraries(MOT11 PRIVATE mot_core mot_assets juce::juce_audio_devices
        juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
