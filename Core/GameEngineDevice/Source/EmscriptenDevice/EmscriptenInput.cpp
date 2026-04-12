/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** EmscriptenInput.cpp  –  Keyboard + Mouse for WebAssembly builds.
** Compiled only when __EMSCRIPTEN__ is defined.
*/

#ifdef __EMSCRIPTEN__

#include "EmscriptenDevice/EmscriptenInput.h"
#include <cstring>
#include <cstdio>

// ===========================================================================
//  EmscriptenKeyboard
// ===========================================================================

EmscriptenKeyboard *EmscriptenKeyboard::s_instance = nullptr;

EmscriptenKeyboard::EmscriptenKeyboard()
{
    s_instance = this;
}

EmscriptenKeyboard::~EmscriptenKeyboard()
{
    if (s_instance == this) s_instance = nullptr;
}

void EmscriptenKeyboard::init()
{
    Keyboard::init();
}

void EmscriptenKeyboard::reset()
{
    Keyboard::reset();
    m_capsLock = false;
}

void EmscriptenKeyboard::update()
{
    // Key events are pushed directly into the buffer by OnKeyDown/OnKeyUp.
    // The base class update() drains m_keys[] into the stream.
    Keyboard::update();
}

void EmscriptenKeyboard::getKey( KeyboardIO *key )
{
    // Base class implementation reads from m_keys[]; nothing extra needed here.
    key->key    = KEY_NONE;
    key->status = KeyboardIO::STATUS_UNUSED;
}

void EmscriptenKeyboard::postKeyEvent( KeyDefType dik, KeyboardIO::StatusType status )
{
    if (!dik) return;
    setKeyStatus( dik, status, 0 );
}

void EmscriptenKeyboard::OnKeyDown( const EmscriptenKeyboardEvent *e )
{
    if (!s_instance) return;
    if (strcmp(e->key, "CapsLock") == 0)
        s_instance->m_capsLock = !s_instance->m_capsLock;
    KeyDefType dik = BrowserKeyToDIK(e->key, e->code);
    s_instance->postKeyEvent(dik, KeyboardIO::STATUS_DOWN);
}

void EmscriptenKeyboard::OnKeyUp( const EmscriptenKeyboardEvent *e )
{
    if (!s_instance) return;
    KeyDefType dik = BrowserKeyToDIK(e->key, e->code);
    s_instance->postKeyEvent(dik, KeyboardIO::STATUS_UP);
}

