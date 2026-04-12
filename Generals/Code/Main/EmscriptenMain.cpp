/*
** Command & Conquer Generals(tm)
** Copyright 2025 Electronic Arts Inc.
**
** EmscriptenMain.cpp
**
** Replaces WinMain.cpp when building with Emscripten.
** Provides:
**   - int main()          : one-time init, hands off to browser event loop
**   - WebMain_Loop()      : called ~60/s by requestAnimationFrame
**   - Emscripten input callbacks bridging to the existing Mouse/Keyboard classes
**
** Compile guard: this file is only compiled when __EMSCRIPTEN__ is defined.
** The CMakeLists_web.txt build script selects this file instead of WinMain.cpp.
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

// ---------------------------------------------------------------------------
// Globals (mirrors WinMain.cpp globals needed by engine subsystems)
// ---------------------------------------------------------------------------
const Char* g_strFile = "data/Generals.str";
const Char* g_csfFile = "data/%s/Generals.csf";
const char* gAppPrefix = "";

static CriticalSection critSec1, critSec2, critSec3, critSec4, critSec5;

// Canvas resolution – can be overridden via URL params in the future
static constexpr int CANVAS_WIDTH  = 1280;
static constexpr int CANVAS_HEIGHT = 720;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
extern int  GameMain();          // defined in existing engine code
extern GameEngine* CreateGameEngine();

// ---------------------------------------------------------------------------
// Per-frame callback (replaces the Win32 message loop)
// ---------------------------------------------------------------------------
static void WebMain_Loop()
{
    if (WebGL2Wrapper::Is_Device_Lost()) return;

    // The engine's GameMain loop pumps TheGameEngine->update() internally.
    // For Emscripten we call the engine's single-frame update instead.
    // Adapt this to whatever the engine exposes (execute(), update(), etc.)
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
// Input bridging
// ---------------------------------------------------------------------------
static EM_BOOL OnKeyDown(int, const EmscriptenKeyboardEvent* e, void*)
{
    // TODO: translate e->keyCode to the engine's key enum and post to
    // TheKeyboard / TheMessageStream.  Minimal implementation for now.
    if (e->keyCode == 27 /*Escape*/)
    {
        if (TheGameEngine) TheGameEngine->setQuitting(TRUE);
    }
    return EM_TRUE;
}

static EM_BOOL OnMouseMove(int, const EmscriptenMouseEvent* e, void*)
{
    // TODO: forward to TheWin32Mouse equivalent
    // TheMouseManager->set_position(e->clientX, e->clientY);
    (void)e;
    return EM_TRUE;
}

static EM_BOOL OnMouseButton(int event_type, const EmscriptenMouseEvent* e, void*)
{
    // TODO: forward button events to input manager
    (void)event_type; (void)e;
    return EM_TRUE;
}

static EM_BOOL OnWheel(int, const EmscriptenWheelEvent* e, void*)
{
    // TODO: forward wheel delta to mouse manager
    (void)e;
    return EM_TRUE;
}

// ---------------------------------------------------------------------------
// main() – Emscripten entry point
// ---------------------------------------------------------------------------
int main(int /*argc*/, char** /*argv*/)
{
    // Critical sections (same as WinMain)
    TheAsciiStringCriticalSection   = &critSec1;
    TheUnicodeStringCriticalSection = &critSec2;
    TheDmaCriticalSection           = &critSec3;
    TheMemoryPoolCriticalSection    = &critSec4;
    TheDebugLogCriticalSection      = &critSec5;

    initMemoryManager();

    // Parse command line (Emscripten passes query string params as argv)
    CommandLine::parseCommandLineForStartup();

    // Version
    TheVersion = NEW Version;
    TheVersion->setVersion(
        VERSION_MAJOR, VERSION_MINOR, VERSION_BUILDNUM, VERSION_LOCALBUILDNUM,
        AsciiString(VERSION_BUILDUSER), AsciiString(VERSION_BUILDLOC),
        AsciiString(__TIME__), AsciiString(__DATE__));

    // Init WebGL2 – canvas element must exist in index.html with id="canvas"
    if (!WebGL2Wrapper::Init("#canvas", CANVAS_WIDTH, CANVAS_HEIGHT))
    {
        printf("[Generals] WebGL2 init failed – check that your browser supports WebGL2.\n");
        return 1;
    }

    // Register input callbacks on the canvas
    emscripten_set_keydown_callback    (EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, OnKeyDown);
    emscripten_set_mousemove_callback  ("#canvas", nullptr, false, OnMouseMove);
    emscripten_set_mousedown_callback  ("#canvas", nullptr, false, OnMouseButton);
    emscripten_set_mouseup_callback    ("#canvas", nullptr, false, OnMouseButton);
    emscripten_set_wheel_callback      ("#canvas", nullptr, false, OnWheel);

    // Create and init the game engine (same factory function as WinMain)
    // GameMain() in the original code runs the full loop; here we call it
    // only for init then hand off to emscripten_set_main_loop.
    // If GameMain() blocks (runs its own loop), wrap it in a separate
    // Emscripten asyncify call:  emscripten_set_main_loop(GameMain_Asyncify, 0, 0)
    // For now we assume the engine can be driven frame-by-frame:
    TheGameEngine = CreateGameEngine();
    if (TheGameEngine) TheGameEngine->init();

    // Hand control to the browser.
    // fps=0  → use requestAnimationFrame (~60 fps, respects vsync)
    // simulate_infinite_loop=1 → main() never returns (required)
    emscripten_set_main_loop(WebMain_Loop, 0, 1);

    // Never reached:
    return 0;
}

// GameEngine factory (mirrors WinMain.cpp's CreateGameEngine)
// This is declared extern in GameEngine headers and must be defined per-platform.
// The web version uses the same Win32GameEngine since we're cross-compiling;
// swap for a WebGameEngine subclass if you create one later.
#include "Win32Device/Common/Win32GameEngine.h"
GameEngine* CreateGameEngine()
{
    return NEW Win32GameEngine;
}

#endif // __EMSCRIPTEN__
