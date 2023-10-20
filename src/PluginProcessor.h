#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct EventAfterNFrames {
    int frames = -1;
    std::function<void(int)> f = [](int bufFrameOffset){};
};

//==============================================================================
class AudioPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

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
    
    void playStop();

private:
    //==============================================================================
//CurrentPositionInfo positionInfo;

    std::vector<short> metronomeSampleData{0, 2047, 4092, 6145, 8186, 10239, 12284, 14329, 14333, 14330, 14330, 14332,
                                           14330, 14331, 14330, 14330, 14333, 14329, 14333, 14328, 14335, 14329, 14331,
                                           14331, 14331, 14333, 14329, 14333, 14329, 14331, 14330, 14331, 14331, 14331,
                                           14330, 14330, 14333, 14331, 14330, 14331, 14331, 14329, 14331, 14331, 14331,
                                           14331, 14330, 14330, 14334, 14328, 1};

    // 120BPM
    double framesPerClock = 918.75;
    const short framesPerQuarterNote = 22050;

    int frameCounter = 0;
    char metronomeFrameIndex = 127;
    float nextClockIsInHowManyFrames = 0.f;

    // Counts to 4 to provide accents
    char metronomeCounter = 3;

    int playStartFrame = -1;
    std::atomic<bool> playing{false };
   // bool wasPlaying{false};

    bool internalSequencerShouldStartOnNextClock = false;
    bool justStartedPlaying = false;
    bool justStopped = false;

    EventAfterNFrames eventAfterNFrames;

    bool playheadRunning = false;

    bool   wasPlaying         = false;
    double syncPpqPosition    = -999.0;
    double posChangeThreshold = 0.001;
    double ppqToStartSyncAt   = 0.0;
    bool   followSongPosition = true;
    uint8_t  syncFlag           = 0;
    int    ppqOffset          = 0;

    static const int gCycleEnd   = 1;
    static const int gStartSlave = 2;

    juce::MidiMessage clockMessage{juce::MidiMessage::midiClock()};
    juce::MidiMessage continueMessage{juce::MidiMessage::midiContinue()};
    juce::MidiMessage stopMessage{juce::MidiMessage::midiStop()};

    static juce::int64
    roundToInt64(double val) noexcept
    {
        return (val - floor(val) >= 0.5) ? (juce::int64)(ceil(val)) : (juce::int64)(floor(val));
    }

    static double
    getNearestSixteenthInPPQ(double ppqPosition)
    {
        return ceil(ppqPosition * 4.0) / 4.0;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
