/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 TheSuperHackers / WebGL2 Port Contributors
**
** win32_types_compat.h
**
** Minimal Win32 type aliases for Emscripten / non-Windows builds.
** Provides DWORD, BOOL, HANDLE, HWND and other fundamental types
** that engine code uses but that normally come from windows.h.
**
** Only included when __EMSCRIPTEN__ is defined (see PreRTS.h).
** On Windows, windows.h provides all of these.
*/

#pragma once

#ifndef _WIN32

#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Basic integer types
// ---------------------------------------------------------------------------
typedef unsigned long       DWORD;
typedef unsigned short      WORD;
typedef unsigned char       BYTE;
typedef int                 BOOL;
typedef unsigned int        UINT;
typedef int                 INT;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef long long           LONGLONG;
typedef unsigned long long  ULONGLONG;
typedef float               FLOAT;

#ifndef TRUE
#  define TRUE  1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

// ---------------------------------------------------------------------------
// Pointer-sized types
// ---------------------------------------------------------------------------
typedef uintptr_t           ULONG_PTR;
typedef uintptr_t           DWORD_PTR;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           UINT_PTR;

// ---------------------------------------------------------------------------
// Handle types (opaque pointers — all null on web, never dereferenced)
// ---------------------------------------------------------------------------
typedef void*   HANDLE;
typedef void*   HWND;
typedef void*   HINSTANCE;
typedef void*   HMODULE;
typedef void*   HKEY;
typedef void*   HDC;
typedef void*   HBITMAP;
typedef void*   HICON;
typedef void*   HCURSOR;
typedef void*   HMENU;
typedef void*   HBRUSH;
typedef void*   HFONT;
typedef void*   HPEN;
typedef void*   HRGN;
typedef void*   HGLOBAL;
typedef void*   HLOCAL;
typedef void*   HRSRC;
typedef void*   HTASK;
typedef void*   HFILE;

// ---------------------------------------------------------------------------
// String types
// ---------------------------------------------------------------------------
typedef char            CHAR;
typedef wchar_t         WCHAR;
typedef char*           LPSTR;
typedef const char*     LPCSTR;
typedef wchar_t*        LPWSTR;
typedef const wchar_t*  LPCWSTR;
typedef void*           LPVOID;
typedef const void*     LPCVOID;

// ---------------------------------------------------------------------------
// Message / window types (stubs — WinMain is not compiled on web)
// ---------------------------------------------------------------------------
typedef uintptr_t   WPARAM;
typedef intptr_t    LPARAM;
typedef intptr_t    LRESULT;

// ---------------------------------------------------------------------------
// HRESULT and COM stubs
// ---------------------------------------------------------------------------
typedef long    HRESULT;
#define S_OK            ((HRESULT)0x00000000L)
#define S_FALSE         ((HRESULT)0x00000001L)
#define E_FAIL          ((HRESULT)0x80004005L)
#define E_NOINTERFACE   ((HRESULT)0x80004002L)
#define E_OUTOFMEMORY   ((HRESULT)0x8007000EL)
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)

// IUnknown stub (needed by a few headers that forward-declare it)
struct IUnknown {
    virtual HRESULT QueryInterface(const void* riid, void** ppv) { return E_NOINTERFACE; }
    virtual unsigned long AddRef()  { return 0; }
    virtual unsigned long Release() { return 0; }
};

// ---------------------------------------------------------------------------
// WINAPI / calling convention macros
// ---------------------------------------------------------------------------
#ifndef WINAPI
#  define WINAPI
#endif
#ifndef CALLBACK
#  define CALLBACK
#endif
#ifndef APIENTRY
#  define APIENTRY
#endif
#ifndef PASCAL
#  define PASCAL
#endif
#ifndef FAR
#  define FAR
#endif
#ifndef NEAR
#  define NEAR
#endif
#ifndef CONST
#  define CONST const
#endif
#ifndef VOID
#  define VOID void
#endif

