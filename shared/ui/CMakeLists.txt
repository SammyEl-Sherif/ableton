# Shared Animal Audio UI kit (theme, brand mark, knob, pill buttons).
# INTERFACE library: sources are compiled into each plugin that links it, so JUCE modules are never
# compiled twice into one binary.
add_library(animalaudio_ui INTERFACE)
add_library(animalaudio::ui ALIAS animalaudio_ui)
target_include_directories(animalaudio_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(animalaudio_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BrandBadge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PillButton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RotaryKnob.cpp)
target_link_libraries(animalaudio_ui INTERFACE
    juce::juce_gui_basics)
target_compile_features(animalaudio_ui INTERFACE cxx_std_17)
