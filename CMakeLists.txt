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

target_sources(SimpleDaw PRIVATE
    src/Main.cpp
)

target_compile_definitions(SimpleDaw PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SimpleDaw,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SimpleDaw,JUCE_VERSION>"
)

# juce_gui_extra transitively pulls in juce_audio_basics, juce_audio_devices,
# juce_audio_formats, juce_audio_processors, juce_audio_utils, etc.
target_link_libraries(SimpleDaw
    PRIVATE
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
