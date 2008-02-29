/*****************************************************************************
 * hotkeys.c: Hotkey handling for vlc
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_osd.h>
#include <vlc_playlist.h>
#include "vlc_keys.h"

#define BUFFER_SIZE 10

#define CHANNELS_NUMBER 4
#define VOLUME_TEXT_CHAN     p_intf->p_sys->p_channels[ 0 ]
#define VOLUME_WIDGET_CHAN   p_intf->p_sys->p_channels[ 1 ]
#define POSITION_TEXT_CHAN   p_intf->p_sys->p_channels[ 2 ]
#define POSITION_WIDGET_CHAN p_intf->p_sys->p_channels[ 3 ]
/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t         change_lock;  /* mutex to keep the callback
                                       * and the main loop from
                                       * stepping on each others
                                       * toes */
    int                 p_keys[ BUFFER_SIZE ]; /* buffer that contains
                                                * keyevents */
    int                 i_size;        /* number of events in buffer */
    int                 p_channels[ CHANNELS_NUMBER ]; /* contains registered
                                                        * channel IDs */
    input_thread_t *    p_input;       /* pointer to input */
    vout_thread_t *     p_vout;        /* pointer to vout object */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );
static int  GetKey  ( intf_thread_t *);
static int  KeyEvent( vlc_object_t *, char const *,
                      vlc_value_t, vlc_value_t, void * );
