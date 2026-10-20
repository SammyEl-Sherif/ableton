set(ICEPENGUIN_SOURCES
    Source/IcePenguinEditor.cpp
    Source/IcePenguinProcessor.cpp
    Source/dsp/IceReverb.cpp
    Source/dsp/IcePenguinPresets.cpp
    Source/ui/IcePenguinTheme.cpp
    Source/ui/IcePenguinMotif.cpp)

aa_add_plugin(IcePenguin
    PRODUCT_NAME "IcePenguin"
    PLUGIN_CODE Icep
    DESCRIPTION "Glacial reverb with ice-themed presets"
    SOURCES ${ICEPENGUIN_SOURCES})

target_include_directories(IcePenguin PRIVATE Source)

if(AA_BUILD_TESTS)
    add_executable(IcePenguinTests tests/IcePenguinTests.cpp ${ICEPENGUIN_SOURCES})
    target_include_directories(IcePenguinTests PRIVATE Source)
    target_link_libraries(IcePenguinTests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(IcePenguinTests TEST_PREFIX "icepenguin." PROPERTIES RESOURCE_LOCK native_display)
endif()
