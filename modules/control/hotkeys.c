/*****************************************************************************
 * hotkeys.c: Hotkey handling for vlc
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>
#include <vlc_playlist.h>
#include <vlc_keys.h>
#include "math.h"

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
    vout_thread_t      *p_last_vout;
    int                 p_channels[ CHANNELS_NUMBER ]; /* contains registered
                                                        * channel IDs */
    int                 i_mousewheel_mode;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static int  ActionEvent( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int  SpecialKeyEvent( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );
static void PlayBookmark( intf_thread_t *, int );
static void SetBookmark ( intf_thread_t *, int );
static void DisplayPosition( intf_thread_t *, vout_thread_t *, input_thread_t * );
static void DisplayVolume( intf_thread_t *, vout_thread_t *, float );
static void DisplayRate ( vout_thread_t *, float );
static float AdjustRateFine( vlc_object_t *, const int );
static void ClearChannels  ( intf_thread_t *, vout_thread_t * );

#define DisplayMessage(vout, ch, fmt, ...) \
    do { if(vout) vout_OSDMessage(vout, ch, fmt, __VA_ARGS__); } while(0)
#define DisplayIcon(vout, icon) \
    do { if(vout) vout_OSDIcon(vout, SPU_DEFAULT_CHANNEL, icon); } while(0)

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname( N_("Hotkeys") )
    set_description( N_("Hotkeys management interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_HOTKEYS )

vlc_module_end ()

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;
    p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_intf->p_sys = p_sys;

    p_sys->p_last_vout = NULL;
    p_intf->p_sys->i_mousewheel_mode =
        var_InheritInteger( p_intf, "hotkeys-mousewheel-mode" );

    var_AddCallback( p_intf->p_libvlc, "key-pressed", SpecialKeyEvent, p_intf );
    var_AddCallback( p_intf->p_libvlc, "key-action", ActionEvent, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    var_DelCallback( p_intf->p_libvlc, "key-action", ActionEvent, p_intf );
    var_DelCallback( p_intf->p_libvlc, "key-pressed", SpecialKeyEvent, p_intf );

    /* Destroy structure */
    free( p_sys );
}

static int PutAction( intf_thread_t *p_intf, int i_action )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get( p_intf );

    /* Update the input */
    input_thread_t *p_input = playlist_CurrentInput( p_playlist );

    /* Update the vout */
    vout_thread_t *p_vout = p_input ? input_GetVout( p_input ) : NULL;

    /* Register OSD channels */
    /* FIXME: this check can fail if the new vout is reallocated at the same
     * address as the old one... We should rather listen to vout events.
     * Alternatively, we should keep a reference to the vout thread. */
    if( p_vout && p_vout != p_sys->p_last_vout )
        for( unsigned i = 0; i < CHANNELS_NUMBER; i++ )
            p_intf->p_sys->p_channels[i] = vout_RegisterSubpictureChannel( p_vout );
    p_sys->p_last_vout = p_vout;

    /* Quit */
    switch( i_action )
    {
        /* Libvlc / interface actions */
        case ACTIONID_QUIT:
            libvlc_Quit( p_intf->p_libvlc );

            ClearChannels( p_intf, p_vout );
            DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Quit" ) );
            break;

        case ACTIONID_INTF_TOGGLE_FSC:
        case ACTIONID_INTF_HIDE:
            var_TriggerCallback( p_intf->p_libvlc, "intf-toggle-fscontrol" );
            break;
        case ACTIONID_INTF_BOSS:
            var_TriggerCallback( p_intf->p_libvlc, "intf-boss" );
            break;

        /* Playlist actions (including audio) */
        case ACTIONID_LOOP:
            /* Toggle Normal -> Loop -> Repeat -> Normal ... */
            if( var_GetBool( p_playlist, "repeat" ) )
                var_SetBool( p_playlist, "repeat", false );
            else
            if( var_GetBool( p_playlist, "loop" ) )
            { /* FIXME: this is not atomic, we should use a real tristate */
                var_SetBool( p_playlist, "loop", false );
                var_SetBool( p_playlist, "repeat", true );
            }
            else
                var_SetBool( p_playlist, "loop", true );
            break;

        case ACTIONID_RANDOM:
            var_ToggleBool( p_playlist, "random" );
            break;

        case ACTIONID_NEXT:
            DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Next") );
            playlist_Next( p_playlist );
            break;
        case ACTIONID_PREV:
            DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Previous") );
            playlist_Prev( p_playlist );
            break;

        case ACTIONID_STOP:
            playlist_Stop( p_playlist );
            break;

        case ACTIONID_RATE_NORMAL:
            var_SetFloat( p_playlist, "rate", 1.f );
            DisplayRate( p_vout, 1.f );
            break;
        case ACTIONID_FASTER:
            var_TriggerCallback( p_playlist, "rate-faster" );
            DisplayRate( p_vout, var_GetFloat( p_playlist, "rate" ) );
            break;
        case ACTIONID_SLOWER:
            var_TriggerCallback( p_playlist, "rate-slower" );
            DisplayRate( p_vout, var_GetFloat( p_playlist, "rate" ) );
            break;
        case ACTIONID_RATE_FASTER_FINE:
        case ACTIONID_RATE_SLOWER_FINE:
        {
            const int i_dir = i_action == ACTIONID_RATE_FASTER_FINE ? 1 : -1;
            float rate = AdjustRateFine( VLC_OBJECT(p_playlist), i_dir );

            var_SetFloat( p_playlist, "rate", rate );
            DisplayRate( p_vout, rate );
            break;
        }

        case ACTIONID_PLAY_BOOKMARK1:
        case ACTIONID_PLAY_BOOKMARK2:
        case ACTIONID_PLAY_BOOKMARK3:
        case ACTIONID_PLAY_BOOKMARK4:
        case ACTIONID_PLAY_BOOKMARK5:
        case ACTIONID_PLAY_BOOKMARK6:
        case ACTIONID_PLAY_BOOKMARK7:
        case ACTIONID_PLAY_BOOKMARK8:
        case ACTIONID_PLAY_BOOKMARK9:
        case ACTIONID_PLAY_BOOKMARK10:
            PlayBookmark( p_intf, i_action - ACTIONID_PLAY_BOOKMARK1 + 1 );
            break;

        case ACTIONID_SET_BOOKMARK1:
        case ACTIONID_SET_BOOKMARK2:
        case ACTIONID_SET_BOOKMARK3:
        case ACTIONID_SET_BOOKMARK4:
        case ACTIONID_SET_BOOKMARK5:
        case ACTIONID_SET_BOOKMARK6:
        case ACTIONID_SET_BOOKMARK7:
        case ACTIONID_SET_BOOKMARK8:
        case ACTIONID_SET_BOOKMARK9:
        case ACTIONID_SET_BOOKMARK10:
            SetBookmark( p_intf, i_action - ACTIONID_SET_BOOKMARK1 + 1 );
            break;

        case ACTIONID_VOL_UP:
        {
            float vol;
            if( playlist_VolumeUp( p_playlist, 1, &vol ) == 0 )
                DisplayVolume( p_intf, p_vout, vol );
            break;
        }
        case ACTIONID_VOL_DOWN:
        {
            float vol;
            if( playlist_VolumeDown( p_playlist, 1, &vol ) == 0 )
                DisplayVolume( p_intf, p_vout, vol );
            break;
        }
        case ACTIONID_VOL_MUTE:
            if( playlist_MuteToggle( p_playlist ) == 0 )
            {
                float vol = playlist_VolumeGet( p_playlist );
                if( playlist_MuteGet( p_playlist ) > 0 || vol == 0.f )
                {
                    ClearChannels( p_intf, p_vout );
                    DisplayIcon( p_vout, OSD_MUTE_ICON );
                }
                else
                    DisplayVolume( p_intf, p_vout, vol );
            }
            break;

        case ACTIONID_AUDIODEVICE_CYCLE:
        {
            audio_output_t *p_aout = playlist_GetAout( p_playlist );
            if( p_aout == NULL )
                break;

            char **ids, **names;
            int n = aout_DevicesList( p_aout, &ids, &names );
            if( n == -1 )
                break;

            char *dev = aout_DeviceGet( p_aout );
            const char *devstr = (dev != NULL) ? dev : "";

            int idx = 0;
            for( int i = 0; i < n; i++ )
            {
                if( !strcmp(devstr, ids[i]) )
                    idx = (i + 1) % n;
            }
            free( dev );

            if( !aout_DeviceSet( p_aout, ids[idx] ) )
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                _("Audio Device: %s"), names[idx] );
            vlc_object_release( p_aout );

            for( int i = 0; i < n; i++ )
            {
                free( ids[i] );
                free( names[i] );
            }
            free( ids );
            free( names );
            break;
        }

        /* Playlist + input actions */
        case ACTIONID_PLAY_PAUSE:
            if( p_input )
            {
                ClearChannels( p_intf, p_vout );

                int state = var_GetInteger( p_input, "state" );
                DisplayIcon( p_vout, state != PAUSE_S ? OSD_PAUSE_ICON : OSD_PLAY_ICON );
                playlist_Pause( p_playlist );
            }
            else
                playlist_Play( p_playlist );
            break;

        case ACTIONID_PLAY:
            if( p_input && var_GetFloat( p_input, "rate" ) != 1. )
                /* Return to normal speed */
                var_SetFloat( p_input, "rate", 1. );
            else
            {
                ClearChannels( p_intf, p_vout );
                DisplayIcon( p_vout, OSD_PLAY_ICON );
                playlist_Play( p_playlist );
            }
            break;

        /* Playlist + video output actions */
        case ACTIONID_WALLPAPER:
        {   /* FIXME: this is invalid if not using DirectX output!!! */
            vlc_object_t *obj = p_vout ? VLC_OBJECT(p_vout)
                                       : VLC_OBJECT(p_playlist);
            var_ToggleBool( obj, "video-wallpaper" );
            break;
        }

        /* Input actions */
        case ACTIONID_PAUSE:
            if( p_input && var_GetInteger( p_input, "state" ) != PAUSE_S )
            {
                ClearChannels( p_intf, p_vout );
                DisplayIcon( p_vout, OSD_PAUSE_ICON );
                var_SetInteger( p_input, "state", PAUSE_S );
            }
            break;

        case ACTIONID_RECORD:
            if( p_input && var_GetBool( p_input, "can-record" ) )
            {
                const bool on = var_ToggleBool( p_input, "record" );
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s",
                    vlc_gettext(on ? N_("Recording") : N_("Recording done")) );
            }
            break;

        case ACTIONID_FRAME_NEXT:
            if( p_input )
            {
                var_TriggerCallback( p_input, "frame-next" );
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                "%s", _("Next frame") );
            }
            break;

        case ACTIONID_SUBDELAY_DOWN:
        case ACTIONID_SUBDELAY_UP:
        {
            int diff = (i_action == ACTIONID_SUBDELAY_UP) ? 50000 : -50000;
            if( p_input )
            {
                int64_t i_delay = var_GetTime( p_input, "spu-delay" ) + diff;

                var_SetTime( p_input, "spu-delay", i_delay );
                ClearChannels( p_intf, p_vout );
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                _( "Subtitle delay %i ms" ),
                                (int)(i_delay/1000) );
            }
            break;
        }
        case ACTIONID_AUDIODELAY_DOWN:
        case ACTIONID_AUDIODELAY_UP:
        {
            int diff = (i_action == ACTIONID_AUDIODELAY_UP) ? 50000 : -50000;
            if( p_input )
            {
                int64_t i_delay = var_GetTime( p_input, "audio-delay" ) + diff;

                var_SetTime( p_input, "audio-delay", i_delay );
                ClearChannels( p_intf, p_vout );
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                _( "Audio delay %i ms" ),
                                 (int)(i_delay/1000) );
            }
            break;
        }

        case ACTIONID_AUDIO_TRACK:
            if( p_input )
            {
                vlc_value_t val, list, list2;
                int i_count, i;
                var_Get( p_input, "audio-es", &val );
                var_Change( p_input, "audio-es", VLC_VAR_GETCHOICES,
                            &list, &list2 );
                i_count = list.p_list->i_count;
                if( i_count > 1 )
                {
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
                        i = 0;
                    }
                    else if( i == i_count - 1 )
                        i = 1;
                    else
                        i++;
                    var_Set( p_input, "audio-es", list.p_list->p_values[i] );
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    _("Audio track: %s"),
                                    list2.p_list->p_values[i].psz_string );
                }
                var_FreeList( &list, &list2 );
            }
            break;
        case ACTIONID_SUBTITLE_TRACK:
            if( p_input )
            {
                vlc_value_t val, list, list2;
                int i_count, i;
                var_Get( p_input, "spu-es", &val );

                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &list, &list2 );
                i_count = list.p_list->i_count;
                if( i_count <= 1 )
                {
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    _("Subtitle track: %s"), _("N/A") );
                    var_FreeList( &list, &list2 );
                    break;
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
                    i = 0;
                }
                else if( i == i_count - 1 )
                    i = 0;
                else
                    i++;
                var_Set( p_input, "spu-es", list.p_list->p_values[i] );
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                _("Subtitle track: %s"),
                                list2.p_list->p_values[i].psz_string );
                var_FreeList( &list, &list2 );
            }
            break;
        case ACTIONID_PROGRAM_SID:
            if( p_input )
            {
                vlc_value_t val, list, list2;
                int i_count, i;
                var_Get( p_input, "program", &val );

                var_Change( p_input, "program", VLC_VAR_GETCHOICES,
                            &list, &list2 );
                i_count = list.p_list->i_count;
                if( i_count <= 1 )
                {
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    _("Program Service ID: %s"), _("N/A") );
                    var_FreeList( &list, &list2 );
                    break;
                }
                for( i = 0; i < i_count; i++ )
                {
                    if( val.i_int == list.p_list->p_values[i].i_int )
                    {
                        break;
                    }
                }
                /* value of program was not in choices list */
                if( i == i_count )
                {
                    msg_Warn( p_input,
                              "invalid current program SID, selecting 0" );
                    i = 0;
                }
                else if( i == i_count - 1 )
                    i = 0;
                else
                    i++;
                var_Set( p_input, "program", list.p_list->p_values[i] );
                DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                _("Program Service ID: %s"),
                                list2.p_list->p_values[i].psz_string );
                var_FreeList( &list, &list2 );
            }
            break;

        case ACTIONID_JUMP_BACKWARD_EXTRASHORT:
        case ACTIONID_JUMP_FORWARD_EXTRASHORT:
        case ACTIONID_JUMP_BACKWARD_SHORT:
        case ACTIONID_JUMP_FORWARD_SHORT:
        case ACTIONID_JUMP_BACKWARD_MEDIUM:
        case ACTIONID_JUMP_FORWARD_MEDIUM:
        case ACTIONID_JUMP_BACKWARD_LONG:
        case ACTIONID_JUMP_FORWARD_LONG:
        {
            if( p_input == NULL || !var_GetBool( p_input, "can-seek" ) )
                break;

            const char *varname;
            int sign = +1;
            switch( i_action )
            {
                case ACTIONID_JUMP_BACKWARD_EXTRASHORT:
                    sign = -1;
                case ACTIONID_JUMP_FORWARD_EXTRASHORT:
                    varname = "extrashort-jump-size";
                    break;
                case ACTIONID_JUMP_BACKWARD_SHORT:
                    sign = -1;
                case ACTIONID_JUMP_FORWARD_SHORT:
                    varname = "short-jump-size";
                    break;
                case ACTIONID_JUMP_BACKWARD_MEDIUM:
                    sign = -1;
                case ACTIONID_JUMP_FORWARD_MEDIUM:
                    varname = "medium-jump-size";
                    break;
                case ACTIONID_JUMP_BACKWARD_LONG:
                    sign = -1;
                case ACTIONID_JUMP_FORWARD_LONG:
                    varname = "long-jump-size";
                    break;
            }

            mtime_t it = var_InheritInteger( p_input, varname );
            if( it < 0 )
                break;
            var_SetTime( p_input, "time-offset", it * sign * CLOCK_FREQ );
            DisplayPosition( p_intf, p_vout, p_input );
            break;
        }

        /* Input navigation (DVD & MKV) */
        case ACTIONID_TITLE_PREV:
            if( p_input )
                var_TriggerCallback( p_input, "prev-title" );
            break;
        case ACTIONID_TITLE_NEXT:
            if( p_input )
                var_TriggerCallback( p_input, "next-title" );
            break;
        case ACTIONID_CHAPTER_PREV:
            if( p_input )
                var_TriggerCallback( p_input, "prev-chapter" );
            break;
        case ACTIONID_CHAPTER_NEXT:
            if( p_input )
                var_TriggerCallback( p_input, "next-chapter" );
            break;
        case ACTIONID_DISC_MENU:
            if( p_input )
                var_SetInteger( p_input, "title  0", 2 );
            break;

        /* Video Output actions */
        case ACTIONID_SNAPSHOT:
            if( p_vout )
                var_TriggerCallback( p_vout, "video-snapshot" );
            break;

        case ACTIONID_TOGGLE_FULLSCREEN:
        {
            bool fs = var_ToggleBool( p_playlist, "fullscreen" );
            if( p_vout )
                var_SetBool( p_vout, "fullscreen", fs );
            break;
        }

        case ACTIONID_LEAVE_FULLSCREEN:
            if( p_vout )
                var_SetBool( p_vout, "fullscreen", false );
            var_SetBool( p_playlist, "fullscreen", false );
            break;

        case ACTIONID_ASPECT_RATIO:
            if( p_vout )
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
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    _("Aspect ratio: %s"),
                                    text_list.p_list->p_values[i].psz_string );

                    var_FreeList( &val_list, &text_list );
                }
                free( val.psz_string );
            }
            break;

        case ACTIONID_CROP:
            if( p_vout )
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
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    _("Crop: %s"),
                                    text_list.p_list->p_values[i].psz_string );

                    var_FreeList( &val_list, &text_list );
                }
                free( val.psz_string );
            }
            break;
        case ACTIONID_CROP_TOP:
            if( p_vout )
                var_IncInteger( p_vout, "crop-top" );
            break;
        case ACTIONID_UNCROP_TOP:
            if( p_vout )
                var_DecInteger( p_vout, "crop-top" );
            break;
        case ACTIONID_CROP_BOTTOM:
            if( p_vout )
                var_IncInteger( p_vout, "crop-bottom" );
            break;
        case ACTIONID_UNCROP_BOTTOM:
            if( p_vout )
                var_DecInteger( p_vout, "crop-bottom" );
            break;
        case ACTIONID_CROP_LEFT:
            if( p_vout )
                var_IncInteger( p_vout, "crop-left" );
            break;
        case ACTIONID_UNCROP_LEFT:
            if( p_vout )
                var_DecInteger( p_vout, "crop-left" );
            break;
        case ACTIONID_CROP_RIGHT:
            if( p_vout )
                var_IncInteger( p_vout, "crop-right" );
            break;
        case ACTIONID_UNCROP_RIGHT:
            if( p_vout )
                var_DecInteger( p_vout, "crop-right" );
            break;

         case ACTIONID_TOGGLE_AUTOSCALE:
            if( p_vout )
            {
                float f_scalefactor = var_GetFloat( p_vout, "scale" );
                if ( f_scalefactor != 1.f )
                {
                    var_SetFloat( p_vout, "scale", 1.f );
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    "%s", _("Zooming reset") );
                }
                else
                {
                    bool b_autoscale = !var_GetBool( p_vout, "autoscale" );
                    var_SetBool( p_vout, "autoscale", b_autoscale );
                    if( b_autoscale )
                        DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                        "%s", _("Scaled to screen") );
                    else
                        DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                        "%s", _("Original Size") );
                }
            }
            break;
        case ACTIONID_SCALE_UP:
            if( p_vout )
            {
               float f_scalefactor = var_GetFloat( p_vout, "scale" );

               if( f_scalefactor < 10.f )
                   f_scalefactor += .1f;
               var_SetFloat( p_vout, "scale", f_scalefactor );
            }
            break;
        case ACTIONID_SCALE_DOWN:
            if( p_vout )
            {
               float f_scalefactor = var_GetFloat( p_vout, "scale" );

               if( f_scalefactor > .3f )
                   f_scalefactor -= .1f;
               var_SetFloat( p_vout, "scale", f_scalefactor );
            }
            break;

        case ACTIONID_ZOOM_QUARTER:
        case ACTIONID_ZOOM_HALF:
        case ACTIONID_ZOOM_ORIGINAL:
        case ACTIONID_ZOOM_DOUBLE:
            if( p_vout )
            {
                float f;
                switch( i_action )
                {
                    case ACTIONID_ZOOM_QUARTER:  f = 0.25; break;
                    case ACTIONID_ZOOM_HALF:     f = 0.5;  break;
                    case ACTIONID_ZOOM_ORIGINAL: f = 1.;   break;
                     /*case ACTIONID_ZOOM_DOUBLE:*/
                    default:                     f = 2.;   break;
                }
                var_SetFloat( p_vout, "zoom", f );
            }
            break;
        case ACTIONID_ZOOM:
        case ACTIONID_UNZOOM:
            if( p_vout )
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
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    _("Zoom mode: %s"),
                                    text_list.p_list->p_values[i].psz_string );

                    var_FreeList( &val_list, &text_list );
                }
            }
            break;

        case ACTIONID_DEINTERLACE:
            if( p_vout )
            {
                int i_deinterlace = var_GetInteger( p_vout, "deinterlace" );
                if( i_deinterlace != 0 )
                {
                    var_SetInteger( p_vout, "deinterlace", 0 );
                    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                    "%s", _("Deinterlace off") );
                }
                else
                {
                    var_SetInteger( p_vout, "deinterlace", 1 );

                    char *psz_mode = var_GetString( p_vout, "deinterlace-mode" );
                    vlc_value_t vlist, tlist;
                    if( psz_mode && !var_Change( p_vout, "deinterlace-mode", VLC_VAR_GETCHOICES, &vlist, &tlist ) >= 0 )
                    {
                        const char *psz_text = NULL;
                        for( int i = 0; i < vlist.p_list->i_count; i++ )
                        {
                            if( !strcmp( vlist.p_list->p_values[i].psz_string, psz_mode ) )
                            {
                                psz_text = tlist.p_list->p_values[i].psz_string;
                                break;
                            }
                        }
                        DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                        "%s (%s)", _("Deinterlace on"), psz_text ? psz_text : psz_mode );

                        var_FreeList( &vlist, &tlist );
                    }
                    free( psz_mode );
                }
            }
            break;
        case ACTIONID_DEINTERLACE_MODE:
            if( p_vout )
            {
                char *psz_mode = var_GetString( p_vout, "deinterlace-mode" );
                vlc_value_t vlist, tlist;
                if( psz_mode && !var_Change( p_vout, "deinterlace-mode", VLC_VAR_GETCHOICES, &vlist, &tlist ) >= 0 )
                {
                    const char *psz_text = NULL;
                    int i;
                    for( i = 0; i < vlist.p_list->i_count; i++ )
                    {
                        if( !strcmp( vlist.p_list->p_values[i].psz_string, psz_mode ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == vlist.p_list->i_count ) i = 0;
                    psz_text = tlist.p_list->p_values[i].psz_string;
                    var_SetString( p_vout, "deinterlace-mode", vlist.p_list->p_values[i].psz_string );

                    int i_deinterlace = var_GetInteger( p_vout, "deinterlace" );
                    if( i_deinterlace != 0 )
                    {
                      DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                      "%s (%s)", _("Deinterlace on"), psz_text ? psz_text : psz_mode );
                    }
                    else
                    {
                      DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                                      "%s (%s)", _("Deinterlace off"), psz_text ? psz_text : psz_mode );
                    }

                    var_FreeList( &vlist, &tlist );
                }
                free( psz_mode );
            }
            break;

        case ACTIONID_SUBPOS_DOWN:
        case ACTIONID_SUBPOS_UP:
        {
            int i_pos;
            if( i_action == ACTIONID_SUBPOS_DOWN )
                i_pos = var_DecInteger( p_vout, "sub-margin" );
            else
                i_pos = var_IncInteger( p_vout, "sub-margin" );

            ClearChannels( p_intf, p_vout );
            DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL,
                            _( "Subtitle position %d px" ), i_pos );
            break;
        }

        /* Input + video output */
        case ACTIONID_POSITION:
            if( p_vout && vout_OSDEpg( p_vout, input_GetItem( p_input ) ) )
                DisplayPosition( p_intf, p_vout, p_input );
            break;
    }

    if( p_vout )
        vlc_object_release( p_vout );
    if( p_input )
        vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * SpecialKeyEvent: callback for mouse events
 *****************************************************************************/