static int  ActionKeyCB( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static void PlayBookmark( intf_thread_t *, int );
static void SetBookmark ( intf_thread_t *, int );
static void DisplayPosition( intf_thread_t *, vout_thread_t *, input_thread_t * );
static void DisplayVolume  ( intf_thread_t *, vout_thread_t *, audio_volume_t );
static void ClearChannels  ( intf_thread_t *, vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define BOOKMARK1_TEXT    N_("Playlist bookmark 1")
#define BOOKMARK2_TEXT    N_("Playlist bookmark 2")
#define BOOKMARK3_TEXT    N_("Playlist bookmark 3")
#define BOOKMARK4_TEXT    N_("Playlist bookmark 4")
#define BOOKMARK5_TEXT    N_("Playlist bookmark 5")
#define BOOKMARK6_TEXT    N_("Playlist bookmark 6")
#define BOOKMARK7_TEXT    N_("Playlist bookmark 7")
#define BOOKMARK8_TEXT    N_("Playlist bookmark 8")
#define BOOKMARK9_TEXT    N_("Playlist bookmark 9")
#define BOOKMARK10_TEXT   N_("Playlist bookmark 10")
#define BOOKMARK_LONGTEXT N_("Define playlist bookmarks.")

vlc_module_begin();
    set_shortname( _("Hotkeys") );
    set_description( _("Hotkeys management interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    MALLOC_ERR( p_intf->p_sys, intf_sys_t );

    vlc_mutex_init( p_intf, &p_intf->p_sys->change_lock );
    p_intf->p_sys->i_size = 0;
    p_intf->pf_run = Run;

    var_AddCallback( p_intf->p_libvlc, "key-pressed", KeyEvent, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    var_DelCallback( p_intf->p_libvlc, "key-pressed", KeyEvent, p_intf );

    vlc_mutex_destroy( &p_intf->p_sys->change_lock );
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    input_thread_t *p_input = NULL;
    vout_thread_t *p_vout = NULL;
    vout_thread_t *p_last_vout = NULL;
    struct hotkey *p_hotkeys = p_intf->p_libvlc->p_hotkeys;
    vlc_value_t val;
    int i;
    playlist_t *p_playlist = pl_Yield( p_intf );

    /* Initialize hotkey structure */
    for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
    {
        var_Create( p_intf->p_libvlc, p_hotkeys[i].psz_action,
                    VLC_VAR_HOTKEY | VLC_VAR_DOINHERIT );

        var_AddCallback( p_intf->p_libvlc, p_hotkeys[i].psz_action,
                         ActionKeyCB, NULL );
        var_Get( p_intf->p_libvlc, p_hotkeys[i].psz_action, &val );
        var_Set( p_intf->p_libvlc, p_hotkeys[i].psz_action, val );
    }

    for( vlc_bool_t b_quit = VLC_FALSE ; !b_quit; )
    {
        int i_key, i_action;
        int i_times = 0;

        /* Sleep a bit */
        /* msleep( INTF_IDLE_SLEEP ); */

        i_action = 0;
        i_key = GetKey( p_intf );

        /* Special action for mouse event */
        /* FIXME: This should probably be configurable */
        /* FIXME: rework hotkeys handling to allow more than 1 event
         * to trigger one same action */
        switch (i_key & KEY_SPECIAL)
        {
            case KEY_MOUSEWHEELUP:
                i_action = ACTIONID_VOL_UP;
                break;
            case KEY_MOUSEWHEELDOWN:
                i_action = ACTIONID_VOL_DOWN;
                break;
            case KEY_MOUSEWHEELLEFT:
                i_action = ACTIONID_JUMP_BACKWARD_EXTRASHORT;
                break;
            case KEY_MOUSEWHEELRIGHT:
                i_action = ACTIONID_JUMP_FORWARD_EXTRASHORT;
                break;
            default: break;
        }

        /* No mouse action, find action triggered by hotkey */
        if(!i_action)
        {
            for( i = 0; i_key != -1 && p_hotkeys[i].psz_action != NULL; i++ )
            {
                if( p_hotkeys[i].i_key == i_key )
                {
                    i_action = p_hotkeys[i].i_action;
                    i_times  = p_hotkeys[i].i_times;
                    /* times key pressed within max. delta time */
                    p_hotkeys[i].i_times = 0;
                    break;
                }
            }
        }

        if( !i_action )
        {
            b_quit = vlc_object_lock_and_wait( p_intf );
            /* No key pressed, sleep a bit more */
            continue;
        }

        /* Update the input */
        PL_LOCK;
        p_input = p_playlist->p_input;
        if( p_input )
            vlc_object_yield( p_input );
        PL_UNLOCK;

        /* Update the vout */
        p_last_vout = p_vout;
        p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );

        /* Register OSD channels */
        if( p_vout && p_vout != p_last_vout )
        {
            for( i = 0; i < CHANNELS_NUMBER; i++ )
            {
                spu_Control( p_vout->p_spu, SPU_CHANNEL_REGISTER,
                             &p_intf->p_sys->p_channels[ i ] );
            }
        }

        if( i_action == ACTIONID_QUIT )
        {
            if( p_playlist )
                playlist_Stop( p_playlist );
            vlc_object_kill( p_intf->p_libvlc );
            vlc_object_kill( p_intf );
            ClearChannels( p_intf, p_vout );
            vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Quit" ) );
            if( p_vout )
                vlc_object_release( p_vout );
            if( p_input )
                vlc_object_release( p_input );
            continue;
        }
        else if( i_action == ACTIONID_VOL_UP )
        {
            audio_volume_t i_newvol;
            aout_VolumeUp( p_intf, 1, &i_newvol );
            DisplayVolume( p_intf, p_vout, i_newvol );
        }
        else if( i_action == ACTIONID_VOL_DOWN )
        {
            audio_volume_t i_newvol;
            aout_VolumeDown( p_intf, 1, &i_newvol );
            DisplayVolume( p_intf, p_vout, i_newvol );
        }
        else if( i_action == ACTIONID_VOL_MUTE )
        {
            audio_volume_t i_newvol = -1;
            aout_VolumeMute( p_intf, &i_newvol );
            if( p_vout )
            {
                if( i_newvol == 0 )
                {
                    ClearChannels( p_intf, p_vout );
                    vout_OSDIcon( VLC_OBJECT( p_intf ), DEFAULT_CHAN,
                                  OSD_MUTE_ICON );
                }
                else
                {
                    DisplayVolume( p_intf, p_vout, i_newvol );
                }
            }
        }
        else if( i_action == ACTIONID_INTF_SHOW )
            var_SetBool( p_playlist, "intf-show", VLC_TRUE );
        else if( i_action == ACTIONID_INTF_HIDE )
            var_SetBool( p_playlist, "intf-show", VLC_FALSE );
        else if( i_action == ACTIONID_SNAPSHOT )
        {
            if( p_vout ) vout_Control( p_vout, VOUT_SNAPSHOT );
        }
        else if( i_action == ACTIONID_TOGGLE_FULLSCREEN )
        {
            if( p_vout )
            {
                var_Get( p_vout, "fullscreen", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_vout, "fullscreen", val );
            }
            else
            {
                var_Get( p_playlist, "fullscreen", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_playlist, "fullscreen", val );
            }
        }
        else if( i_action == ACTIONID_LEAVE_FULLSCREEN )
        {
            if( p_vout && var_GetBool( p_vout, "fullscreen" ) )
            {
                var_SetBool( p_vout, "fullscreen", VLC_FALSE );
            }
        }
        else if( i_action == ACTIONID_WALLPAPER )
        {
            if( p_vout )
            {
                var_Get( p_vout, "directx-wallpaper", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_vout, "directx-wallpaper", val );
            }
            else
            {
                var_Get( p_playlist, "directx-wallpaper", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_playlist, "directx-wallpaper", val );
            }
        }
        else if( i_action == ACTIONID_LOOP )
        {
            /* Toggle Normal -> Loop -> Repeat -> Normal ... */
            vlc_value_t val2;
            var_Get( p_playlist, "loop", &val );
            var_Get( p_playlist, "repeat", &val2 );
            if( val2.b_bool == VLC_TRUE )
            {
                val.b_bool = VLC_FALSE;
                val2.b_bool = VLC_FALSE;
            }
            else if( val.b_bool == VLC_TRUE )
            {
                val.b_bool = VLC_FALSE;
                val2.b_bool = VLC_TRUE;
            }
            else
            {
                val.b_bool = VLC_TRUE;
            }
            var_Set( p_playlist, "loop", val );
            var_Set( p_playlist, "repeat", val2 );
        }
        else if( i_action == ACTIONID_RANDOM )
        {
            var_Get( p_playlist, "random", &val );
            val.b_bool = !val.b_bool;
            var_Set( p_playlist, "random", val );
        }
        else if( i_action == ACTIONID_ZOOM_QUARTER || 
                 i_action == ACTIONID_ZOOM_HALF ||
                 i_action == ACTIONID_ZOOM_ORIGINAL || 
                 i_action == ACTIONID_ZOOM_DOUBLE )
        {
            if( p_vout )
            {
                if( i_action == ACTIONID_ZOOM_QUARTER )
                    val.f_float = 0.25;
                if( i_action == ACTIONID_ZOOM_HALF )
                    val.f_float = 0.5;
                if( i_action == ACTIONID_ZOOM_ORIGINAL )
                    val.f_float = 1;
                if( i_action == ACTIONID_ZOOM_DOUBLE )
                    val.f_float = 2;
                var_Set( p_vout, "zoom", val );
            }
        }
        else if( i_action == ACTIONID_PLAY_PAUSE )
        {
            val.i_int = PLAYING_S;
            if( p_input )
            {
                ClearChannels( p_intf, p_vout );

                var_Get( p_input, "state", &val );
                if( val.i_int != PAUSE_S )
                {
                    vout_OSDIcon( VLC_OBJECT( p_intf ), DEFAULT_CHAN,
                                  OSD_PAUSE_ICON );
                    val.i_int = PAUSE_S;
                }
                else
                {
                    vout_OSDIcon( VLC_OBJECT( p_intf ), DEFAULT_CHAN,
                                  OSD_PLAY_ICON );
                    val.i_int = PLAYING_S;
                }
                var_Set( p_input, "state", val );
            }
            else
            {
                playlist_Play( p_playlist );
            }
        }
        else if( p_input )
        {
            /* FIXME --fenrir
             * How to get a valid value ?
             * That's not that easy with some special stream
             */
            vlc_bool_t b_seekable = VLC_TRUE;
            int i_interval =0;

            if( i_action == ACTIONID_PAUSE )
            {
                var_Get( p_input, "state", &val );
                if( val.i_int != PAUSE_S )
                {
                    ClearChannels( p_intf, p_vout );
                    vout_OSDIcon( VLC_OBJECT( p_intf ), DEFAULT_CHAN,
                                  OSD_PAUSE_ICON );
                    val.i_int = PAUSE_S;
                    var_Set( p_input, "state", val );
                }
            }
            else if( i_action == ACTIONID_JUMP_BACKWARD_EXTRASHORT
                     && b_seekable )
            {
#define SET_TIME( a, b ) \
    i_interval = config_GetInt( p_input, a "-jump-size" ); \
    if( i_interval > 0 ) { \
        val.i_time = ( (mtime_t)(i_interval * b) * 1000000L \
                       * ((mtime_t)(1 << i_times))); \
        var_Set( p_input, "time-offset", val ); \
        DisplayPosition( p_intf, p_vout, p_input ); \
    }
                SET_TIME( "extrashort", -1 );
            }
            else if( i_action == ACTIONID_JUMP_FORWARD_EXTRASHORT && b_seekable )
            {
                SET_TIME( "extrashort", 1 );
            }
            else if( i_action == ACTIONID_JUMP_BACKWARD_SHORT && b_seekable )
            {
                SET_TIME( "short", -1 );
            }
            else if( i_action == ACTIONID_JUMP_FORWARD_SHORT && b_seekable )
            {
                SET_TIME( "short", 1 );
            }
            else if( i_action == ACTIONID_JUMP_BACKWARD_MEDIUM && b_seekable )
            {
                SET_TIME( "medium", -1 );
            }
            else if( i_action == ACTIONID_JUMP_FORWARD_MEDIUM && b_seekable )
            {
                SET_TIME( "medium", 1 );
            }
            else if( i_action == ACTIONID_JUMP_BACKWARD_LONG && b_seekable )
            {
                SET_TIME( "long", -1 );
            }
            else if( i_action == ACTIONID_JUMP_FORWARD_LONG && b_seekable )
            {
                SET_TIME( "long", 1 );
#undef SET_TIME
            }
            else if( i_action == ACTIONID_AUDIO_TRACK )
            {
                vlc_value_t val, list, list2;
                int i_count, i;
                var_Get( p_input, "audio-es", &val );
                var_Change( p_input, "audio-es", VLC_VAR_GETCHOICES,
                            &list, &list2 );
                i_count = list.p_list->i_count;
                if( i_count <= 1 )
                {
                    continue;
                }
                for( i = 0; i < i_count; i++ )
                {
                    if( val.i_int == list.p_list->p_values[i].i_int )
                    {
                        break;
                    }
                }
                /* value of audio-es was not in choices list */
                if( i == i_count )
                {
                    msg_Warn( p_input,
                              "invalid current audio track, selecting 0" );
                    var_Set( p_input, "audio-es",
                             list.p_list->p_values[0] );
                    i = 0;
                }
                else if( i == i_count - 1 )
                {
                    var_Set( p_input, "audio-es",
                             list.p_list->p_values[1] );
                    i = 1;
                }
                else
                {
                    var_Set( p_input, "audio-es",
                             list.p_list->p_values[i+1] );
                    i++;
                }
                vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                 _("Audio track: %s"),
                                 list2.p_list->p_values[i].psz_string );
            }
            else if( i_action == ACTIONID_SUBTITLE_TRACK )
            {
                vlc_value_t val, list, list2;
                int i_count, i;
                var_Get( p_input, "spu-es", &val );

                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &list, &list2 );
                i_count = list.p_list->i_count;
                if( i_count <= 1 )
                {
                    vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                     _("Subtitle track: %s"), _("N/A") );
                    continue;
                }
                for( i = 0; i < i_count; i++ )
                {
                    if( val.i_int == list.p_list->p_values[i].i_int )
                    {
                        break;
                    }
                }
                /* value of spu-es was not in choices list */
                if( i == i_count )
                {
                    msg_Warn( p_input,
                              "invalid current subtitle track, selecting 0" );
                    var_Set( p_input, "spu-es", list.p_list->p_values[0] );
                    i = 0;
                }
                else if( i == i_count - 1 )
                {
                    var_Set( p_input, "spu-es", list.p_list->p_values[0] );
                    i = 0;
                }
                else
                {
                    var_Set( p_input, "spu-es", list.p_list->p_values[i+1] );
                    i = i + 1;
                }
                vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                 _("Subtitle track: %s"),
                                 list2.p_list->p_values[i].psz_string );
            }
            else if( i_action == ACTIONID_ASPECT_RATIO && p_vout )
            {
                vlc_value_t val={0}, val_list, text_list;
                var_Get( p_vout, "aspect-ratio", &val );
                if( var_Change( p_vout, "aspect-ratio", VLC_VAR_GETLIST,
                                &val_list, &text_list ) >= 0 )
                {
                    int i;
                    for( i = 0; i < val_list.p_list->i_count; i++ )
                    {
                        if( !strcmp( val_list.p_list->p_values[i].psz_string,
                                     val.psz_string ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == val_list.p_list->i_count ) i = 0;
                    var_SetString( p_vout, "aspect-ratio",
                                   val_list.p_list->p_values[i].psz_string );
                    vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                     _("Aspect ratio: %s"),
                                     text_list.p_list->p_values[i].psz_string );
                }
                free( val.psz_string );
            }
            else if( i_action == ACTIONID_CROP && p_vout )
            {
                vlc_value_t val={0}, val_list, text_list;
                var_Get( p_vout, "crop", &val );
                if( var_Change( p_vout, "crop", VLC_VAR_GETLIST,
                                &val_list, &text_list ) >= 0 )
                {
                    int i;
                    for( i = 0; i < val_list.p_list->i_count; i++ )
                    {
                        if( !strcmp( val_list.p_list->p_values[i].psz_string,
                                     val.psz_string ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == val_list.p_list->i_count ) i = 0;
                    var_SetString( p_vout, "crop",
                                   val_list.p_list->p_values[i].psz_string );
                    vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                     _("Crop: %s"),
                                     text_list.p_list->p_values[i].psz_string );
                }
                free( val.psz_string );
            }
            else if( i_action == ACTIONID_DEINTERLACE && p_vout )
            {
                vlc_value_t val={0}, val_list, text_list;
                var_Get( p_vout, "deinterlace", &val );
                if( var_Change( p_vout, "deinterlace", VLC_VAR_GETLIST,
                                &val_list, &text_list ) >= 0 )
                {
                    int i;
                    for( i = 0; i < val_list.p_list->i_count; i++ )
                    {
                        if( !strcmp( val_list.p_list->p_values[i].psz_string,
                                     val.psz_string ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == val_list.p_list->i_count ) i = 0;
                    var_SetString( p_vout, "deinterlace",
                                   val_list.p_list->p_values[i].psz_string );
                    vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                     _("Deinterlace mode: %s"),
                                     text_list.p_list->p_values[i].psz_string );
                }
                free( val.psz_string );
            }
            else if( ( i_action == ACTIONID_ZOOM || i_action == ACTIONID_UNZOOM ) && p_vout )
            {
                vlc_value_t val={0}, val_list, text_list;
                var_Get( p_vout, "zoom", &val );
                if( var_Change( p_vout, "zoom", VLC_VAR_GETLIST,
                                &val_list, &text_list ) >= 0 )
                {
                    int i;
                    for( i = 0; i < val_list.p_list->i_count; i++ )
                    {
                        if( val_list.p_list->p_values[i].f_float
                           == val.f_float )
                        {
                            if( i_action == ACTIONID_ZOOM )
                                i++;
                            else /* ACTIONID_UNZOOM */
                                i--;
                            break;
                        }
                    }
                    if( i == val_list.p_list->i_count ) i = 0;
                    if( i == -1 ) i = val_list.p_list->i_count-1;
                    var_SetFloat( p_vout, "zoom",
                                  val_list.p_list->p_values[i].f_float );
                    vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                     _("Zoom mode: %s"),
                                text_list.p_list->p_values[i].var.psz_name );
                }
            }
            else if( i_action == ACTIONID_CROP_TOP && p_vout )
                var_IncInteger( p_vout, "crop-top" );
            else if( i_action == ACTIONID_UNCROP_TOP && p_vout )
                var_DecInteger( p_vout, "crop-top" );
            else if( i_action == ACTIONID_CROP_BOTTOM && p_vout )
                var_IncInteger( p_vout, "crop-bottom" );
            else if( i_action == ACTIONID_UNCROP_BOTTOM && p_vout )
                 var_DecInteger( p_vout, "crop-bottom" );
            else if( i_action == ACTIONID_CROP_LEFT && p_vout )
                 var_IncInteger( p_vout, "crop-left" );
            else if( i_action == ACTIONID_UNCROP_LEFT && p_vout )
                 var_DecInteger( p_vout, "crop-left" );
            else if( i_action == ACTIONID_CROP_RIGHT && p_vout )
                 var_IncInteger( p_vout, "crop-right" );
            else if( i_action == ACTIONID_UNCROP_RIGHT && p_vout )
                 var_DecInteger( p_vout, "crop-right" );

            else if( i_action == ACTIONID_NEXT )
            {
                vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN, _("Next") );
                playlist_Next( p_playlist );
            }
            else if( i_action == ACTIONID_PREV )
            {
                vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                 _("Previous") );
                playlist_Prev( p_playlist );
            }
            else if( i_action == ACTIONID_STOP )
            {
                playlist_Stop( p_playlist );
            }
            else if( i_action == ACTIONID_FASTER )
            {
                var_SetVoid( p_input, "rate-faster" );
                vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                 _("Faster") );
            }
            else if( i_action == ACTIONID_SLOWER )
            {
                var_SetVoid( p_input, "rate-slower" );
                vout_OSDMessage( VLC_OBJECT(p_input), DEFAULT_CHAN,
                                 _("Slower") );
            }
            else if( i_action == ACTIONID_POSITION && b_seekable )
            {
                DisplayPosition( p_intf, p_vout, p_input );
            }
            else if( i_action >= ACTIONID_PLAY_BOOKMARK1 &&
                     i_action <= ACTIONID_PLAY_BOOKMARK10 )
            {
                PlayBookmark( p_intf, i_action - ACTIONID_PLAY_BOOKMARK1 + 1 );
            }
            else if( i_action >= ACTIONID_SET_BOOKMARK1 &&
                     i_action <= ACTIONID_SET_BOOKMARK10 )
            {
                SetBookmark( p_intf, i_action - ACTIONID_SET_BOOKMARK1 + 1 );
            }
            /* Only makes sense with DVD */
            else if( i_action == ACTIONID_TITLE_PREV )
                var_SetVoid( p_input, "prev-title" );
            else if( i_action == ACTIONID_TITLE_NEXT )
                var_SetVoid( p_input, "next-title" );
            else if( i_action == ACTIONID_CHAPTER_PREV )
                var_SetVoid( p_input, "prev-chapter" );
            else if( i_action == ACTIONID_CHAPTER_NEXT )
                var_SetVoid( p_input, "next-chapter" );
            else if( i_action == ACTIONID_DISC_MENU )
                var_SetInteger( p_input, "title  0", 2 );

            else if( i_action == ACTIONID_SUBDELAY_DOWN )
            {
                int64_t i_delay = var_GetTime( p_input, "spu-delay" );
                i_delay -= 50000;    /* 50 ms */
                var_SetTime( p_input, "spu-delay", i_delay );
                ClearChannels( p_intf, p_vout );
                vout_OSDMessage( p_intf, DEFAULT_CHAN,
                                 _( "Subtitle delay %i ms" ),
                                 (int)(i_delay/1000) );
            }
            else if( i_action == ACTIONID_SUBDELAY_UP )
            {
                int64_t i_delay = var_GetTime( p_input, "spu-delay" );
                i_delay += 50000;    /* 50 ms */
                var_SetTime( p_input, "spu-delay", i_delay );
                ClearChannels( p_intf, p_vout );
                vout_OSDMessage( p_intf, DEFAULT_CHAN,
                                _( "Subtitle delay %i ms" ),
                                 (int)(i_delay/1000) );
            }
            else if( i_action == ACTIONID_AUDIODELAY_DOWN )
            {
                int64_t i_delay = var_GetTime( p_input, "audio-delay" );
                i_delay -= 50000;    /* 50 ms */
                var_SetTime( p_input, "audio-delay", i_delay );
                ClearChannels( p_intf, p_vout );
                vout_OSDMessage( p_intf, DEFAULT_CHAN,
                                _( "Audio delay %i ms" ),
                                 (int)(i_delay/1000) );
            }
            else if( i_action == ACTIONID_AUDIODELAY_UP )
            {
                int64_t i_delay = var_GetTime( p_input, "audio-delay" );
                i_delay += 50000;    /* 50 ms */
                var_SetTime( p_input, "audio-delay", i_delay );
                ClearChannels( p_intf, p_vout );
                vout_OSDMessage( p_intf, DEFAULT_CHAN,
                                _( "Audio delay %i ms" ),
                                 (int)(i_delay/1000) );
            }
            else if( i_action == ACTIONID_PLAY )
            {
                var_Get( p_input, "rate", &val );
                if( val.i_int != INPUT_RATE_DEFAULT )
                {
                    /* Return to normal speed */
                    var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT );
                }
                else
                {
                    ClearChannels( p_intf, p_vout );
                    vout_OSDIcon( VLC_OBJECT( p_intf ), DEFAULT_CHAN,
                                  OSD_PLAY_ICON );
                    playlist_Play( p_playlist );
                }
            }
            else if( i_action == ACTIONID_MENU_ON )
            {
                osd_MenuShow( VLC_OBJECT(p_intf) );
            }
            else if( i_action == ACTIONID_MENU_OFF )
            {
                osd_MenuHide( VLC_OBJECT(p_intf) );
            }
            else if( i_action == ACTIONID_MENU_LEFT )
            {
                osd_MenuPrev( VLC_OBJECT(p_intf) );
            }
            else if( i_action == ACTIONID_MENU_RIGHT )
            {
                osd_MenuNext( VLC_OBJECT(p_intf) );
            }
            else if( i_action == ACTIONID_MENU_UP )
            {
                osd_MenuUp( VLC_OBJECT(p_intf) );
            }
            else if( i_action == ACTIONID_MENU_DOWN )
            {
                osd_MenuDown( VLC_OBJECT(p_intf) );
            }
            else if( i_action == ACTIONID_MENU_SELECT )
            {
                osd_MenuActivate( VLC_OBJECT(p_intf) );
            }
        }
        if( p_vout )
            vlc_object_release( p_vout );
        if( p_input )
            vlc_object_release( p_input );
    }
    pl_Release( p_intf );
}

