/*****************************************************************************
 * InterfaceWindow.h: BeOS interface window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: InterfaceWindow.h,v 1.12.2.3 2002/09/03 12:00:25 tcastley Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef BEOS_INTERFACE_WINDOW_H
#define BEOS_INTERFACE_WINDOW_H

#include <Menu.h>
#include <Window.h>

class BMenuBar;
class MediaControlView;
class PlayListWindow;
class BFilePanel;

class CDMenu : public BMenu
{
 public:
							CDMenu(const char* name);
	virtual					~CDMenu();

	virtual	void			AttachedToWindow(void);

 private:
	int						GetCD(const char* directory);
};

class LanguageMenu : public BMenu
{
 public:
							LanguageMenu(const char* name,
										 int menu_kind,
										 intf_thread_t* p_interface);
	virtual					~LanguageMenu();

	virtual	void			AttachedToWindow(void);

 private:
	intf_thread_t*			p_intf;
	int						kind;
	int						GetChannels();
};

class InterfaceWindow : public BWindow
{
 public:
							InterfaceWindow(BRect frame,
											const char* name,
											intf_thread_t* p_interface);
	virtual					~InterfaceWindow();

							// standard window member
	virtual	void			FrameResized(float width, float height);
	virtual	void			MessageReceived(BMessage* message);
	virtual	bool			QuitRequested();

							// InterfaceWindow
			void 			updateInterface();
			bool			IsStopped() const;
	    
	MediaControlView*		p_mediaControl;

 private:	
	void					_SetMenusEnabled(bool hasFile,
											 bool hasChapters = false,
											 bool hasTitles = false);

	intf_thread_t*			p_intf;
	bool					b_empty_playlist;
	BFilePanel*				file_panel;
	PlayListWindow*			playlist_window;
	BMenuItem*				miOnTop;
	es_descriptor_t*		p_audio_es;
	es_descriptor_t*		p_spu_es;
	BMenuBar*				fMenuBar;
	BMenuItem*				fNextTitleMI;
	BMenuItem*				fPrevTitleMI;
	BMenuItem*				fNextChapterMI;
	BMenuItem*				fPrevChapterMI;
	BMenu*					fAudioMenu;
	BMenu*					fNavigationMenu;
	BMenu*					fLanguageMenu;
	BMenu*					fSubtitlesMenu;
	BMenu*					fSpeedMenu;
	bigtime_t				fLastUpdateTime;
};

#endif	// BEOS_INTERFACE_WINDOW_H
