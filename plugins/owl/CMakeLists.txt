set(OWL_SOURCES
    Source/OwlEditor.cpp
    Source/OwlProcessor.cpp
    Source/ui/OwlMotif.cpp
    Source/ui/OwlTheme.cpp
    Source/ui/ResponseView.cpp)

aa_add_plugin(Owl
    PRODUCT_NAME "Owl"
    PLUGIN_CODE Owlx
    DESCRIPTION "Simple three-band EQ (low shelf, mid bell, high shelf) with a live spectrum and draggable bands"
    SOURCES ${OWL_SOURCES})

target_include_directories(Owl PRIVATE Source)

if(AA_BUILD_TESTS)
    # Set AA_SCREENSHOT_DIR to also write the headless UI renders as PNGs.
    add_executable(OwlTests tests/OwlTests.cpp ${OWL_SOURCES})
    target_include_directories(OwlTests PRIVATE Source)
    target_link_libraries(OwlTests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(OwlTests TEST_PREFIX "owl.")
endif()