static int SpecialKeyEvent( vlc_object_t *libvlc, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    int i_action = 0;

    (void)psz_var;
    (void)oldval;

    int i_mode = p_intf->p_sys->i_mousewheel_mode;

    /* Special action for mouse event */
    /* FIXME: rework hotkeys handling to allow more than 1 event
     * to trigger one same action */
    switch (newval.i_int & ~KEY_MODIFIER)
    {
        case KEY_MOUSEWHEELUP:
            i_action = (i_mode == MOUSEWHEEL_VOLUME ) ? ACTIONID_VOL_UP
                                 : ACTIONID_JUMP_FORWARD_EXTRASHORT;
            break;
        case KEY_MOUSEWHEELDOWN:
            i_action = (i_mode == MOUSEWHEEL_VOLUME ) ? ACTIONID_VOL_DOWN
                                : ACTIONID_JUMP_BACKWARD_EXTRASHORT;
            break;
        case KEY_MOUSEWHEELLEFT:
            i_action = (i_mode == MOUSEWHEEL_VOLUME ) ?
                        ACTIONID_JUMP_BACKWARD_EXTRASHORT : ACTIONID_VOL_DOWN;
            break;
        case KEY_MOUSEWHEELRIGHT:
            i_action = (i_mode == MOUSEWHEEL_VOLUME ) ?
                        ACTIONID_JUMP_FORWARD_EXTRASHORT : ACTIONID_VOL_UP;
            break;
        case KEY_MENU:
            var_SetBool( libvlc, "intf-popupmenu", true );
            break;
    }

    if( i_mode == NO_MOUSEWHEEL ) return VLC_SUCCESS;

    if( i_action )
        return PutAction( p_intf, i_action );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ActionEvent: callback for hotkey actions
 *****************************************************************************/
static int ActionEvent( vlc_object_t *libvlc, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    (void)libvlc;
    (void)psz_var;
    (void)oldval;

    return PutAction( p_intf, newval.i_int );
}

static void PlayBookmark( intf_thread_t *p_intf, int i_num )
{
    char *psz_bookmark_name;
    if( asprintf( &psz_bookmark_name, "bookmark%i", i_num ) == -1 )
        return;

    playlist_t *p_playlist = pl_Get( p_intf );
    char *psz_bookmark = var_CreateGetString( p_intf, psz_bookmark_name );

    PL_LOCK;
    FOREACH_ARRAY( playlist_item_t *p_item, p_playlist->items )
        char *psz_uri = input_item_GetURI( p_item->p_input );
        if( !strcmp( psz_bookmark, psz_uri ) )
        {
            free( psz_uri );
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked,
                              NULL, p_item );
            break;
        }
        else
            free( psz_uri );
    FOREACH_END();
    PL_UNLOCK;

    free( psz_bookmark );
    free( psz_bookmark_name );
}

