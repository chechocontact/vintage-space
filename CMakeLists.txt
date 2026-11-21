cmake_minimum_required (VERSION 3.25)

# Single source of truth for the version: CI and the installer read it from here.
project (VoxLift VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

include (FetchContent)
FetchContent_Declare (JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
    GIT_CONFIG core.longpaths=true)
FetchContent_MakeAvailable (JUCE)

# ------------------------------------------------------------------------------
# Core: DSP and presets, no JUCE. Shared by the plugin and the tests.
add_library (VoxLiftCore STATIC
    Source/Core/Presets.cpp
    Source/Core/Reverb.cpp
    Source/Core/RiserEngine.cpp)
target_include_directories (VoxLiftCore PUBLIC Source/Core)
if (MSVC)
    target_compile_options (VoxLiftCore PRIVATE /W4)
else()
    target_compile_options (VoxLiftCore PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
set_target_properties (VoxLiftCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------------------
# Plugin
juce_add_plugin (VoxLift
    COMPANY_NAME "Enless Smile Si Si"
    PRODUCT_NAME "VoxLift"
    BUNDLE_ID com.enlesssmilesisi.voxlift
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Enls
    PLUGIN_CODE Vxl1
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Reverb Filter
    COPY_PLUGIN_AFTER_BUILD FALSE)

set (VOXLIFT_UI_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LookAndFeel.cpp)

target_sources (VoxLift PRIVATE ${VOXLIFT_UI_SOURCES})

set (VOXLIFT_JUCE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions (VoxLift PUBLIC ${VOXLIFT_JUCE_DEFINITIONS})

target_link_libraries (VoxLift
    PRIVATE
        VoxLiftCore
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Preset tuning aid: renders all presets on a synthetic voice to a WAV file.
add_executable (RenderDemo Tools/RenderDemo.cpp)
target_link_libraries (RenderDemo PRIVATE VoxLiftCore)

# ------------------------------------------------------------------------------
# Tests
enable_testing()

add_executable (CoreTests Tests/CoreTests.cpp)
target_link_libraries (CoreTests PRIVATE VoxLiftCore)
add_test (NAME CoreTests COMMAND CoreTests)

juce_add_console_app (EditorTests PRODUCT_NAME "VoxLiftEditorTests")
target_sources (EditorTests PRIVATE Tests/EditorTests.cpp ${VOXLIFT_UI_SOURCES})
target_include_directories (EditorTests PRIVATE Source)
target_compile_definitions (EditorTests PRIVATE ${VOXLIFT_JUCE_DEFINITIONS} JUCE_STANDALONE_APPLICATION=1)
target_link_libraries (EditorTests
    PRIVATE
        VoxLiftCore
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
add_test (NAME EditorTests COMMAND EditorTests)
