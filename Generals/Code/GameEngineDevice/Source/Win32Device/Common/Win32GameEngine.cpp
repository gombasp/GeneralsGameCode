/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: W3DGameEngine.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2001
// Description:
//   Implementation of the Win32 game engine, this is the highest level of
//   the game application, it creates all the devices we will use for the game
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#include "Win32Device/Common/Win32GameEngine.h"
#include "Common/PerfTimer.h"

#include "GameNetwork/LANAPICallbacks.h"

extern DWORD TheMessageTime;

//-------------------------------------------------------------------------------------------------
/** Constructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::Win32GameEngine()
{
#ifndef __EMSCRIPTEN__
	// Stop blue screen
	m_previousErrorMode = SetErrorMode( SEM_FAILCRITICALERRORS );
#else
	m_previousErrorMode = 0;
#endif
}

//-------------------------------------------------------------------------------------------------
/** Destructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::~Win32GameEngine()
{
#ifndef __EMSCRIPTEN__
	// restore it (this isn't really necessary, but feels good.)
	SetErrorMode( m_previousErrorMode );
#endif
}


//-------------------------------------------------------------------------------------------------
/** Initialize the game engine */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::init()
{

	// extending functionality
	GameEngine::init();

}

//-------------------------------------------------------------------------------------------------
/** Reset the system */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::reset()
{

	// extending functionality
	GameEngine::reset();

}

//-------------------------------------------------------------------------------------------------
/** Update the game engine by updating the GameClient and
	* GameLogic singletons. */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::update()
{
	// call the engine normal update
	GameEngine::update();

#ifndef __EMSCRIPTEN__
	extern HWND ApplicationHWnd;
	if (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
		while (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
			// We are alt-tabbed out here.  Sleep a bit, & process windows
			// so that we can become un-alt-tabbed out.
			Sleep(5);
			serviceWindowsOS();

			if (TheLAN != nullptr) {
				// BGC - need to update TheLAN so we can process and respond to other
				// people's messages who may not be alt-tabbed out like we are.
				TheLAN->setIsActive(isActive());
				TheLAN->update();
			}

			// If we are running a multiplayer game, keep running the logic.
			// There is code in the client to skip client redraw if we are
			// iconic.  jba.
			if (TheGameEngine->getQuitting() || TheGameLogic->isInInternetGame() || TheGameLogic->isInLanGame()) {
				break; // keep running.
			}
		}
	}

	// allow windows to perform regular windows maintenance stuff like msgs
	serviceWindowsOS();
#endif
}

//-------------------------------------------------------------------------------------------------
/** This function may be called from within this application to let
  * Microsoft Windows do its message processing and dispatching. */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::serviceWindowsOS()
{
#ifndef __EMSCRIPTEN__
	MSG msg;
  Int returnValue;

	while( PeekMessage( &msg, nullptr, 0, 0, PM_NOREMOVE ) )
	{
		returnValue = GetMessage( &msg, nullptr, 0, 0 );

		TheMessageTime = msg.time;
		TranslateMessage( &msg );
		DispatchMessage( &msg );
		TheMessageTime = 0;
	}
#endif
}