static void SetBookmark( intf_thread_t *p_intf, int i_num )
{
    char *psz_bookmark_name;
    char *psz_uri = NULL;
    if( asprintf( &psz_bookmark_name, "bookmark%i", i_num ) == -1 )
        return;

    playlist_t *p_playlist = pl_Get( p_intf );
    var_Create( p_intf, psz_bookmark_name,
                VLC_VAR_STRING|VLC_VAR_DOINHERIT );

    PL_LOCK;
    playlist_item_t * p_item = playlist_CurrentPlayingItem( p_playlist );
    if( p_item ) psz_uri = input_item_GetURI( p_item->p_input );
    PL_UNLOCK;

    if( p_item )
    {
        config_PutPsz( p_intf, psz_bookmark_name, psz_uri);
        msg_Info( p_intf, "setting playlist bookmark %i to %s", i_num, psz_uri);
    }

    free( psz_uri );
    free( psz_bookmark_name );
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
        DisplayMessage( p_vout, POSITION_TEXT_CHAN, "%s / %s",
                        psz_time, psz_duration );
    }
    else if( i_seconds > 0 )
    {
        DisplayMessage( p_vout, POSITION_TEXT_CHAN, "%s", psz_time );
    }

    if( var_GetBool( p_vout, "fullscreen" ) )
    {
        var_Get( p_input, "position", &pos );
        vout_OSDSlider( p_vout, POSITION_WIDGET_CHAN,
                        pos.f_float * 100, OSD_HOR_SLIDER );
    }
}