// ---------------------------------------------------------------------------
// MAKEWORD / MAKELONG / LOWORD / HIWORD
// ---------------------------------------------------------------------------
#ifndef MAKEWORD
#  define MAKEWORD(l,h) ((WORD)(((BYTE)(l)) | (((WORD)((BYTE)(h))) << 8)))
#endif
#ifndef MAKELONG
#  define MAKELONG(l,h) ((long)(((WORD)(l)) | (((DWORD)((WORD)(h))) << 16)))
#endif
#ifndef LOWORD
#  define LOWORD(l) ((WORD)(((DWORD)(l)) & 0xffff))
#endif
#ifndef HIWORD
#  define HIWORD(l) ((WORD)((((DWORD)(l)) >> 16) & 0xffff))
#endif
#ifndef LOBYTE
#  define LOBYTE(w) ((BYTE)(w))
#endif
#ifndef HIBYTE
#  define HIBYTE(w) ((BYTE)(((WORD)(w) >> 8) & 0xFF))
#endif

// ---------------------------------------------------------------------------
// RECT / POINT / SIZE
// ---------------------------------------------------------------------------
typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE  { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT  { LONG left, top, right, bottom; } RECT, *LPRECT;

// ---------------------------------------------------------------------------
// Registry stubs (all return failure — replaced by INI on web)
// ---------------------------------------------------------------------------
#define HKEY_LOCAL_MACHINE  ((HKEY)(uintptr_t)0x80000002)
#define HKEY_CURRENT_USER   ((HKEY)(uintptr_t)0x80000001)
#define KEY_READ            0x20019
#define ERROR_SUCCESS       0L
typedef DWORD REGSAM;
inline long RegOpenKeyEx(HKEY,const char*,DWORD,REGSAM,HKEY*) { return 1; }
inline long RegQueryValueEx(HKEY,const char*,DWORD*,DWORD*,BYTE*,DWORD*) { return 1; }
inline long RegCloseKey(HKEY) { return 0; }
inline long RegCreateKeyEx(HKEY,const char*,DWORD,char*,DWORD,REGSAM,void*,HKEY*,DWORD*) { return 1; }
inline long RegSetValueEx(HKEY,const char*,DWORD,DWORD,const BYTE*,DWORD) { return 1; }

// ---------------------------------------------------------------------------
// GetTickCount / timeGetTime already provided by time_compat.h
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Mutex stubs (no multi-window / instance guard needed on web)
// ---------------------------------------------------------------------------
#define INFINITE    0xFFFFFFFFUL
inline HANDLE CreateMutex(void*,BOOL,const char*) { return nullptr; }
inline HANDLE OpenMutex(DWORD,BOOL,const char*)   { return nullptr; }
inline BOOL   ReleaseMutex(HANDLE)                { return TRUE; }
inline DWORD  WaitForSingleObject(HANDLE,DWORD)   { return 0; }
inline BOOL   CloseHandle(HANDLE)                 { return TRUE; }

// ---------------------------------------------------------------------------
// Library loading stubs (Miles, GameSpy DLLs — not used on web)
// ---------------------------------------------------------------------------
inline HMODULE LoadLibrary(const char*)            { return nullptr; }
inline HMODULE LoadLibraryEx(const char*,HANDLE,DWORD) { return nullptr; }
inline BOOL    FreeLibrary(HMODULE)                { return TRUE; }
inline void*   GetProcAddress(HMODULE,const char*) { return nullptr; }

// ---------------------------------------------------------------------------
// Shell / path stubs
// ---------------------------------------------------------------------------
#define CSIDL_PERSONAL          0x0005
#define CSIDL_DESKTOPDIRECTORY  0x0010
inline BOOL SHGetSpecialFolderPath(HWND,char* path,int,BOOL)
{
    if (path) path[0] = '\0';
    return FALSE;
}

// ---------------------------------------------------------------------------
// GUID stub (used in COM forward-declarations)
// ---------------------------------------------------------------------------
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct { unsigned long  Data1; unsigned short Data2; unsigned short Data3; unsigned char Data4[8]; } GUID;
typedef const GUID& REFIID;
typedef const GUID& REFCLSID;
#endif

// ---------------------------------------------------------------------------
// GetLastError stub
// ---------------------------------------------------------------------------
inline DWORD GetLastError() { return 0; }

// ---------------------------------------------------------------------------
// _access (from io.h) — available as access() in unistd.h on Emscripten
// ---------------------------------------------------------------------------
#include <unistd.h>
#ifndef _access
#  define _access access
#endif

// ---------------------------------------------------------------------------
// _MAX_PATH already defined by compat.h — guard against double-definition
// ---------------------------------------------------------------------------
#ifndef _MAX_PATH
#  define _MAX_PATH 260
#endif

#endif // !_WIN32

// ---------------------------------------------------------------------------
// QueryPerformanceCounter / QueryPerformanceFrequency
// Replaced by emscripten_get_now() which gives sub-millisecond precision.
// ---------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
typedef union { struct { DWORD LowPart; LONG HighPart; }; long long QuadPart; } LARGE_INTEGER;
typedef LARGE_INTEGER* PLARGE_INTEGER;

inline BOOL QueryPerformanceCounter(LARGE_INTEGER* lp)
{
    if (lp) lp->QuadPart = (long long)(emscripten_get_now() * 1000.0); // microseconds
    return TRUE;
}
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* lp)
{
    if (lp) lp->QuadPart = 1000000LL; // 1 MHz = microseconds
    return TRUE;
}
// _LARGE_INTEGER alias used in a few places
typedef LARGE_INTEGER _LARGE_INTEGER;
#endif // __EMSCRIPTEN__

