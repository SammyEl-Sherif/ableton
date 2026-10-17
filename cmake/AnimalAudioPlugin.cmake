# aa_add_plugin(<Target>
#     PRODUCT_NAME  "Duck"
#     PLUGIN_CODE   Duck          # unique 4 chars, at least one uppercase
#     [DESCRIPTION  "..."]
#     [IS_SYNTH FALSE]
#     [BUNDLE_ID    com.animalaudio.duck]
#     [VST3_CATEGORIES Fx EQ]     # default: Fx
#     SOURCES       a.cpp b.cpp ...)
#
# Wraps juce_add_plugin with the Animal Audio defaults (manufacturer, VST3 only,
# Ableton-friendly category, shared libs linked, warnings on).
function(aa_add_plugin target)
    cmake_parse_arguments(AA "" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION;IS_SYNTH;BUNDLE_ID" "SOURCES;VST3_CATEGORIES"
                          ${ARGN})

    if(NOT AA_PRODUCT_NAME OR NOT AA_PLUGIN_CODE)
        message(FATAL_ERROR "aa_add_plugin(${target}): PRODUCT_NAME and PLUGIN_CODE are required")
    endif()
    if(NOT AA_DESCRIPTION)
        set(AA_DESCRIPTION "${AA_PRODUCT_NAME} by Animal Audio")
    endif()
    if(NOT AA_IS_SYNTH)
        set(AA_IS_SYNTH FALSE)
    endif()
    if(NOT AA_VST3_CATEGORIES)
        set(AA_VST3_CATEGORIES Fx)
    endif()
    if(NOT AA_BUNDLE_ID)
        string(TOLOWER "${AA_PRODUCT_NAME}" _lower)
        string(REGEX REPLACE "[^a-z0-9]" "" _lower "${_lower}")
        set(AA_BUNDLE_ID "com.animalaudio.${_lower}")
    endif()

    set(_formats VST3)
    if(AA_BUILD_STANDALONE)
        list(APPEND _formats Standalone)
    endif()

    juce_add_plugin(${target}
        PRODUCT_NAME "${AA_PRODUCT_NAME}"
        COMPANY_NAME "Animal Audio"
        COMPANY_WEBSITE "https://animal.audio"
        BUNDLE_ID "${AA_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE AnAu
        PLUGIN_CODE ${AA_PLUGIN_CODE}
        FORMATS ${_formats}
        VST3_CATEGORIES ${AA_VST3_CATEGORIES}
        IS_SYNTH ${AA_IS_SYNTH}
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        DESCRIPTION "${AA_DESCRIPTION}"
        COPY_PLUGIN_AFTER_BUILD ${AA_COPY_PLUGINS_AFTER_BUILD})

    target_sources(${target} PRIVATE ${AA_SOURCES})

    # Refresh the build stamp before this plugin compiles (also implied by linking animalaudio::core, but explicit
    # is what every generator, including Visual Studio, reliably honours).
    add_dependencies(${target} aa_build_info)

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0)

    target_link_libraries(${target}
        PRIVATE
            animalaudio::ui
            animalaudio::core
            animalaudio::dsp
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
