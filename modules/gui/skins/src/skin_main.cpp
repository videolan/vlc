/*****************************************************************************
 * skin-main.cpp: skins plugin for VLC
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: skin_main.cpp,v 1.9 2003/04/14 10:18:25 asmax Exp $
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
#include "os_api.h"
#include "event.h"
#include "dialog.h"
#include "os_dialog.h"
#include "banks.h"
#include "window.h"
#include "theme.h"
#include "os_theme.h"
#include "themeloader.h"
#include "vlcproc.h"
#include "skin_common.h"


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


    // Suscribe to messages bank
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    // Set no new theme when opening file
    p_intf->p_sys->p_new_theme_file = NULL;

    // Initialize Win32 thread
    p_intf->p_sys->i_index        = -1;
    p_intf->p_sys->i_size         = 0;


    p_intf->p_sys->i_close_status = VLC_NOTHING;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_playlist = (playlist_t *)vlc_object_find( p_intf,
        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

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

    // Unsuscribe to messages bank
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );



    // Destroy structure
    free( p_intf->p_sys );
}


//---------------------------------------------------------------------------
// Run: main loop
//---------------------------------------------------------------------------
static void Run( intf_thread_t *p_intf )
{

#if !defined WIN32
/* FIXME: should be elsewhere ? */
    // Initialize GDK
    int    i_args   = 1;
    char  *p_args[] = { "", NULL };
    char **pp_args  = p_args;

    gdk_init( &i_args, &pp_args );
#endif

    int a = OSAPI_GetTime();

    // Load a theme
    char *skin_last = config_GetPsz( p_intf, "skin_last" );
    ThemeLoader *Loader = new ThemeLoader( p_intf );

    if( skin_last == NULL || ! Loader->Load( skin_last ) )
    {
        // Too bad, it failed. Let's try with the default theme
#if 0
        if( ! Loader->Load( DEFAULT_SKIN_FILE ) )
#else
#ifdef WIN32
        string default_dir = (string)p_intf->p_libvlc->psz_vlcpath +
                             DIRECTORY_SEPARATOR + "skins" +
                             DIRECTORY_SEPARATOR + "default" +
                             DIRECTORY_SEPARATOR + "theme.xml";
#else
// FIXME: find VLC directory 
        string default_dir = (string)"./share" +
                             DIRECTORY_SEPARATOR + "skins" +
                             DIRECTORY_SEPARATOR + "default" +
                             DIRECTORY_SEPARATOR + "theme.xml";
#endif
        if( ! Loader->Load( default_dir ) )
#endif
        {
            // Last chance: the user can  select a new theme file

            // Initialize file structure
            OpenFileDialog *OpenFile;
            OpenFile = (OpenFileDialog *)new OSOpenFileDialog( NULL,
                _("Open skin"), false );
            OpenFile->AddFilter( _("Skin files"), "*.vlt" );
            OpenFile->AddFilter( _("Skin files"), "*.xml" );
            OpenFile->AddFilter( _("All files"), "*.*" );

            // Open dialog box
            if( OpenFile->Open() )
            {
                // try to load selected file
                if( ! Loader->Load( OpenFile->FileList.front() ) )
                {
                    // He, he, what the hell is he doing ?
                    delete OpenFile;
                    delete Loader;
                    return;
                }
            }
            else
            {
                delete OpenFile;
                delete Loader;
                return;
            }

            delete OpenFile;
        }
    }

    // Show the theme
    p_intf->p_sys->p_theme->InitTheme();
    p_intf->p_sys->p_theme->ShowTheme();

    delete Loader;

    msg_Err( p_intf, "Load theme time : %i ms", OSAPI_GetTime() - a );

    // Refresh the whole interface
    OSAPI_PostMessage( NULL, VLC_INTF_REFRESH, 0, (int)true );

    // Run interface message loop
    OSRun( p_intf );
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
    add_shortcut( "skins" );
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

    OSAPI_PostMessage( NULL, VLC_INTF_REFRESH, 0, (long)false );

    // Update the log window
    //p_intf->p_sys->p_theme->UpdateLog( p_intf->p_sys->p_sub );

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
        //if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
        //{
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
        //}
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    //-------------------------------------------------------------------------
    vlc_mutex_unlock( &p_intf->change_lock );

    return( VLC_TRUE );
}
//---------------------------------------------------------------------------