// ---------------------------------------------------------------------------
// Winsock stubs (WSADATA, WSAStartup, WSACleanup)
// On Emscripten sockets go through the POSIX layer directly; no init needed.
// ---------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
struct WSADATA { WORD wVersion; WORD wHighVersion; };
inline int WSAStartup(WORD, WSADATA* d) { if(d){d->wVersion=0x0202;d->wHighVersion=0x0202;} return 0; }
inline int WSACleanup() { return 0; }
inline int WSAGetLastError() { return errno; }
// ioctlsocket — map to fcntl on POSIX
#include <fcntl.h>
inline int ioctlsocket(int s, long cmd, unsigned long* argp)
{
    if (cmd == 0x8004667EL /*FIONBIO*/) {
        int flags = fcntl(s, F_GETFL, 0);
        if (*argp) fcntl(s, F_SETFL, flags | O_NONBLOCK);
        else       fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
        return 0;
    }
    return -1;
}
#define FIONBIO 0x8004667EL
#define SD_BOTH SHUT_RDWR
#endif // __EMSCRIPTEN__

// ---------------------------------------------------------------------------
// Win32 string / file API aliases for Emscripten (POSIX equivalents)
// ---------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
#include <string.h>
#include <unistd.h>
#define _strdup          strdup
#define lstrcmpi         strcasecmp
#define lstrcpy          strcpy
#define lstrcat          strcat
#define lstrlen          strlen
#define lstrcmp          strcmp
#define lstrcpyn         strncpy
#define MAX_PATH         260
// GetCurrentDirectory(size, buf) -> getcwd(buf, size) but different signature
inline DWORD GetCurrentDirectory(DWORD sz, char* buf) {
    return getcwd(buf, sz) ? (DWORD)strlen(buf) : 0;
}
// GetFileAttributes returns INVALID_FILE_ATTRIBUTES (0xFFFFFFFF) if not found
inline DWORD GetFileAttributes(const char* path) {
    return (access(path, F_OK) == 0) ? 0 : 0xFFFFFFFF;
}
#define INVALID_FILE_ATTRIBUTES ((DWORD)0xFFFFFFFF)
#endif // __EMSCRIPTEN__
