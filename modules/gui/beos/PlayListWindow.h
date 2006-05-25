/*****************************************************************************
 * PlayListWindow.h: BeOS interface window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Tony Castley <tcastley@mail.powerup.com.au>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#if 0
#ifndef BEOS_PLAY_LIST_WINDOW_H
#define BEOS_PLAY_LIST_WINDOW_H

#include <Window.h>

class BMenuItem;
class InterfaceWindow;
class PlaylistView;

class PlayListWindow : public BWindow
{
 public:
								PlayListWindow(BRect frame,
											   const char* name,
											   InterfaceWindow* mainWindow,
											   intf_thread_t *p_interface );
	virtual						~PlayListWindow();

								// BWindow
	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage *message);
	virtual	void				FrameResized(float width, float height);

								// PlayListWindow
			void				ReallyQuit();
			void				UpdatePlaylist( bool rebuild = false );

			void				SetDisplayMode( uint32 mode );
			uint32				DisplayMode() const;

 private:	
			void				_CheckItemsEnableState() const;
			void				_SetMenuItemEnabled( BMenuItem* item,
													 bool enabled ) const;

			PlaylistView *      fListView;
			BView *             fBackgroundView;
			BMenuBar *          fMenuBar;
			InterfaceWindow *   fMainWindow;

			BMenuItem*			fSelectAllMI;
			BMenuItem*			fSelectNoneMI;
			BMenuItem*			fSortReverseMI;
			BMenuItem*			fSortNameMI;
			BMenuItem*			fSortPathMI;
			BMenuItem*			fRandomizeMI;
			BMenuItem*			fRemoveMI;
			BMenuItem*			fRemoveAllMI;
			BMenu*				fViewMenu;
			
			intf_thread_t *     p_intf;
};

#endif	// BEOS_PLAY_LIST_WINDOW_H
#endif

