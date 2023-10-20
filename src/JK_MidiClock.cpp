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
#include "JK_MidiClock.h"

void
JK_MidiClock::generateMidiclock(const AudioPlayHead::PositionInfo& positionInfo, MidiBuffer* midiBuffer, int bufferSize, double sampleRate)
{
    //###################################Some explanation about musical tempo #################
    // A Time Signature, is two numbers, one on top of the other. The numerator describes the  #
    // number of Beats in a Bar, while the denominator describes of what note value a Beat is. #
    // So 4/4 would be four quarter-notes per Bar, while 4/2 would be four half-notes per Bar, #
    // 4/8 would be four eighth-notes per Bar, and 2/4 would be two quarter-notes per Bar.     #
    //#########################################################################################


    if (midiBuffer != nullptr)
    {
        const double bpm = *positionInfo.getBpm();

        // PPQ value of one sample
        const double ppqPerSample = (bpm / 60.0) / sampleRate;

        const auto ppqPosition = *positionInfo.getPpqPosition();

        // PPQ offset to compensate Midi interface latency
        double hostPpqPosition = ppqPosition + ppqOffset * ppqPerSample;

        if (positionInfo.getIsPlaying() || positionInfo.getIsRecording())
        {
            if (!wasPlaying)
            {
                // set the point where to start the slave
                ppqToStartSyncAt = getNearestSixteenthInPPQ(hostPpqPosition);

                // Special case: Master is set to always start playback from the previous start position...
                if (positionJumped(syncPpqPosition, hostPpqPosition, sampleRate, ppqPerSample))
                {
                    // Cue Midiclock slave to the nearest sixteenth note to new start position
                    // because the one calculated in stop mode isn't valid anymore.
                    sendSongPositionPointerMessage(ppqToStartSyncAt, 0, midiBuffer);
                }
            }
            else
            {
                // Position jump (loop or manually position change while playing)
                if (positionJumped(syncPpqPosition, hostPpqPosition, sampleRate, ppqPerSample))
                {
                    // set the point where to start the slave
                    ppqToStartSyncAt = getNearestSixteenthInPPQ(hostPpqPosition);

                    // User has changed position manually while playing
                    if (syncFlag == 0)
                    {
                        stopMessage.setTimeStamp(0);

                        midiBuffer->addEvent(stopMessage, 0);

                        sendSongPositionPointerMessage(hostPpqPosition, 0, midiBuffer);

                        syncFlag = gStartSlave;
                    }
                    else
                    {
                        if (followSongPosition)
                        {
                            sendSongPositionPointerMessage(hostPpqPosition, 0, midiBuffer);

                            syncFlag = gStartSlave;
                        }
                        else
                            syncFlag = 0;
                    }
                }
            }

            for (int posInBuffer = 0; posInBuffer < bufferSize; ++posInBuffer)
            {
                syncPpqPosition = hostPpqPosition + (posInBuffer * ppqPerSample);

                const int   clockDistanceInSamples = roundToInt((60.0 * sampleRate) / (bpm * 24.0));
                const int64 hostSamplePos          = roundToInt64((hostPpqPosition * (60.0 / bpm)) * sampleRate);
                const int64 syncSamplePos          = hostSamplePos + posInBuffer;

                // Some hosts like Cubase come up with a wacky ppqPosition
                // that could break the timing! Best is to "wait"
                // here for the right ppqPosition to jump on.
                if (syncPpqPosition >= ppqToStartSyncAt)
                {
                    if ((syncFlag & gStartSlave) == gStartSlave)
                    {
                        continueMessage.setTimeStamp(static_cast<double>(posInBuffer));

                        midiBuffer->addEvent(continueMessage, posInBuffer);

                        syncFlag &= gCycleEnd;
                    }

                    // Loop mode on
                    auto loopPoints = *positionInfo.getLoopPoints();
                    if (positionInfo.getIsLooping() && loopPoints.ppqStart != loopPoints.ppqEnd)
                    {
                        const double ppqToCycleEnd     = fabs(loopPoints.ppqEnd - syncPpqPosition);
                        const int64  samplesToCycleEnd = roundToInt64(ppqToCycleEnd * (60.0 / bpm) * sampleRate);

                        if ((syncFlag & gCycleEnd) == 0)
                        {
                            if (samplesToCycleEnd <= clockDistanceInSamples)  // For fine tuning tweak here
                            {
                                // We have reached the loop- end position
                                // and must stop the Midiclock slave here
                                if (followSongPosition)
                                {
                                    stopMessage.setTimeStamp(static_cast<double>(posInBuffer));

                                    midiBuffer->addEvent(stopMessage, posInBuffer);
                                }

                                syncFlag |= gCycleEnd;
                            }
                        }
                    }
                }

                // For best timing we should never interupt Midiclock messages!
                // Seems that some slaves constantly adjusting their internal clock
                // to Midiclock even if they are in stop mode.
                if (syncSamplePos % clockDistanceInSamples == 0)
                {
                    clockMessage.setTimeStamp(static_cast<double>(posInBuffer));

                    midiBuffer->addEvent(clockMessage, posInBuffer);
                }
            }

            wasPlaying = true;
        }
        else
        {
            // Send positioning message if the user has stopped or if he changed the playhead position
            // manually in stop mode! This will also initially cue slave after loading plugin instance.
            if (wasPlaying || positionJumped(syncPpqPosition, hostPpqPosition, sampleRate, ppqPerSample))
            {
                stopMessage.setTimeStamp(0);

                midiBuffer->addEvent(stopMessage, 0);

                sendSongPositionPointerMessage(hostPpqPosition, 0, midiBuffer);
            }

            syncPpqPosition = hostPpqPosition;

            syncFlag = gStartSlave;

            wasPlaying = false;
        }
    }
}

bool
JK_MidiClock::positionJumped(double lastPosInPPQ, double currentPosInPPQ, double sampleRate, double ppqPerSample)
{
    // This returns true if the user has changed the playhead position manually or if
    // a jump has occured! The comperator's default threshold is lastPosInPPQ +- 10ms.
    if (currentPosInPPQ < lastPosInPPQ - ((posChangeThreshold * sampleRate) * ppqPerSample) ||
        currentPosInPPQ > lastPosInPPQ + ((posChangeThreshold * sampleRate) * ppqPerSample))
        return true;

    return false;
}

void
JK_MidiClock::sendSongPositionPointerMessage(double ppqPosition, int posInBuffer, MidiBuffer* buffer)
{
    // This will cue the slave to the NEAREST
    // 16th note to the given ppqPosition.
    const int intBeat = static_cast<int>(ceil(ppqPosition * 4));

    MidiMessage songPositionMessage(MidiMessage::songPositionPointer(intBeat));
    songPositionMessage.setTimeStamp(static_cast<double>(posInBuffer));

    buffer->addEvent(songPositionMessage, posInBuffer);
}