static int GetKey( intf_thread_t *p_intf)
{
    vlc_mutex_lock( &p_intf->p_sys->change_lock );
    if ( p_intf->p_sys->i_size == 0 )
    {
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        return -1;
    }
    else
    {
        int i_return = p_intf->p_sys->p_keys[ 0 ];
        int i;
        p_intf->p_sys->i_size--;
        for ( i = 0; i < BUFFER_SIZE - 1; i++)
        {
            p_intf->p_sys->p_keys[ i ] = p_intf->p_sys->p_keys[ i + 1 ];
        }
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        return i_return;
    }
}

/*****************************************************************************
 * KeyEvent: callback for keyboard events
 *****************************************************************************/
static int KeyEvent( vlc_object_t *p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    if ( !newval.i_int )
    {
        msg_Warn( p_this, "Received invalid key event %d", newval.i_int );
        return VLC_EGENERIC;
    }
    vlc_mutex_lock( &p_intf->p_sys->change_lock );
    if ( p_intf->p_sys->i_size == BUFFER_SIZE )
    {
        msg_Warn( p_intf, "event buffer full, dropping keypress" );
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        return VLC_EGENERIC;
    }
    else
    {
        p_intf->p_sys->p_keys[ p_intf->p_sys->i_size ] = newval.i_int;
        p_intf->p_sys->i_size++;
    }
    vlc_mutex_lock( &p_intf->object_lock );
    vlc_cond_signal( &p_intf->object_wait );
    vlc_mutex_unlock( &p_intf->object_lock );
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );

    return VLC_SUCCESS;
}