// ---------------------------------------------------------------------------
// Browser key → DIK_ scan code translation table.
// We match on e->code (physical key) first, then fall back to e->key (char).
// ---------------------------------------------------------------------------
KeyDefType EmscriptenKeyboard::BrowserKeyToDIK( const char *key, const char *code )
{
    // Physical key codes (layout-independent)
    struct { const char *code; KeyDefType dik; } codeMap[] = {
        {"Escape",        DIK_ESCAPE},
        {"Digit1",        DIK_1},    {"Digit2",  DIK_2},    {"Digit3",  DIK_3},
        {"Digit4",        DIK_4},    {"Digit5",  DIK_5},    {"Digit6",  DIK_6},
        {"Digit7",        DIK_7},    {"Digit8",  DIK_8},    {"Digit9",  DIK_9},
        {"Digit0",        DIK_0},
        {"Minus",         DIK_MINUS},
        {"Equal",         DIK_EQUALS},
        {"Backspace",     DIK_BACK},
        {"Tab",           DIK_TAB},
        {"KeyQ",          DIK_Q},    {"KeyW",    DIK_W},    {"KeyE",    DIK_E},
        {"KeyR",          DIK_R},    {"KeyT",    DIK_T},    {"KeyY",    DIK_Y},
        {"KeyU",          DIK_U},    {"KeyI",    DIK_I},    {"KeyO",    DIK_O},
        {"KeyP",          DIK_P},
        {"BracketLeft",   DIK_LBRACKET},
        {"BracketRight",  DIK_RBRACKET},
        {"Enter",         DIK_RETURN},
        {"ControlLeft",   DIK_LCONTROL},
        {"KeyA",          DIK_A},    {"KeyS",    DIK_S},    {"KeyD",    DIK_D},
        {"KeyF",          DIK_F},    {"KeyG",    DIK_G},    {"KeyH",    DIK_H},
        {"KeyJ",          DIK_J},    {"KeyK",    DIK_K},    {"KeyL",    DIK_L},
        {"Semicolon",     DIK_SEMICOLON},
        {"Quote",         DIK_APOSTROPHE},
        {"Backquote",     DIK_GRAVE},
        {"ShiftLeft",     DIK_LSHIFT},
        {"Backslash",     DIK_BACKSLASH},
        {"KeyZ",          DIK_Z},    {"KeyX",    DIK_X},    {"KeyC",    DIK_C},
        {"KeyV",          DIK_V},    {"KeyB",    DIK_B},    {"KeyN",    DIK_N},
        {"KeyM",          DIK_M},
        {"Comma",         DIK_COMMA},
        {"Period",        DIK_PERIOD},
        {"Slash",         DIK_SLASH},
        {"ShiftRight",    DIK_RSHIFT},
        {"NumpadMultiply",DIK_NUMPADSTAR},
        {"AltLeft",       DIK_LALT},
        {"Space",         DIK_SPACE},
        {"CapsLock",      DIK_CAPSLOCK},
        {"F1",            DIK_F1},   {"F2",      DIK_F2},   {"F3",      DIK_F3},
        {"F4",            DIK_F4},   {"F5",      DIK_F5},   {"F6",      DIK_F6},
        {"F7",            DIK_F7},   {"F8",      DIK_F8},   {"F9",      DIK_F9},
        {"F10",           DIK_F10},
        {"NumLock",       DIK_NUMLOCK},
        {"ScrollLock",    DIK_SCROLL},
        {"Numpad7",       DIK_NUMPAD7}, {"Numpad8",  DIK_NUMPAD8}, {"Numpad9",  DIK_NUMPAD9},
        {"NumpadSubtract",DIK_NUMPADMINUS},
        {"Numpad4",       DIK_NUMPAD4}, {"Numpad5",  DIK_NUMPAD5}, {"Numpad6",  DIK_NUMPAD6},
        {"NumpadAdd",     DIK_NUMPADPLUS},
        {"Numpad1",       DIK_NUMPAD1}, {"Numpad2",  DIK_NUMPAD2}, {"Numpad3",  DIK_NUMPAD3},
        {"Numpad0",       DIK_NUMPAD0},
        {"NumpadDecimal", DIK_NUMPADPERIOD},
        {"F11",           DIK_F11},
        {"F12",           DIK_F12},
        {"NumpadEnter",   DIK_NUMPADENTER},
        {"ControlRight",  DIK_RCONTROL},
        {"NumpadDivide",  DIK_NUMPADSLASH},
        {"PrintScreen",   DIK_SYSRQ},
        {"AltRight",      DIK_RALT},
        {"Home",          DIK_HOME},
        {"ArrowUp",       DIK_UPARROW},
        {"PageUp",        DIK_PGUP},
        {"ArrowLeft",     DIK_LEFTARROW},
        {"ArrowRight",    DIK_RIGHTARROW},
        {"End",           DIK_END},
        {"ArrowDown",     DIK_DOWNARROW},
        {"PageDown",      DIK_PGDN},
        {"Insert",        DIK_INSERT},
        {"Delete",        DIK_DELETE},
        {nullptr,         (KeyDefType)0}
    };

    for (int i = 0; codeMap[i].code; ++i)
        if (strcmp(code, codeMap[i].code) == 0)
            return codeMap[i].dik;

    return (KeyDefType)0;
}

// ===========================================================================
//  EmscriptenMouse
// ===========================================================================

