/*

 //#######################################################################################
 //Class to insert Midiclock messages into a given MidiBuffer, suitable for block
 //based audio callbacks. The class can act on position jumps e.g. "Loops" by sending a
 //positioning message.
 //NOTE: No ballistik came in to play so I wouldn't recommend using it to drive a mechanical
 //tape deck!!!
 //
 //solar3d-software, April- 10- 2012
 //#######################################################################################

 */
#pragma once

//#include "Includes.h"
#include <cstdint>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

using namespace juce;

class JK_MidiClock
{
   public:
    void
    setPositionJumpThreshold(double ms)
    {
        posChangeThreshold = ms / 1000.0;
    }
    double
    getPositionJumpThreshold()
    {
        return posChangeThreshold;
    }
    void
    setFollowSongPosition(bool shouldFollow)
    {
        followSongPosition = shouldFollow;
    }
    bool
    getFollowSongPosition()
    {
        return followSongPosition;
    }
    void
    setOffset(int offset)
    {
        ppqOffset = offset;
    }
    int
    getOffset()
    {
        return ppqOffset;
    }

     void generateMidiclock(const AudioPlayHead::PositionInfo& positionInfo, MidiBuffer* midiBuffer, int bufferSize, double sampleRate);


 
    bool   wasPlaying         = false;
    double syncPpqPosition    = -999.0;
    double posChangeThreshold = 0.001;
    double ppqToStartSyncAt   = 0.0;
    bool   followSongPosition = true;
    uint8_t  syncFlag           = 0;
    int    ppqOffset          = 0;

    static const int gCycleEnd   = 1;
    static const int gStartSlave = 2;

    MidiMessage clockMessage{MidiMessage::midiClock()};
    MidiMessage continueMessage{MidiMessage::midiContinue()};
    MidiMessage stopMessage{MidiMessage::midiStop()};

    void sendSongPositionPointerMessage(double ppqPosition, int posInBuffer, MidiBuffer* buffer);

    bool positionJumped(double lastPosInPPQ, double currentPosInPPQ, double sampleRate, double ppqPerSample);

    static int64
    roundToInt64(double val) noexcept
    {
        return (val - floor(val) >= 0.5) ? (int64)(ceil(val)) : (int64)(floor(val));
    }

    static double
    getNearestSixteenthInPPQ(double ppqPosition)
    {
        return ceil(ppqPosition * 4.0) / 4.0;
    }

    JUCE_LEAK_DETECTOR(JK_MidiClock);
};