static void DisplayVolume( intf_thread_t *p_intf, vout_thread_t *p_vout,
                           float vol )
{
    if( p_vout == NULL )
        return;
    ClearChannels( p_intf, p_vout );

    if( var_GetBool( p_vout, "fullscreen" ) )
        vout_OSDSlider( p_vout, VOLUME_WIDGET_CHAN, lround(vol * 100.),
                        OSD_VERT_SLIDER );
    DisplayMessage( p_vout, VOLUME_TEXT_CHAN, _( "Volume %ld%%" ),
                    lround(vol * 100.) );
}

static void DisplayRate( vout_thread_t *p_vout, float f_rate )
{
    DisplayMessage( p_vout, SPU_DEFAULT_CHANNEL, _("Speed: %.2fx"), f_rate );
}

static float AdjustRateFine( vlc_object_t *p_obj, const int i_dir )
{
    const float f_rate_min = (float)INPUT_RATE_DEFAULT / INPUT_RATE_MAX;
    const float f_rate_max = (float)INPUT_RATE_DEFAULT / INPUT_RATE_MIN;
    float f_rate = var_GetFloat( p_obj, "rate" );

    int i_sign = f_rate < 0 ? -1 : 1;

    f_rate = floor( fabs(f_rate) / 0.1 + i_dir + 0.05 ) * 0.1;

    if( f_rate < f_rate_min )
        f_rate = f_rate_min;
    else if( f_rate > f_rate_max )
        f_rate = f_rate_max;
    f_rate *= i_sign;

    return f_rate;
}

static void ClearChannels( intf_thread_t *p_intf, vout_thread_t *p_vout )
{
    if( p_vout )
    {
        vout_FlushSubpictureChannel( p_vout, SPU_DEFAULT_CHANNEL );
        for( int i = 0; i < CHANNELS_NUMBER; i++ )
            vout_FlushSubpictureChannel( p_vout, p_intf->p_sys->p_channels[i]  );
    }
}