EmscriptenMouse *EmscriptenMouse::s_instance = nullptr;

EmscriptenMouse::EmscriptenMouse()
{
    s_instance = this;
    memset(m_buf, 0, sizeof(m_buf));
}

EmscriptenMouse::~EmscriptenMouse()
{
    if (s_instance == this) s_instance = nullptr;
}

void EmscriptenMouse::init()
{
    Mouse::init();
}

void EmscriptenMouse::reset()
{
    Mouse::reset();
    m_head = m_tail = 0;
}

void EmscriptenMouse::update()
{
    Mouse::update();
}

void EmscriptenMouse::push( const EmscriptenMouseEvent &e )
{
    int next = (m_tail + 1) % EVENT_BUF;
    if (next == m_head) return; // full — drop oldest
    m_buf[m_tail] = e;
    m_tail = next;
}

bool EmscriptenMouse::pop( EmscriptenMouseEvent &e )
{
    if (m_head == m_tail) return false;
    e = m_buf[m_head];
    m_head = (m_head + 1) % EVENT_BUF;
    return true;
}

UnsignedByte EmscriptenMouse::getMouseEvent( MouseIO *result, Bool flush )
{
    EmscriptenMouseEvent ev;
    if (!pop(ev)) return MOUSE_NONE;

    result->pos.x = ev.x;
    result->pos.y = ev.y;
    result->wheel = 0;

    switch (ev.type)
    {
    case EmscriptenMouseEvent::MOVE:
        result->leftState   = 0;
        result->rightState  = 0;
        result->middleState = 0;
        return MOUSE_MOVE;

    case EmscriptenMouseEvent::BUTTON_DOWN:
        if (ev.button == 0) { result->leftState   = 1; return MOUSE_LEFT_BUTTON_DOWN;   }
        if (ev.button == 2) { result->rightState  = 1; return MOUSE_RIGHT_BUTTON_DOWN;  }
        if (ev.button == 1) { result->middleState = 1; return MOUSE_MIDDLE_BUTTON_DOWN; }
        break;

    case EmscriptenMouseEvent::BUTTON_UP:
        if (ev.button == 0) { result->leftState   = 0; return MOUSE_LEFT_BUTTON_UP;   }
        if (ev.button == 2) { result->rightState  = 0; return MOUSE_RIGHT_BUTTON_UP;  }
        if (ev.button == 1) { result->middleState = 0; return MOUSE_MIDDLE_BUTTON_UP; }
        break;

    case EmscriptenMouseEvent::WHEEL:
        result->wheel = (ev.wheelDelta > 0) ? 1 : -1;
        return MOUSE_WHEEL_UP + (ev.wheelDelta < 0 ? 1 : 0);

    default: break;
    }
    return MOUSE_NONE;
}

void EmscriptenMouse::OnMouseMove( int x, int y )
{
    if (!s_instance) return;
    EmscriptenMouseEvent e;
    e.type = EmscriptenMouseEvent::MOVE; e.x = x; e.y = y;
    s_instance->push(e);
}

void EmscriptenMouse::OnMouseDown( int x, int y, int button )
{
    if (!s_instance) return;
    EmscriptenMouseEvent e;
    e.type = EmscriptenMouseEvent::BUTTON_DOWN; e.x = x; e.y = y; e.button = button;
    s_instance->push(e);
}

void EmscriptenMouse::OnMouseUp( int x, int y, int button )
{
    if (!s_instance) return;
    EmscriptenMouseEvent e;
    e.type = EmscriptenMouseEvent::BUTTON_UP; e.x = x; e.y = y; e.button = button;
    s_instance->push(e);
}

void EmscriptenMouse::OnWheel( float delta )
{
    if (!s_instance) return;
    EmscriptenMouseEvent e;
    e.type = EmscriptenMouseEvent::WHEEL; e.wheelDelta = delta;
    s_instance->push(e);
}

#endif // __EMSCRIPTEN__
