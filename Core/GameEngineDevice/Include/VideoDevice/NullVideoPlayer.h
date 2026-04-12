/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** NullVideoPlayer.h
**
** A no-op VideoPlayerInterface implementation used when video playback
** is compiled out (e.g. Emscripten/web builds).
**
** All open/load calls return nullptr immediately. update() and all other
** lifecycle methods are empty. The rest of the engine calls stopMovie()
** and checks whether m_videoStream is null before rendering, so returning
** nullptr from open() is sufficient to disable cutscenes gracefully.
*/

#pragma once

#include "GameClient/VideoPlayer.h"

class NullVideoPlayer : public VideoPlayerInterface
{
public:
    NullVideoPlayer() {}
    virtual ~NullVideoPlayer() override {}

    virtual void init()   override {}
    virtual void reset()  override {}
    virtual void update() override {}
    virtual void deinit() override {}

    virtual void loseFocus()   override {}
    virtual void regainFocus() override {}

    virtual VideoStreamInterface* open( AsciiString ) override { return nullptr; }
    virtual VideoStreamInterface* load( AsciiString ) override { return nullptr; }
    virtual VideoStreamInterface* firstStream()       override { return nullptr; }

    virtual void closeAllStreams()                      override {}
    virtual void addVideo( Video* )                    override {}
    virtual void removeVideo( Video* )                 override {}
    virtual Int  getNumVideos()                        override { return 0; }
    virtual const Video* getVideo( AsciiString )       override { return nullptr; }
    virtual const Video* getVideo( Int )               override { return nullptr; }

    virtual const FieldParse* getFieldParse() const    override { return nullptr; }
    virtual void notifyVideoPlayerOfNewProvider( Bool ) override {}
};
