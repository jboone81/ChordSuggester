/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EZChordsAndHarmonyAudioProcessor::EZChordsAndHarmonyAudioProcessor()
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

EZChordsAndHarmonyAudioProcessor::~EZChordsAndHarmonyAudioProcessor()
{
}

//==============================================================================
const juce::String EZChordsAndHarmonyAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EZChordsAndHarmonyAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EZChordsAndHarmonyAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EZChordsAndHarmonyAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EZChordsAndHarmonyAudioProcessor::getTailLengthSeconds() const
{
    return 16.01;
}

int EZChordsAndHarmonyAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EZChordsAndHarmonyAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EZChordsAndHarmonyAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EZChordsAndHarmonyAudioProcessor::getProgramName (int index)
{
    return {};
}

void EZChordsAndHarmonyAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EZChordsAndHarmonyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    preview.reset (sampleRate);
}

void EZChordsAndHarmonyAudioProcessor::releaseResources()
{
    preview.reset (getSampleRate());
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EZChordsAndHarmonyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void EZChordsAndHarmonyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

void EZChordsAndHarmonyAudioProcessor::previewChord (const chords::Chord& chord)
{
    preview.chord (chord);
}

void EZChordsAndHarmonyAudioProcessor::previewTransition (const chords::Chord& current, const chords::Chord& next)
{
    preview.chord (current, &next);
}

void EZChordsAndHarmonyAudioProcessor::previewPhrase (const melody::Phrase& phrase, bool withChords, bool loop)
{
    preview.phrase (phrase, withChords, loop);
}

void EZChordsAndHarmonyAudioProcessor::previewNote (int midi) { preview.audition (midi); }
void EZChordsAndHarmonyAudioProcessor::setMelodyBackingEnabled (bool enabled) { preview.setMelodyBackingEnabled (enabled); }
double EZChordsAndHarmonyAudioProcessor::melodyPlayhead() const { return preview.playheadBeat(); }

int EZChordsAndHarmonyAudioProcessor::previewProgression (const std::vector<chords::Chord>& progression)
{
    return preview.progression (progression);
}

void EZChordsAndHarmonyAudioProcessor::stopPreview()
{
    preview.stop();
}

//==============================================================================
bool EZChordsAndHarmonyAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EZChordsAndHarmonyAudioProcessor::createEditor()
{
    return new EZChordsAndHarmonyAudioProcessorEditor (*this);
}

//==============================================================================
void EZChordsAndHarmonyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Saved with the DAW project: the chosen built-in sound.
    juce::XmlElement state ("EZChordsAndHarmony");
    state.setAttribute ("sound", int (preview.getSound()));
    copyXmlToBinary (state, destData);

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EZChordsAndHarmonyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto state = getXmlFromBinary (data, sizeInBytes); state != nullptr
        && (state->hasTagName ("EZChordsAndHarmony") || state->hasTagName ("ChordSuggester")))
        preview.setSound (PreviewPlayback::Sound (state->getIntAttribute ("sound", 0)));

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EZChordsAndHarmonyAudioProcessor();
}
