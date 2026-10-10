/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ChordEngine.h"
#include "PreviewPlayback.h"

//==============================================================================
/**
*/
class ChordSuggesterAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ChordSuggesterAudioProcessor();
    ~ChordSuggesterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void previewChord (const chords::Chord& chord);
    void previewTransition (const chords::Chord& current, const chords::Chord& next);
    void previewPhrase (const melody::Phrase& phrase, bool withChords = false, bool loop = false);
    void setMelodyBackingEnabled (bool enabled);
    void previewNote (int midi);
    double melodyPlayhead() const;
    void stopPreview();
    juce::MidiKeyboardState& getMidiKeyboardState() noexcept { return midiKeyboardState; }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    PreviewPlayback preview;
    juce::MidiKeyboardState midiKeyboardState;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSuggesterAudioProcessor)
};
