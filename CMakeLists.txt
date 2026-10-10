   cmake_minimum_required(VERSION 3.22)
   project(ChordSuggester VERSION 0.0.1)

   # Where JUCE lives. Defaults to ~/Downloads/JUCE; override with -DJUCE_PATH=/your/path
   set(JUCE_PATH "$ENV{HOME}/Downloads/JUCE" CACHE PATH "Path to the JUCE folder")
   add_subdirectory(${JUCE_PATH} JUCE)

   juce_add_plugin(ChordSuggester
       COMPANY_NAME "ChordSuggester"
       IS_SYNTH FALSE
       NEEDS_MIDI_INPUT TRUE
       NEEDS_MIDI_OUTPUT TRUE
       IS_MIDI_EFFECT FALSE
       COPY_PLUGIN_AFTER_BUILD TRUE
       PLUGIN_MANUFACTURER_CODE Chsg
       PLUGIN_CODE Chs1
       FORMATS VST3 Standalone
       PRODUCT_NAME "ChordSuggester")

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