static int ActionKeyCB( vlc_object_t *p_this, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    libvlc_int_t *p_libvlc = (libvlc_int_t *)p_this;
    struct hotkey *p_hotkeys = p_libvlc->p_hotkeys;
    mtime_t i_date;
    int i;

    for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
    {
        if( !strcmp( p_hotkeys[i].psz_action, psz_var ) )
        {
            p_hotkeys[i].i_key = newval.i_int;
            /* do hotkey accounting */
            i_date = mdate();
            if( (p_hotkeys[i].i_delta_date > 0) &&
                (p_hotkeys[i].i_delta_date <= (i_date - p_hotkeys[i].i_last_date) ) )
                p_hotkeys[i].i_times = 0;
            else
                p_hotkeys[i].i_times++;
            p_hotkeys[i].i_last_date = i_date;
        }
    }

    return VLC_SUCCESS;
}

static void PlayBookmark( intf_thread_t *p_intf, int i_num )
{
    vlc_value_t val;
    char psz_bookmark_name[11];
    playlist_t *p_playlist = pl_Yield( p_intf );

    sprintf( psz_bookmark_name, "bookmark%i", i_num );
    var_Create( p_intf, psz_bookmark_name, VLC_VAR_STRING|VLC_VAR_DOINHERIT );
    var_Get( p_intf, psz_bookmark_name, &val );

    char *psz_bookmark = strdup( val.psz_string );
    PL_LOCK;
    FOREACH_ARRAY( playlist_item_t *p_item, p_playlist->items )
        char *psz_uri = input_item_GetURI( p_item->p_input );
        if( !strcmp( psz_bookmark, psz_uri ) )
        {
            free( psz_uri );
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, VLC_TRUE,
                              NULL, p_item );
            break;
        }
        else
            free( psz_uri );
    FOREACH_END();
    PL_UNLOCK;
    vlc_object_release( p_playlist );
}

