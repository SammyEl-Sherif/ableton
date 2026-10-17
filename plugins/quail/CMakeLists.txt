set(QUAIL_SOURCES
    Source/QuailEditor.cpp
    Source/QuailParameters.cpp
    Source/QuailProcessor.cpp
    Source/dsp/QuailEngine.cpp
    Source/dsp/QuailSpectral.cpp
    Source/ui/BandActions.cpp
    Source/ui/BandPanel.cpp
    Source/ui/EqDisplay.cpp
    Source/ui/QuailMotif.cpp
    Source/ui/QuailTheme.cpp
    Source/ui/Widgets.cpp)

aa_add_plugin(Quail
    PRODUCT_NAME "Quail"
    PLUGIN_CODE Quai
    DESCRIPTION "24-band dynamic EQ with stereo placement, side-chain and spectral dynamics"
    VST3_CATEGORIES Fx EQ
    SOURCES ${QUAIL_SOURCES})

target_include_directories(Quail PRIVATE Source)

if(AA_BUILD_TESTS)
    # Set AA_SCREENSHOT_DIR to also write the headless UI renders as PNGs.
    add_executable(QuailTests
        tests/QuailDspTests.cpp
        tests/QuailProcessorTests.cpp
        tests/QuailEditorTests.cpp
        ${QUAIL_SOURCES})
    target_include_directories(QuailTests PRIVATE Source)
    target_link_libraries(QuailTests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(QuailTests TEST_PREFIX "quail." PROPERTIES RESOURCE_LOCK native_display)
endif()
