   cmake_minimum_required(VERSION 3.22)
   project(ChordSuggester VERSION 0.0.1)

   # Where JUCE lives. Defaults to ~/Downloads/JUCE; override with -DJUCE_PATH=/your/path
   set(JUCE_PATH "$ENV{HOME}/Downloads/JUCE" CACHE PATH "Path to the JUCE folder")
   add_subdirectory(${JUCE_PATH} JUCE)

   option(CHORDSUGGESTER_COPY_PLUGIN_AFTER_BUILD "Install plugin after building" ON)

   juce_add_plugin(ChordSuggester
       COMPANY_NAME "ChordSuggester"
       IS_SYNTH FALSE
       NEEDS_MIDI_INPUT TRUE
       NEEDS_MIDI_OUTPUT TRUE
       IS_MIDI_EFFECT FALSE
       COPY_PLUGIN_AFTER_BUILD ${CHORDSUGGESTER_COPY_PLUGIN_AFTER_BUILD}
       PLUGIN_MANUFACTURER_CODE Chsg
       PLUGIN_CODE Chs1
       FORMATS VST3 Standalone
       PRODUCT_NAME "EZ Chords & Harmony")

   juce_generate_juce_header(ChordSuggester)

   target_sources(ChordSuggester PRIVATE
       Source/PluginProcessor.cpp
       Source/PluginEditor.cpp)

   target_compile_definitions(ChordSuggester PUBLIC
       JUCE_WEB_BROWSER=0
       JUCE_USE_CURL=0
       JUCE_VST3_CAN_REPLACE_VST2=0)

   target_link_libraries(ChordSuggester
       PRIVATE juce::juce_audio_utils
       PUBLIC juce::juce_recommended_config_flags)

   option(CHORDSUGGESTER_BUILD_TESTS "Build chord, melody, and plugin interaction tests" OFF)
   if(CHORDSUGGESTER_BUILD_TESTS)
       enable_testing()
       add_executable(ChordPreviewTests Tests/ChordPreviewTests.cpp)
       target_include_directories(ChordPreviewTests PRIVATE Source)
       target_compile_features(ChordPreviewTests PRIVATE cxx_std_17)
       add_test(NAME ChordPreviewTests COMMAND ChordPreviewTests)
       add_executable(MelodyTests Tests/MelodyTests.cpp)
       target_include_directories(MelodyTests PRIVATE Source)
       target_compile_features(MelodyTests PRIVATE cxx_std_17)
       add_test(NAME MelodyTests COMMAND MelodyTests)
       add_executable(PluginInteractionTests Tests/PluginInteractionTests.cpp)
       target_include_directories(PluginInteractionTests PRIVATE Source "${JUCE_PATH}/modules"
           "${CMAKE_CURRENT_BINARY_DIR}/ChordSuggester_artefacts/JuceLibraryCode")
       target_compile_features(PluginInteractionTests PRIVATE cxx_std_17)
       target_link_libraries(PluginInteractionTests PRIVATE ChordSuggester)
       add_test(NAME PluginInteractionTests COMMAND PluginInteractionTests)
   endif()