static void SetBookmark( intf_thread_t *p_intf, int i_num )
{
    playlist_t *p_playlist = pl_Yield( p_intf );
    char psz_bookmark_name[11];
    sprintf( psz_bookmark_name, "bookmark%i", i_num );
    var_Create( p_intf, psz_bookmark_name,
                VLC_VAR_STRING|VLC_VAR_DOINHERIT );
    if( p_playlist->status.p_item )
    {
        char *psz_uri = input_item_GetURI( p_playlist->status.p_item->p_input );
        config_PutPsz( p_intf, psz_bookmark_name, psz_uri);
        msg_Info( p_intf, "setting playlist bookmark %i to %s", i_num, psz_uri);
        free( psz_uri );
        config_SaveConfigFile( p_intf, "hotkeys" );
    }
    pl_Release( p_intf );
}

static void DisplayPosition( intf_thread_t *p_intf, vout_thread_t *p_vout,
                             input_thread_t *p_input )
{
    char psz_duration[MSTRTIME_MAX_SIZE];
    char psz_time[MSTRTIME_MAX_SIZE];
    vlc_value_t time, pos;
    mtime_t i_seconds;

    if( p_vout == NULL ) return;

    ClearChannels( p_intf, p_vout );

    var_Get( p_input, "time", &time );
    i_seconds = time.i_time / 1000000;
    secstotimestr ( psz_time, i_seconds );

    var_Get( p_input, "length", &time );
    if( time.i_time > 0 )
    {
        secstotimestr( psz_duration, time.i_time / 1000000 );
        vout_OSDMessage( p_input, POSITION_TEXT_CHAN, (char *) "%s / %s",
                         psz_time, psz_duration );
    }
    else if( i_seconds > 0 )
    {
        vout_OSDMessage( p_input, POSITION_TEXT_CHAN, psz_time );
    }

    if( !p_vout->p_parent_intf || p_vout->b_fullscreen )
    {
        var_Get( p_input, "position", &pos );
        vout_OSDSlider( VLC_OBJECT( p_input ), POSITION_WIDGET_CHAN,
                        pos.f_float * 100, OSD_HOR_SLIDER );
    }
}

static void DisplayVolume( intf_thread_t *p_intf, vout_thread_t *p_vout,
                           audio_volume_t i_vol )
{
    if( p_vout == NULL )
    {
        return;
    }
    ClearChannels( p_intf, p_vout );

    if( !p_vout->p_parent_intf || p_vout->b_fullscreen )
    {
        vout_OSDSlider( VLC_OBJECT( p_vout ), VOLUME_WIDGET_CHAN,
            i_vol*100/AOUT_VOLUME_MAX, OSD_VERT_SLIDER );
    }
    else
    {
        vout_OSDMessage( p_vout, VOLUME_TEXT_CHAN, _( "Volume %d%%" ),
                         i_vol*400/AOUT_VOLUME_MAX );
    }
}

static void ClearChannels( intf_thread_t *p_intf, vout_thread_t *p_vout )
{
    int i;

    if( p_vout )
    {
        spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR, DEFAULT_CHAN );
        for( i = 0; i < CHANNELS_NUMBER; i++ )
        {
            spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR,
                         p_intf->p_sys->p_channels[ i ] );
        }
    }
}
