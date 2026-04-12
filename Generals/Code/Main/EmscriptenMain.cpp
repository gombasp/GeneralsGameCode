/*
** Command & Conquer Generals(tm)
** Copyright 2025 Electronic Arts Inc.
**
** EmscriptenMain.cpp
**
** Replaces WinMain.cpp when building with Emscripten.
** Provides:
**   - int main()       : one-time init, hands off to browser event loop
**   - WebMain_Loop()   : called ~60/s via requestAnimationFrame
**   - HTML5 input callbacks forwarded to EmscriptenKeyboard/EmscriptenMouse
**
** Compiled only when __EMSCRIPTEN__ is defined.
*/

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>
#include <cstdio>
#include <cstdlib>

// Engine includes (same as WinMain.cpp)
#include "Lib/BaseType.h"
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/GlobalData.h"
#include "Common/GameEngine.h"
#include "Common/Debug.h"
#include "Common/GameMemory.h"
#include "Common/MessageStream.h"
#include "Common/version.h"
#include "BuildVersion.h"
#include "GeneratedVersion.h"

// WebGL2 wrapper (provides DX8Wrapper typedef)
#include "webgl2wrapper.h"

// Emscripten input (keyboard + mouse)
#include "EmscriptenDevice/EmscriptenInput.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
const Char* g_strFile = "data/Generals.str";
const Char* g_csfFile = "data/%s/Generals.csf";
const char* gAppPrefix = "";

static CriticalSection critSec1, critSec2, critSec3, critSec4, critSec5;

static constexpr int CANVAS_WIDTH  = 1280;
static constexpr int CANVAS_HEIGHT = 720;

extern GameEngine* CreateGameEngine();

// ---------------------------------------------------------------------------
// Per-frame callback
// ---------------------------------------------------------------------------
static void WebMain_Loop()
{
    if (WebGL2Wrapper::Is_Device_Lost()) return;

    if (TheGameEngine)
    {
        TheGameEngine->execute();

        if (TheGameEngine->isQuitting())
        {
            emscripten_cancel_main_loop();
            TheGameEngine->reset();
            shutdownMemoryManager();
        }
    }
}

// ---------------------------------------------------------------------------
// HTML5 input callbacks — forwarded to EmscriptenKeyboard / EmscriptenMouse
// ---------------------------------------------------------------------------
static EM_BOOL OnKeyDown(int, const EmscriptenKeyboardEvent* e, void*)
{
    EmscriptenKeyboard::OnKeyDown(e);
    // Keep Escape handled at engine level too
    if (e->keyCode == 27 && TheGameEngine)
        TheGameEngine->setQuitting(TRUE);
    return EM_TRUE;
}

static EM_BOOL OnKeyUp(int, const EmscriptenKeyboardEvent* e, void*)
{
    EmscriptenKeyboard::OnKeyUp(e);
    return EM_TRUE;
}

static EM_BOOL OnMouseMove(int, const EmscriptenMouseEvent* e, void*)
{
    EmscriptenMouse::OnMouseMove(e->clientX, e->clientY);
    return EM_TRUE;
}

static EM_BOOL OnMouseDown(int, const EmscriptenMouseEvent* e, void*)
{
    EmscriptenMouse::OnMouseDown(e->clientX, e->clientY, e->button);
    return EM_TRUE;
}

static EM_BOOL OnMouseUp(int, const EmscriptenMouseEvent* e, void*)
{
    EmscriptenMouse::OnMouseUp(e->clientX, e->clientY, e->button);
    return EM_TRUE;
}

static EM_BOOL OnWheel(int, const EmscriptenWheelEvent* e, void*)
{
    // deltaY: positive = scroll down, negative = scroll up
    // Engine convention: positive wheel = scroll up → negate
    EmscriptenMouse::OnWheel(-(float)e->deltaY);
    return EM_TRUE;
}

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------
int main(int /*argc*/, char** /*argv*/)
{
    TheAsciiStringCriticalSection   = &critSec1;
    TheUnicodeStringCriticalSection = &critSec2;
    TheDmaCriticalSection           = &critSec3;
    TheMemoryPoolCriticalSection    = &critSec4;
    TheDebugLogCriticalSection      = &critSec5;

    initMemoryManager();

    CommandLine::parseCommandLineForStartup();

    TheVersion = NEW Version;
    TheVersion->setVersion(
        VERSION_MAJOR, VERSION_MINOR, VERSION_BUILDNUM, VERSION_LOCALBUILDNUM,
        AsciiString(VERSION_BUILDUSER), AsciiString(VERSION_BUILDLOC),
        AsciiString(__TIME__), AsciiString(__DATE__));

    // Init WebGL2
    if (!WebGL2Wrapper::Init("#canvas", CANVAS_WIDTH, CANVAS_HEIGHT))
    {
        printf("[Generals] WebGL2 init failed\n");
        return 1;
    }

    // Register HTML5 input callbacks
    emscripten_set_keydown_callback   (EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, OnKeyDown);
    emscripten_set_keyup_callback     (EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, OnKeyUp);
    emscripten_set_mousemove_callback ("#canvas", nullptr, false, OnMouseMove);
    emscripten_set_mousedown_callback ("#canvas", nullptr, false, OnMouseDown);
    emscripten_set_mouseup_callback   ("#canvas", nullptr, false, OnMouseUp);
    emscripten_set_wheel_callback     ("#canvas", nullptr, false, OnWheel);

    // Prevent right-click context menu on canvas
    EM_ASM(
        document.getElementById('canvas').addEventListener(
            'contextmenu', function(e) { e.preventDefault(); });
    );

    // Mount IndexedDB-backed filesystem for save games
    EM_ASM(
        FS.mkdir('/saves');
        FS.mount(IDBFS, {}, '/saves');
        FS.syncfs(true, function(err) {
            if (err) console.warn('[Generals] IDBFS sync error:', err);
        });
    );

    // Init engine
    TheGameEngine = CreateGameEngine();
    if (TheGameEngine) TheGameEngine->init();

    // Hand control to the browser event loop
    emscripten_set_main_loop(WebMain_Loop, 0, 1);

    return 0; // never reached
}

// ---------------------------------------------------------------------------
// GameEngine factory
// ---------------------------------------------------------------------------
#include "Win32Device/Common/Win32GameEngine.h"
GameEngine* CreateGameEngine()
{
    return NEW Win32GameEngine;
}

#endif // __EMSCRIPTEN__
