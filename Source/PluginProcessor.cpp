/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChordSuggesterAudioProcessor::ChordSuggesterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ChordSuggesterAudioProcessor::~ChordSuggesterAudioProcessor()
{
}

//==============================================================================
const juce::String ChordSuggesterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChordSuggesterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChordSuggesterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChordSuggesterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChordSuggesterAudioProcessor::getTailLengthSeconds() const
{
    return 16.01;
}

int ChordSuggesterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ChordSuggesterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChordSuggesterAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ChordSuggesterAudioProcessor::getProgramName (int index)
{
    return {};
}

void ChordSuggesterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ChordSuggesterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    preview.reset (sampleRate);
}

void ChordSuggesterAudioProcessor::releaseResources()
{
    preview.reset (getSampleRate());
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChordSuggesterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ChordSuggesterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Generate each sample once, then mix the same preview into every output.
    // Keyboard clicks are queued by MidiKeyboardState and merged with incoming MIDI.
    midiKeyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);
    preview.beginBlock();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float tone = preview.nextSample();
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            buffer.addSample (channel, sample, tone);
    }
}

void ChordSuggesterAudioProcessor::previewChord (const chords::Chord& chord)
{
    preview.chord (chord);
}

void ChordSuggesterAudioProcessor::previewTransition (const chords::Chord& current, const chords::Chord& next)
{
    preview.chord (current, &next);
}

void ChordSuggesterAudioProcessor::previewPhrase (const melody::Phrase& phrase, bool withChords, bool loop)
{
    preview.phrase (phrase, withChords, loop);
}

void ChordSuggesterAudioProcessor::previewNote (int midi) { preview.audition (midi); }
void ChordSuggesterAudioProcessor::setMelodyBackingEnabled (bool enabled) { preview.setMelodyBackingEnabled (enabled); }
double ChordSuggesterAudioProcessor::melodyPlayhead() const { return preview.playheadBeat(); }

int ChordSuggesterAudioProcessor::previewProgression (const std::vector<chords::Chord>& progression)
{
    return preview.progression (progression);
}

void ChordSuggesterAudioProcessor::stopPreview()
{
    preview.stop();
}

//==============================================================================
bool ChordSuggesterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ChordSuggesterAudioProcessor::createEditor()
{
    return new ChordSuggesterAudioProcessorEditor (*this);
}

//==============================================================================
void ChordSuggesterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Saved with the DAW project: the chosen built-in sound.
    juce::XmlElement state ("ChordSuggester");
    state.setAttribute ("sound", int (preview.getSound()));
    copyXmlToBinary (state, destData);

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ChordSuggesterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto state = getXmlFromBinary (data, sizeInBytes); state != nullptr && state->hasTagName ("ChordSuggester"))
        preview.setSound (PreviewPlayback::Sound (state->getIntAttribute ("sound", 0)));

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordSuggesterAudioProcessor();
}
