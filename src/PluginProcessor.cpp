#include "PluginProcessor.h"
#include "PluginEditor.h"
//#include "JK_MidiClock.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   return true;
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

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

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
        // ..do something to the data...
    }

    midiMessages.clear();

    //juce::AudioPlayHead::PositionInfo posInfo;
    //const double bpm = *posInfo.getBpm();
    double bpm = *getPlayHead()->getPosition()->getBpm();
    double sampleRate = 48000.0;
    framesPerClock =  sampleRate / bpm / 0.4;

    //const bool isPlaying = true;//playing.load();
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;
    bool isPlaying = getPlayHead()->getPosition()->getIsPlaying();
    //const bool isPlaying = positionInfo.isPlaying;
  

    // We check if something happened on the main thread that prompts the internal sequencer to start
    // playing or to stop.
    justStartedPlaying = justStartedPlaying || (!wasPlaying && isPlaying);
    justStopped = justStopped || (wasPlaying && !isPlaying);

    wasPlaying = isPlaying;

    if (justStartedPlaying)
    {
        justStartedPlaying = false;
        // We enqueue a MIDI start event to be fired 1ms (44 frames) before
        // the next MIDI clock.
        eventAfterNFrames.frames = (int) (nextClockIsInHowManyFrames) - 48;

        if (eventAfterNFrames.frames < 0)
        {
            eventAfterNFrames.frames += framesPerClock;
        }
        eventAfterNFrames.f = [&midiMessages](int bufFrameOffset) {
            const auto startMsg = juce::MidiMessage::midiStart();
            midiMessages.addEvent(startMsg, bufFrameOffset);
//            printf("sending start at bufFrameOffset %d\n", bufFrameOffset);
        };

        internalSequencerShouldStartOnNextClock = true;
    }
    else if (justStopped)
    {
        justStopped = false;
        const auto stopMsg = juce::MidiMessage::midiStop();
        midiMessages.addEvent(stopMsg, 0);

        playStartFrame = -1;
    }

    for (int i = 0; i < buffer.getNumSamples(); i++)
    {
        const auto absoluteFramePos = frameCounter + i;

        if (eventAfterNFrames.frames >= 0)
        {
            if (--eventAfterNFrames.frames == 0)
            {
                eventAfterNFrames.f(i);
            }
        }

        // Clocks are sent at a constant interval of ~919 frames. Additionally,
        // all sequencer state transitions are quantized to this interval.
        if (floor(fmod(absoluteFramePos, framesPerClock)) == 0)
        {
            const auto clockMsg = juce::MidiMessage::midiClock();

            midiMessages.addEvent(clockMsg, i);
//            printf("Sending clock at bufFrameOffset %d\n", i);

            nextClockIsInHowManyFrames = floor(framesPerClock);

            if (internalSequencerShouldStartOnNextClock)
            {
                playStartFrame = absoluteFramePos + i;
                internalSequencerShouldStartOnNextClock = false;
            }
        }

        nextClockIsInHowManyFrames--;
    }

    frameCounter += buffer.getNumSamples();
    
/*
    static int noteOn;
    static int samplesToNextMidiEvent;

    int noteNumber = 60;        //play middle c
    float velocity = 1.0;    //play max velocity
    int numSamples = buffer.getNumSamples();

    juce::AudioPlayHead* playHead = getPlayHead();
	if (playHead == nullptr) return; // Need a playhead

	juce::AudioPlayHead::CurrentPositionInfo positionInfo;
	playHead->getCurrentPosition(positionInfo);

   

    if (positionInfo.isPlaying) {
        if (!playheadRunning)
        {
            playheadRunning = true;
            midiMessages.addEvent(juce::MidiMessage::midiStart(), 0);
            
            //juce::MidiMessage msg;
            //msg = juce::MidiMessage::controllerEvent(1, 88, 127);
            //msg = juce::MidiMessage(176, 88, 127);
            //midiMessages.addEvent(msg, 0);

        }
    if (samplesToNextMidiEvent < numSamples)  //time to play the event yet?
    {
        juce::MidiMessage msg;

        if (noteOn ^= 1)
            msg = juce::MidiMessage::noteOn(1, noteNumber, velocity);
        else
            msg = juce::MidiMessage::noteOff(1, noteNumber, (float)0.0);
           

        //here samplesToNextMidiEvent is between 0 and numSamples-1 and controls when
        //the midi event will play: 0 at start of the processblock, numSamples-1 at the end of processBlock
    
        midiMessages.addEvent(msg, samplesToNextMidiEvent);
        midiMessages.addEvent(juce::MidiMessage::midiClock(), 990);
        samplesToNextMidiEvent += 48000;   //the next midi event will play in exactly 1 second (assuming a samplerate of 44100)
    }

    samplesToNextMidiEvent -= numSamples; //count down with the number of samples in every processblock
    } else {
        if (playheadRunning)
        {
            playheadRunning = false;
            midiMessages.addEvent(juce::MidiMessage::midiStop(), 0);
        }
    }

int sampleRate = 48000;
int bufferSize = buffer.getNumSamples();
juce::AudioPlayHead::PositionInfo posInfo;
 //const double bpm = *posInfo.getBpm();
 //double newbpm = *getPlayHead()->getPosition()->getBpm();
 auto bpm = *getPlayHead()->getPosition()->getBpm();

        // PPQ value of one sample
        const double ppqPerSample = (bpm / 60.0) / sampleRate;

        const auto ppqPosition = *posInfo.getPpqPosition();

        // PPQ offset to compensate Midi interface latency
        double hostPpqPosition = ppqPosition + ppqOffset * ppqPerSample;

        if (posInfo.getIsPlaying() || posInfo.getIsRecording())
        {
            if (!wasPlaying)
            {
                // set the point where to start the slave
                ppqToStartSyncAt = getNearestSixteenthInPPQ(hostPpqPosition);

            
            }
            else
            {
                
            }

            for (int posInBuffer = 0; posInBuffer < bufferSize; ++posInBuffer)
            {
                syncPpqPosition = hostPpqPosition + (posInBuffer * ppqPerSample);

                const int   clockDistanceInSamples = (int)((60.0 * sampleRate) / (bpm * 24.0));
                const juce::int64 hostSamplePos          = roundToInt64((hostPpqPosition * (60.0 / bpm)) * sampleRate);
                const juce::int64 syncSamplePos          = hostSamplePos + posInBuffer;

                // Some hosts like Cubase come up with a wacky ppqPosition
                // that could break the timing! Best is to "wait"
                // here for the right ppqPosition to jump on.
                if (syncPpqPosition >= ppqToStartSyncAt)
                {
                    if ((syncFlag & gStartSlave) == gStartSlave)
                    {
                        continueMessage.setTimeStamp(static_cast<double>(posInBuffer));

                        midiMessages.addEvent(continueMessage, posInBuffer);

                        syncFlag &= gCycleEnd;
                    }

                    
                }

                // For best timing we should never interupt Midiclock messages!
                // Seems that some slaves constantly adjusting their internal clock
                // to Midiclock even if they are in stop mode.
                if (syncSamplePos % clockDistanceInSamples == 0)
                {
                    clockMessage.setTimeStamp(static_cast<double>(posInBuffer));

                    midiMessages.addEvent(clockMessage, posInBuffer);
                }
            }

            wasPlaying = true;
        }
        else
        {
          

            syncPpqPosition = hostPpqPosition;

            syncFlag = gStartSlave;

            wasPlaying = false;
        }
    



*/

}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

void AudioPluginAudioProcessor::playStop()
{
    playing.store(!playing.load());
}