/*****************************************************************************
 * InterfaceWindow.h: BeOS interface window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: InterfaceWindow.h,v 1.13 2002/06/01 09:20:16 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Tony Castley <tcastley@mail.powerup.com.au>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
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
class MediaControlView;
class PlayListWindow;

class CDMenu : public BMenu
{
public:
	CDMenu(const char *name);
	~CDMenu();
	void AttachedToWindow(void);
private:
	int GetCD(const char *directory);
};

class LanguageMenu : public BMenu
{
public:
	LanguageMenu(const char *name, int menu_kind, 
	             intf_thread_t  *p_interface);
	~LanguageMenu();
	void AttachedToWindow(void);
private:
	intf_thread_t  *p_intf;
	int kind;
	int GetChannels();
};

class InterfaceWindow : public BWindow
{
public:
    InterfaceWindow( BRect frame, const char *name, 
                     intf_thread_t  *p_interface );
    ~InterfaceWindow();

    // standard window member
    virtual bool    QuitRequested();
    virtual void    MessageReceived(BMessage *message);
	void 			updateInterface();
	    
	MediaControlView *p_mediaControl;

private:	
    intf_thread_t  *p_intf;
    bool            b_empty_playlist;
	BFilePanel *file_panel;
	PlayListWindow* playlist_window;
    BMenuItem      *miOnTop;
	es_descriptor_t *  p_audio_es;
    es_descriptor_t *  p_spu_es;

};

