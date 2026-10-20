set(DUCK_SOURCES
    Source/DuckEditor.cpp
    Source/DuckProcessor.cpp
    Source/dsp/DuckPresets.cpp
    Source/ui/DuckMotif.cpp
    Source/ui/DuckTheme.cpp)

aa_add_plugin(Duck
    PRODUCT_NAME "Duck"
    PLUGIN_CODE Duck
    DESCRIPTION "Tempo-synced volume ducking with an editable curve"
    SOURCES ${DUCK_SOURCES})

target_include_directories(Duck PRIVATE Source)

if(AA_BUILD_TESTS)
    # The standard shaper-plugin suites (shared/testing) run against Duck's own processor and editor.
    # Set AA_SCREENSHOT_DIR to also write the headless UI renders as PNGs.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _duck_version)
    string(STRIP "${_duck_version}" _duck_version)
    add_executable(DuckTests tests/DuckTests.cpp ${DUCK_SOURCES})
    target_include_directories(DuckTests PRIVATE Source)
    aa_attach_plugin_build_info(DuckTests "${_duck_version}")
    target_link_libraries(DuckTests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(DuckTests TEST_PREFIX "duck." PROPERTIES RESOURCE_LOCK native_display)
endif()
