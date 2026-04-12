/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** NullAudioManager.h
**
** No-op AudioManager implementation for Emscripten/web builds.
** Miles Sound System is a Win32 DLL and cannot run in the browser.
** All methods are silent stubs so the rest of the engine compiles
** and runs without audio.
*/

#pragma once

#include "Common/GameAudio.h"

class NullAudioManager : public AudioManager
{
public:
    NullAudioManager() {}
    virtual ~NullAudioManager() override {}

    // SubsystemInterface
    virtual void init()   override {}
    virtual void reset()  override {}
    virtual void update() override {}

    // Pure virtuals from AudioManager
    virtual void audioDebugDisplay( DebugDisplayInterface*, void*, FILE* ) override {}

    virtual void stopAudio( AudioAffect )  override {}
    virtual void pauseAudio( AudioAffect ) override {}
    virtual void resumeAudio( AudioAffect ) override {}
    virtual void pauseAmbient( Bool ) override {}
    virtual void killAudioEventImmediately( AudioHandle ) override {}

    virtual void nextMusicTrack() override {}
    virtual void prevMusicTrack() override {}
    virtual Bool isMusicPlaying() const override { return FALSE; }
    virtual Bool hasMusicTrackCompleted( const AsciiString&, Int ) const override { return TRUE; }
    virtual AsciiString getMusicTrackName() const override { return AsciiString::TheEmptyString; }

    virtual void openDevice()  override {}
    virtual void closeDevice() override {}
    virtual void *getDevice()  override { return nullptr; }

    virtual void notifyOfAudioCompletion( UnsignedInt, UnsignedInt ) override {}

    virtual UnsignedInt getProviderCount() const override { return 0; }
    virtual AsciiString getProviderName( UnsignedInt ) const override { return AsciiString::TheEmptyString; }
    virtual UnsignedInt getProviderIndex( AsciiString ) const override { return 0; }
    virtual void selectProvider( UnsignedInt ) override {}
    virtual void unselectProvider() override {}
    virtual UnsignedInt getSelectedProvider() const override { return 0; }

    virtual void setSpeakerType( UnsignedInt ) override {}
    virtual UnsignedInt getSpeakerType() override { return 0; }

    virtual UnsignedInt getNum2DSamples() const override { return 0; }
    virtual UnsignedInt getNum3DSamples() const override { return 0; }
    virtual UnsignedInt getNumStreams()   const override { return 0; }

    virtual Bool doesViolateLimit( AudioEventRTS* )     const override { return FALSE; }
    virtual Bool isPlayingLowerPriority( AudioEventRTS* ) const override { return FALSE; }
    virtual Bool isPlayingAlready( AudioEventRTS* )     const override { return FALSE; }
    virtual Bool isObjectPlayingVoice( UnsignedInt )    const override { return FALSE; }

    virtual void adjustVolumeOfPlayingAudio( AsciiString, Real ) override {}
    virtual void removePlayingAudio( AsciiString ) override {}
    virtual void removeAllDisabledAudio() override {}

    virtual Bool has3DSensitiveStreamsPlaying() const override { return FALSE; }

    virtual void *getHandleForBink()  override { return nullptr; }
    virtual void releaseHandleForBink() override {}

    virtual void friend_forcePlayAudioEventRTS( const AudioEventRTS* ) override {}
    virtual void setPreferredProvider( AsciiString ) override {}
    virtual void setPreferredSpeaker( AsciiString )  override {}

    virtual Real getFileLengthMS( AsciiString ) const override { return 0.0f; }
    virtual void closeAnySamplesUsingFile( const void* ) override {}
    virtual void setDeviceListenerPosition() override {}
};
