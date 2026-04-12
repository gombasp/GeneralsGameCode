/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** NullWebBrowser.h
**
** No-op WebBrowser implementation for Emscripten builds.
** The in-game web browser (W3DWebBrowser) depends on COM/ATL and
** cannot run in the browser environment.
*/

#pragma once

#include "GameNetwork/WOLBrowser/WebBrowser.h"

class NullWebBrowser : public WebBrowser
{
public:
    NullWebBrowser() {}
    virtual ~NullWebBrowser() override {}

    virtual void init()   override {}
    virtual void reset()  override {}
    virtual void update() override {}

    virtual Bool createBrowserWindow( const char*, GameWindow* ) override { return FALSE; }
    virtual void closeBrowserWindow( GameWindow* ) override {}
};
