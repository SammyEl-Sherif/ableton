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
    # IceReverb embeds multi-MiB delay buffers; default MSVC 1 MiB stack overflows in Catch2 tests.
    if(MSVC)
        target_link_options(IcePenguinTests PRIVATE "LINKER:/STACK:8388608")
    endif()
    catch_discover_tests(IcePenguinTests TEST_PREFIX "icepenguin." PROPERTIES RESOURCE_LOCK native_display)
endif()
