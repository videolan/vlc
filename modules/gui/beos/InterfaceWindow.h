/*****************************************************************************
 * InterfaceWindow.h: BeOS interface window class prototype
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

#ifndef BEOS_INTERFACE_WINDOW_H
#define BEOS_INTERFACE_WINDOW_H

#include <Menu.h>
#include <Window.h>

class BMenuBar;
class MediaControlView;
class PlayListWindow;
class BFilePanel;
class PreferencesWindow;
class MessagesWindow;

class CDMenu : public BMenu
{
 public:
                            CDMenu( const char* name );
    virtual                 ~CDMenu();

    virtual void            AttachedToWindow();

 private:
    int                     GetCD( const char* directory );
};

class LanguageMenu : public BMenu
{
 public:
                            LanguageMenu( intf_thread_t * p_intf,
                                          const char * psz_name,
                                          char * psz_variable );
    virtual                 ~LanguageMenu();

    virtual void            AttachedToWindow();

 private:
    intf_thread_t         * p_intf;
    char                  * psz_variable;
};

class TitleMenu : public BMenu
{
 public:
                            TitleMenu( const char* name, intf_thread_t  *p_interface );
    virtual                 ~TitleMenu();

    virtual void            AttachedToWindow();

    intf_thread_t  *p_intf;
};

class ChapterMenu : public BMenu
{
 public:
                            ChapterMenu( const char* name, intf_thread_t  *p_interface );
    virtual                 ~ChapterMenu();

    virtual void            AttachedToWindow();

    intf_thread_t  *p_intf;
};


class InterfaceWindow : public BWindow
{
 public:
                            InterfaceWindow( intf_thread_t * p_intf,
                                             BRect frame,
                                             const char * name );
    virtual                 ~InterfaceWindow();

                            // BWindow
    virtual void            FrameResized( float width, float height );
    virtual void            MessageReceived( BMessage* message );
    virtual bool            QuitRequested();

                            // InterfaceWindow
            void            UpdateInterface();
            void            UpdatePlaylist();

            bool            IsStopped() const;
        
    MediaControlView*        p_mediaControl;
    MessagesWindow*         fMessagesWindow;

 private:    
            void            _SetMenusEnabled( bool hasFile,
                                              bool hasChapters = false,
                                              bool hasTitles = false );
            void            _UpdateSpeedMenu( int rate );
            void            _ShowFilePanel( uint32 command,
                                            const char* windowTitle );
            void            _RestoreSettings();
            void            _StoreSettings();

    intf_thread_t         * p_intf;
    input_thread_t        * p_input;
    playlist_t            * p_playlist;
    es_descriptor_t       * p_spu_es;
    bool                    b_playlist_update;

    BFilePanel*             fFilePanel;
    PlayListWindow*         fPlaylistWindow;
    PreferencesWindow*      fPreferencesWindow;
    BMenuBar*               fMenuBar;
    BMenuItem*              fGotoMenuMI;
    BMenuItem*              fNextTitleMI;
    BMenuItem*              fPrevTitleMI;
    BMenuItem*              fNextChapterMI;
    BMenuItem*              fPrevChapterMI;
    BMenuItem*              fOnTopMI;
    BMenuItem*              fHeighthMI;
    BMenuItem*              fQuarterMI;
    BMenuItem*              fHalfMI;
    BMenuItem*              fNormalMI;
    BMenuItem*              fTwiceMI;
    BMenuItem*              fFourMI;
    BMenuItem*              fHeightMI;
    BMenu*                  fAudioMenu;
    BMenu*                  fNavigationMenu;
    BMenu*                  fTitleMenu;
    BMenu*                  fChapterMenu;
    BMenu*                  fLanguageMenu;
    BMenu*                  fSubtitlesMenu;
    BMenu*                  fSpeedMenu;
    BMenu*                  fShowMenu;
    bigtime_t               fLastUpdateTime;
    BMessage*               fSettings;  // we keep the message arround
                                        // for forward compatibility
};


// some global support functions
status_t load_settings( BMessage* message,
                        const char* fileName,
                        const char* folder = NULL );

status_t save_settings( BMessage* message,
                        const char* fileName,
                        const char* folder = NULL );


#endif    // BEOS_INTERFACE_WINDOW_H
