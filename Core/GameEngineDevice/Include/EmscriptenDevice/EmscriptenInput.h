/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** EmscriptenInput.h
**
** Keyboard and Mouse implementations for Emscripten/WebAssembly builds.
** These replace Win32Mouse/DirectInputKeyboard which depend on Win32 APIs.
**
** EmscriptenKeyboard:
**   Receives key events via static callbacks registered with
**   emscripten_set_keydown/keyup_callback in EmscriptenMain.cpp.
**   Translates browser key codes to DIK_ scan codes and posts them into
**   the engine's existing KeyboardIO buffer via setKeyStatus().
**
** EmscriptenMouse:
**   Receives pointer events (move, buttons, wheel) via static callbacks.
**   Stores them in a small ring buffer; getMouseEvent() drains one per call,
**   matching the Win32Mouse contract the engine expects.
*/

#pragma once

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/KeyDefs.h"

// ===========================================================================
//  EmscriptenKeyboard
// ===========================================================================
class EmscriptenKeyboard : public Keyboard
{
public:
    EmscriptenKeyboard();
    virtual ~EmscriptenKeyboard() override;

    virtual void init()   override;
    virtual void reset()  override;
    virtual void update() override;

    virtual Bool getCapsState() override { return m_capsLock; }
    virtual void getKey( KeyboardIO *key ) override;

    // Called from EmscriptenMain HTML5 callbacks
    static void OnKeyDown( const EmscriptenKeyboardEvent *e );
    static void OnKeyUp(   const EmscriptenKeyboardEvent *e );

private:
    static EmscriptenKeyboard *s_instance;
    bool m_capsLock = false;

    // Translate a browser key string to a DIK_ scan code.
    // Returns 0 if not mapped.
    static KeyDefType BrowserKeyToDIK( const char *key, const char *code );

    void postKeyEvent( KeyDefType dik, KeyboardIO::StatusType status );
};

// ===========================================================================
//  EmscriptenMouse
// ===========================================================================

// Small ring buffer for mouse events fed from HTML5 callbacks
struct EmscriptenMouseEvent
{
    enum Type { NONE, MOVE, BUTTON_DOWN, BUTTON_UP, WHEEL } type = NONE;
    int x = 0, y = 0;
    int button = 0;       // 0=left 1=middle 2=right
    float wheelDelta = 0; // positive = up
};

class EmscriptenMouse : public Mouse
{
    static const int EVENT_BUF = 64;

public:
    EmscriptenMouse();
    virtual ~EmscriptenMouse() override;

    virtual void init()   override;
    virtual void reset()  override;
    virtual void update() override;

    virtual void initCursorResources() override {}
    virtual void setCursor( MouseCursor ) override {}
    virtual void capture()        override {}
    virtual void releaseCapture() override {}

    virtual UnsignedByte getMouseEvent( MouseIO *result, Bool flush ) override;

    // Called from EmscriptenMain HTML5 callbacks
    static void OnMouseMove  ( int x, int y );
    static void OnMouseDown  ( int x, int y, int button );
    static void OnMouseUp    ( int x, int y, int button );
    static void OnWheel      ( float delta );

private:
    static EmscriptenMouse *s_instance;

    EmscriptenMouseEvent m_buf[EVENT_BUF];
    int m_head = 0, m_tail = 0;

    void push( const EmscriptenMouseEvent &e );
    bool pop ( EmscriptenMouseEvent &e );
};

#endif // __EMSCRIPTEN__
