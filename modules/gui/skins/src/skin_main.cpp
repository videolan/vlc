/*****************************************************************************
 * skin-main.cpp: skins plugin for VLC
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: skin_main.cpp,v 1.46 2003/07/18 20:06:00 gbazin Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "event.h"
#include "banks.h"
#include "window.h"
#include "theme.h"
#include "../os_theme.h"
#include "themeloader.h"
#include "vlcproc.h"
#include "skin_common.h"
#include "dialogs.h"

#ifdef X11_SKINS
#include <X11/Xlib.h>
#include <Imlib2.h>
#endif

//---------------------------------------------------------------------------
// Interface thread
// It is a global variable because we have C code for the parser, and we
// need to access C++ objects from there
//---------------------------------------------------------------------------
intf_thread_t *g_pIntf;

//---------------------------------------------------------------------------
// Exported interface functions.
//---------------------------------------------------------------------------
#ifdef WIN32
extern "C" __declspec( dllexport )
    int __VLC_SYMBOL( vlc_entry ) ( module_t *p_module );
#endif

//---------------------------------------------------------------------------
// Local prototypes.
//---------------------------------------------------------------------------
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );
static void Run    ( intf_thread_t * );

int  SkinManage( intf_thread_t *p_intf );
void OSRun( intf_thread_t *p_intf );

//---------------------------------------------------------------------------
// Open: initialize interface
//---------------------------------------------------------------------------
static int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    g_pIntf = p_intf;

    // Allocate instance and initialize some members
    p_intf->p_sys = (intf_sys_t *) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    };

    p_intf->pf_run = Run;
    p_intf->p_sys->pf_showdialog = Dialogs::ShowDialog;


    // Suscribe to messages bank
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    // Set no new theme when opening file
    p_intf->p_sys->p_new_theme_file = NULL;

    // Initialize info on playlist
    p_intf->p_sys->i_index        = -1;
    p_intf->p_sys->i_size         = 0;

    p_intf->p_sys->i_close_status = VLC_NOTHING;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_playlist = (playlist_t *)vlc_object_find( p_intf,
        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

#if defined X11_SKINS
    // Initialize X11
    Display *display = XOpenDisplay( NULL );
    p_intf->p_sys->display = display;
    vlc_mutex_init( p_intf, &p_intf->p_sys->xlock );
    // Fake window to receive broadcast events
    Window root = DefaultRootWindow( display );
    p_intf->p_sys->mainWin = XCreateSimpleWindow( display, root, 0, 0, 
                                                  1, 1, 0, 0, 0 );
    XStoreName( display, p_intf->p_sys->mainWin, "VLC Media Player" );

    // Load the vlc icon
    int screen = DefaultScreen( display );
    Screen *screenptr = DefaultScreenOfDisplay( display );
    Visual *visual = DefaultVisualOfScreen( screenptr );
    imlib_context_set_display( display );
    imlib_context_set_visual( visual );
    imlib_context_set_drawable( root );
    imlib_context_set_colormap( DefaultColormap( display, screen ) );
    imlib_context_set_dither( 1 );
    imlib_context_set_blend( 1 );
    Imlib_Image img = imlib_load_image_immediately( DATA_PATH"/vlc32x32.png" );
    if( img == NULL )
    {
        // for developers ;)
        img = imlib_load_image_immediately( "./share/vlc32x32.png" );
    }
    if( img == NULL )
    {
        msg_Err( p_intf, "loading vlc icon failed" );
        p_intf->p_sys->iconPixmap = None;
        p_intf->p_sys->iconMask = None;
    }
    else
    {
        imlib_context_set_image( img );
        imlib_render_pixmaps_for_whole_image( &p_intf->p_sys->iconPixmap,
                                              &p_intf->p_sys->iconMask );
        imlib_free_image();
    }


#elif defined WIN32
    // Interface thread id used to post broadcast messages
    p_intf->p_sys->dwThreadId = GetCurrentThreadId();

    // We dynamically load msimg32.dll to get a pointer to TransparentBlt()
    p_intf->p_sys->h_msimg32_dll = LoadLibrary("msimg32.dll");
    if( !p_intf->p_sys->h_msimg32_dll ||
        !( p_intf->p_sys->TransparentBlt =
           (BOOL (WINAPI*)(HDC,int,int,int,int,HDC,
                           int,int,int,int,unsigned int))
           GetProcAddress( p_intf->p_sys->h_msimg32_dll, "TransparentBlt" ) ) )
    {
        p_intf->p_sys->TransparentBlt = NULL;
        msg_Dbg( p_intf, "Couldn't find TransparentBlt(), "
                 "falling back to BitBlt()" );
    }

    // idem for user32.dll and SetLayeredWindowAttributes()
    p_intf->p_sys->h_user32_dll = LoadLibrary("user32.dll");
    if( !p_intf->p_sys->h_user32_dll ||
        !( p_intf->p_sys->SetLayeredWindowAttributes =
           (BOOL (WINAPI *)(HWND,COLORREF,BYTE,DWORD))
           GetProcAddress( p_intf->p_sys->h_user32_dll,
                           "SetLayeredWindowAttributes" ) ) )
    {
        p_intf->p_sys->SetLayeredWindowAttributes = NULL;
        msg_Dbg( p_intf, "Couldn't find SetLayeredWindowAttributes()" );
    }

#endif

    p_intf->p_sys->p_theme = (Theme *)new OSTheme( p_intf );

    return( 0 );
}

//---------------------------------------------------------------------------
// Close: destroy interface
//---------------------------------------------------------------------------
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    if( p_intf->p_sys->p_playlist )
    {
        vlc_object_release( p_intf->p_sys->p_playlist );
    }

    // Delete theme, it's important to do it correctly
    delete (OSTheme *)p_intf->p_sys->p_theme;

#if defined X11_SKINS
    XDestroyWindow( p_intf->p_sys->display, p_intf->p_sys->mainWin );
    XCloseDisplay( p_intf->p_sys->display );
#endif

    // Unsuscribe to messages bank
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

#ifdef WIN32
    // Unload msimg32.dll and user32.dll
    if( p_intf->p_sys->h_msimg32_dll )
        FreeLibrary( p_intf->p_sys->h_msimg32_dll );
    if( p_intf->p_sys->h_user32_dll )
        FreeLibrary( p_intf->p_sys->h_user32_dll );
#elif defined X11_SKINS
    vlc_mutex_destroy( &p_intf->p_sys->xlock );
#endif

    // Destroy structure
    free( p_intf->p_sys );
}


//---------------------------------------------------------------------------
// Run: main loop
//---------------------------------------------------------------------------
static void Run( intf_thread_t *p_intf )
{

    int a = OSAPI_GetTime();

    // Initialize the dialog boxes
    p_intf->p_sys->p_dialogs = new Dialogs( p_intf );
    if( !p_intf->p_sys->p_dialogs ) return;

    // Load a theme
    char *skin_last = config_GetPsz( p_intf, "skin_last" );
    ThemeLoader *Loader = new ThemeLoader( p_intf );

    if( skin_last == NULL || ! Loader->Load( skin_last ) )
    {
        // Too bad, it failed. Let's try with the default theme
//        if( ! Loader->Load( DEFAULT_SKIN_FILE ) )
#ifdef WIN32
        string default_dir = (string)p_intf->p_libvlc->psz_vlcpath +
                             DIRECTORY_SEPARATOR + "skins" +
                             DIRECTORY_SEPARATOR + "default" +
                             DIRECTORY_SEPARATOR + "theme.xml";
        if( ! Loader->Load( default_dir ) )
        {
            // Last chance: the user can  select a new theme file
#else
        string user_skin = (string)p_intf->p_vlc->psz_homedir +
                              DIRECTORY_SEPARATOR + CONFIG_DIR +
                              DIRECTORY_SEPARATOR + "skins" +
                              DIRECTORY_SEPARATOR + "default" +
                              DIRECTORY_SEPARATOR + "theme.xml";

        string default_skin = (string)DATA_PATH +
                              DIRECTORY_SEPARATOR + "skins" +
                              DIRECTORY_SEPARATOR + "default" +
                              DIRECTORY_SEPARATOR + "theme.xml";
        if( !Loader->Load( user_skin ) && !Loader->Load( default_skin ) )
        {
#endif
#if 0
#if !defined(MODULE_NAME_IS_basic_skins)
            wxMutexGuiEnter();
            wxFileDialog dialog( NULL,
                wxU(_("Open a skin file")), wxT(""), wxT(""),
                wxT("Skin files (*.vlt)|*.vlt|Skin files (*.xml)|*.xml|"
                    "All files|*.*"), wxOPEN );

            if( dialog.ShowModal() == wxID_OK )
            {
                // try to load selected file
                if( ! Loader->Load( (string)dialog.GetPath().mb_str() ) )
                {
                    // He, he, what the hell is he doing ?
                    delete Loader;
                    wxMutexGuiLeave();
                    return;
                }
                wxMutexGuiLeave();
            }
            else
#endif
#endif
            {
                delete Loader;
#if 0
#if !defined(MODULE_NAME_IS_basic_skins)
                wxMutexGuiLeave();
#endif
#endif
                return;
            }
        }
    }

    // Show the theme
    p_intf->p_sys->p_theme->InitTheme();
    p_intf->p_sys->p_theme->ShowTheme();

    delete Loader;

    msg_Dbg( p_intf, "Load theme time : %i ms", OSAPI_GetTime() - a );

    OSAPI_PostMessage( NULL, VLC_INTF_REFRESH, 0, (int)true );

    OSRun( p_intf );

    // clean up the dialog boxes
    delete p_intf->p_sys->p_dialogs;
}

//---------------------------------------------------------------------------
// Module descriptor
//---------------------------------------------------------------------------
#define DEFAULT_SKIN        N_("Last skin actually used")
#define DEFAULT_SKIN_LONG   N_("Last skin actually used")
#define SKIN_CONFIG         N_("Config of last used skin")
#define SKIN_CONFIG_LONG    N_("Config of last used skin")
#define SKIN_TRAY           N_("Show application in system tray")
#define SKIN_TRAY_LONG      N_("Show application in system tray")
#define SKIN_TASKBAR        N_("Show application in taskbar")
#define SKIN_TASKBAR_LONG   N_("Show application in taskbar")

vlc_module_begin();
    add_string( "skin_last", "", NULL, DEFAULT_SKIN, DEFAULT_SKIN_LONG,
                VLC_TRUE );
    add_string( "skin_config", "", NULL, SKIN_CONFIG, SKIN_CONFIG_LONG,
                VLC_TRUE );
    add_bool( "show_in_tray", VLC_FALSE, NULL, SKIN_TRAY, SKIN_TRAY_LONG,
              VLC_FALSE );
    add_bool( "show_in_taskbar", VLC_TRUE, NULL, SKIN_TASKBAR,
              SKIN_TASKBAR_LONG, VLC_FALSE );
    set_description( _("Skinnable Interface") );
    set_capability( "interface", 30 );
    set_callbacks( Open, Close );
    set_program( "svlc" );
vlc_module_end();


//---------------------------------------------------------------------------
// Refresh procedure
//---------------------------------------------------------------------------
int SkinManage( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );

    // Update the input
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)
                    vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    //-------------------------------------------------------------------------
    if( p_intf->p_sys->p_input != NULL && !p_intf->p_sys->p_input->b_die )
    {
        input_thread_t  * p_input = p_intf->p_sys->p_input;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        // Refresh sound volume
        audio_volume_t volume;

        // Get sound volume from VLC
        aout_VolumeGet( p_intf, &volume);

        // Update sliders
        OSAPI_PostMessage( NULL, CTRL_SET_SLIDER,
            (unsigned int)
            p_intf->p_sys->p_theme->EvtBank->Get( "volume_refresh" ),
            (long)( volume * SLIDER_RANGE / AOUT_VOLUME_MAX ) );

        // Refresh slider
        // if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
        if( p_input->stream.b_seekable )
        {
#define p_area p_input->stream.p_selected_area

            // Set value of sliders
            long Value = SLIDER_RANGE *
                p_input->stream.p_selected_area->i_tell /
                p_input->stream.p_selected_area->i_size;

            // Update sliders
            OSAPI_PostMessage( NULL, CTRL_SET_SLIDER, (unsigned int)
                p_intf->p_sys->p_theme->EvtBank->Get( "time" ), (long)Value );

            // Text char * for updating text controls
            char *text = new char[OFFSETTOTIME_MAX_SIZE];

            // Create end time text
            input_OffsetToTime( p_intf->p_sys->p_input, &text[1],
                                p_area->i_size - p_area->i_tell );
            text[0] = '-';
            p_intf->p_sys->p_theme->EvtBank->Get( "left_time" )
                ->PostTextMessage( text );

            // Create time text and update
            input_OffsetToTime( p_intf->p_sys->p_input, text, p_area->i_tell );
            p_intf->p_sys->p_theme->EvtBank->Get( "time" )
                ->PostTextMessage( text );

            // Create total time text
            input_OffsetToTime( p_intf->p_sys->p_input, text, p_area->i_size );
            p_intf->p_sys->p_theme->EvtBank->Get( "total_time" )
                ->PostTextMessage( text );

            // Free memory
            delete[] text;

#undef p_area
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    //-------------------------------------------------------------------------
    vlc_mutex_unlock( &p_intf->change_lock );

    return( VLC_TRUE );
}
