cmake_minimum_required(VERSION 3.22)

project(SimpleDaw VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE 8 lives in third_party/JUCE. The example build uses
# juce_add_gui_app() which wires up the Win32 / COM / GDI / etc. modules
# and links the appropriate system libraries on Windows.
add_subdirectory(third_party/JUCE)

juce_add_gui_app(SimpleDaw
    PRODUCT_NAME "Simple DAW"
    COMPANY_NAME "Viibe"
    VERSION "${PROJECT_VERSION}"
)

# Generates a JuceHeader.h aggregator in the build tree and adds it to the
# include path. Main.cpp uses #include <JuceHeader.h>.
juce_generate_juce_header(SimpleDaw)

# Glob all .cpp files under src/ so new modules (audio/, midi/, tracks/, plugin/,
# ui/) are picked up automatically. CONFIGURE_DEPENDS makes CMake re-glob when
# files are added or removed.
file(GLOB_RECURSE SIMPLE_DAW_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(SimpleDaw PRIVATE ${SIMPLE_DAW_SOURCES})

target_compile_definitions(SimpleDaw PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SimpleDaw,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SimpleDaw,JUCE_VERSION>"
)

# Link the audio modules explicitly so juce_generate_juce_header picks them
# up in the JuceHeader.h aggregator. juce_gui_extra transitively links the
# code, but the auto-generated header only lists modules named here.
target_link_libraries(SimpleDaw
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# juce_add_binary_data() goes here once we ship icons, fonts, or sample files.
# Example:
#   juce_add_binary_data(SimpleDaw_Assets
#       HEADER_NAME "BinaryAssets.h"
#       NAMESPACE "SimpleDawAssets"
#       SOURCE_FILES  resources/icon.png
#   )
#   target_link_libraries(SimpleDaw PRIVATE SimpleDaw_Assets)
