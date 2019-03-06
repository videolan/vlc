/*****************************************************************************
 * hotkeys.c: Hotkey handling for vlc
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_aout.h>
#include <vlc_mouse.h>
#include <vlc_viewpoint.h>
#include <vlc_vout_osd.h>
#include <vlc_playlist_legacy.h>
#include <vlc_actions.h>
#include "math.h"

#include <assert.h>

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t         lock;
    vout_thread_t      *p_vout;
    input_thread_t     *p_input;
    int slider_chan;

    /*subtitle_delaybookmarks: placeholder for storing subtitle sync timestamps*/
    struct
    {
        vlc_tick_t i_time_subtitle;
        vlc_tick_t i_time_audio;
    } subtitle_delaybookmarks;

    struct
    {
        bool b_can_change;
        bool b_button_pressed;
        int x, y;
    } vrnav;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static int  ActionEvent( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static void PlayBookmark( intf_thread_t *, int );
static void SetBookmark ( intf_thread_t *, int );
static void DisplayPosition( vout_thread_t *, int,  input_thread_t * );
static void DisplayVolume( vout_thread_t *, int, float );
static void DisplayRate ( vout_thread_t *, float );
static float AdjustRateFine( vlc_object_t *, const int );
static void ClearChannels  ( vout_thread_t *, int );

#define DisplayMessage(vout, ...) \
    do { \
        if (vout) \
            vout_OSDMessage(vout, VOUT_SPU_CHANNEL_OSD, __VA_ARGS__); \
    } while(0)
#define DisplayIcon(vout, icon) \
    do { if(vout) vout_OSDIcon(vout, VOUT_SPU_CHANNEL_OSD, icon); } while(0)

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

static void var_FreeList( size_t n, vlc_value_t *values, char **texts )
{
    free( values );

    for( size_t i = 0; i < n; i++ )
        free( texts[i] );
    free( texts );
}

static void var_FreeStringList( size_t n, vlc_value_t *values, char **texts )
{
    for( size_t i = 0; i < n; i++ )
         free( values[i].psz_string );

    var_FreeList( n, values, texts );
}

static int MovedEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t    *p_sys = p_intf->p_sys;

    (void) p_this; (void) psz_var; (void) oldval;

    if( p_sys->vrnav.b_button_pressed )
    {
        int i_horizontal = newval.coords.x - p_sys->vrnav.x;
        int i_vertical   = newval.coords.y - p_sys->vrnav.y;

        vlc_viewpoint_t viewpoint = {
            .yaw   = -i_horizontal * 0.05f,
            .pitch = -i_vertical   * 0.05f,
        };

        input_UpdateViewpoint( p_sys->p_input, &viewpoint, false );

        p_sys->vrnav.x = newval.coords.x;
        p_sys->vrnav.y = newval.coords.y;
    }

    return VLC_SUCCESS;
}

static int ViewpointMovedEvent( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t    *p_sys = p_intf->p_sys;

    (void) p_this; (void) psz_var; (void) oldval;

    input_UpdateViewpoint( p_sys->p_input, newval.p_address, false );

    return VLC_SUCCESS;
}

static int ButtonEvent( vlc_object_t *p_this, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) psz_var;

    if ((newval.i_int & (1 << MOUSE_BUTTON_LEFT)) && p_sys->vrnav.b_can_change)
    {
        if( !p_sys->vrnav.b_button_pressed )
        {
            p_sys->vrnav.b_button_pressed = true;
            var_GetCoords( p_this, "mouse-moved",
                           &p_sys->vrnav.x, &p_sys->vrnav.y );
        }
    }
    else
        p_sys->vrnav.b_button_pressed = false;

    unsigned pressed = newval.i_int & ~oldval.i_int;

    if (pressed & (1 << MOUSE_BUTTON_LEFT))
        var_SetBool(pl_Get(p_intf), "intf-popupmenu", false);
    if (pressed & (1 << MOUSE_BUTTON_CENTER))
        var_TriggerCallback(pl_Get(p_intf), "intf-toggle-fscontrol");
#ifndef _WIN32
    if (pressed & (1 << MOUSE_BUTTON_RIGHT))
#else
    if ((oldval.i_int & (1 << MOUSE_BUTTON_RIGHT))
     && !(newval.i_int & (1 << MOUSE_BUTTON_RIGHT)))
#endif
        var_SetBool(pl_Get(p_intf), "intf-popupmenu", true);

    for (int i = MOUSE_BUTTON_WHEEL_UP; i <= MOUSE_BUTTON_WHEEL_RIGHT; i++)
        if (pressed & (1 << i))
        {
            int keycode = KEY_MOUSEWHEEL_FROM_BUTTON(i);
            var_SetInteger(vlc_object_instance(p_intf), "key-pressed", keycode);
        }

    return VLC_SUCCESS;
}

static void ChangeVout( intf_thread_t *p_intf, vout_thread_t *p_vout )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    int slider_chan;
    bool b_vrnav_can_change;
    if( p_vout != NULL )
    {
        slider_chan = vout_RegisterSubpictureChannel( p_vout );
        b_vrnav_can_change = var_GetBool( p_vout, "viewpoint-changeable" );
    }

    vout_thread_t *p_old_vout = p_sys->p_vout;
    if( p_old_vout != NULL && p_sys->vrnav.b_can_change )
        var_DelCallback( p_old_vout, "viewpoint-moved", ViewpointMovedEvent,
                         p_intf );

    vlc_mutex_lock( &p_sys->lock );
    p_sys->p_vout = p_vout;
    if( p_vout != NULL )
    {
        p_sys->slider_chan = slider_chan;
        p_sys->vrnav.b_can_change = b_vrnav_can_change;
    }
    else
        p_sys->vrnav.b_can_change = false;
    vlc_mutex_unlock( &p_sys->lock );

    if( p_old_vout != NULL )
    {
        var_DelCallback( p_old_vout, "mouse-button-down", ButtonEvent,
                         p_intf );
        var_DelCallback( p_old_vout, "mouse-moved", MovedEvent, p_intf );
        vlc_object_release( p_old_vout );
    }

    if( p_vout != NULL )
    {
        var_AddCallback( p_vout, "mouse-moved", MovedEvent, p_intf );
        var_AddCallback( p_vout, "mouse-button-down", ButtonEvent, p_intf );

        if( p_sys->vrnav.b_can_change )
            var_AddCallback( p_vout, "viewpoint-moved",
                             ViewpointMovedEvent, p_intf );
    }
}

static int InputEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    intf_thread_t *p_intf = p_data;

    (void) psz_var; (void) oldval;

    if( val.i_int == INPUT_EVENT_VOUT )
        ChangeVout( p_intf, input_GetVout( p_input ) );

    return VLC_SUCCESS;
}

static void ChangeInput( intf_thread_t *p_intf, input_thread_t *p_input )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    input_thread_t *p_old_input = p_sys->p_input;
    vout_thread_t *p_old_vout = NULL;
    if( p_old_input != NULL )
    {
        /* First, remove callbacks from previous input. It's safe to access it
         * unlocked, since it's written from this thread */
        var_DelCallback( p_old_input, "intf-event", InputEvent, p_intf );

        p_old_vout = p_sys->p_vout;
        /* Remove mouse events before setting new input, since callbacks may
         * access it */
        if( p_old_vout != NULL )
        {
            if( p_sys->vrnav.b_can_change )
                var_DelCallback( p_old_vout, "viewpoint-moved",
                                 ViewpointMovedEvent, p_intf );

            var_DelCallback( p_old_vout, "mouse-button-down", ButtonEvent,
                             p_intf );
            var_DelCallback( p_old_vout, "mouse-moved", MovedEvent,
                             p_intf );
        }
    }

    /* Replace input and vout locked */
    vlc_mutex_lock( &p_sys->lock );
    p_sys->p_input = p_input ? input_Hold(p_input) : NULL;
    p_sys->p_vout = NULL;
    p_sys->vrnav.b_can_change = false;
    vlc_mutex_unlock( &p_sys->lock );

    /* Release old input and vout objects unlocked */
    if( p_old_input != NULL )
    {
        if( p_old_vout != NULL )
            vlc_object_release( p_old_vout );
        input_Release(p_old_input);
    }

    /* Register input events */
    if( p_input != NULL )
        var_AddCallback( p_input, "intf-event", InputEvent, p_intf );
}

static int PlaylistEvent( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    intf_thread_t *p_intf = p_data;

    (void) p_this; (void) psz_var; (void) oldval;

    ChangeInput( p_intf, val.p_address );

    return VLC_SUCCESS;
}

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

    p_sys->p_vout = NULL;
    p_sys->p_input = NULL;
    p_sys->vrnav.b_can_change = false;
    p_sys->vrnav.b_button_pressed = false;
    p_sys->subtitle_delaybookmarks.i_time_audio = VLC_TICK_INVALID;
    p_sys->subtitle_delaybookmarks.i_time_subtitle = VLC_TICK_INVALID;

    vlc_mutex_init( &p_sys->lock );

    var_AddCallback( vlc_object_instance(p_intf), "key-action", ActionEvent, p_intf );

    var_AddCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    var_DelCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    var_DelCallback( vlc_object_instance(p_intf), "key-action", ActionEvent, p_intf );

    ChangeInput( p_intf, NULL );

    vlc_mutex_destroy( &p_sys->lock );

    /* Destroy structure */
    free( p_sys );
}

static int PutAction( intf_thread_t *p_intf, input_thread_t *p_input,
                      vout_thread_t *p_vout, int slider_chan, bool b_vrnav,
                      int i_action )
{
#define DO_ACTION(x) PutAction( p_intf, p_input, p_vout, slider_chan, b_vrnav, x)
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get( p_intf );

    /* Quit */
    switch( i_action )
    {
        /* Libvlc / interface actions */
        case ACTIONID_QUIT:
            libvlc_Quit( vlc_object_instance(p_intf) );

            ClearChannels( p_vout, slider_chan );
            DisplayMessage( p_vout, _( "Quit" ) );
            break;

        case ACTIONID_INTF_TOGGLE_FSC:
        case ACTIONID_INTF_HIDE:
            var_TriggerCallback( p_playlist, "intf-toggle-fscontrol" );
            break;
        case ACTIONID_INTF_BOSS:
            var_TriggerCallback( p_playlist, "intf-boss" );
            break;
        case ACTIONID_INTF_POPUP_MENU:
            var_TriggerCallback( p_playlist, "intf-popupmenu" );
            break;

        /* Playlist actions (including audio) */
        case ACTIONID_LOOP:
        {
            /* Toggle Normal -> Loop -> Repeat -> Normal ... */
            const char *mode;
            if( var_GetBool( p_playlist, "repeat" ) )
            {
                var_SetBool( p_playlist, "repeat", false );
                mode = N_("Off");
            }
            else
            if( var_GetBool( p_playlist, "loop" ) )
            { /* FIXME: this is not atomic, we should use a real tristate */
                var_SetBool( p_playlist, "loop", false );
                var_SetBool( p_playlist, "repeat", true );
                mode = N_("One");
            }
            else
            {
                var_SetBool( p_playlist, "loop", true );
                mode = N_("All");
            }
            DisplayMessage( p_vout, _("Loop: %s"), vlc_gettext(mode) );
            break;
        }

        case ACTIONID_RANDOM:
        {
            const bool state = var_ToggleBool( p_playlist, "random" );
            DisplayMessage( p_vout, _("Random: %s"),
                            vlc_gettext( state ? N_("On") : N_("Off") ) );
            break;
        }

        case ACTIONID_NEXT:
            DisplayMessage( p_vout, _("Next") );
            playlist_Next( p_playlist );
            break;
        case ACTIONID_PREV:
            DisplayMessage( p_vout, _("Previous") );
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
        case ACTIONID_PLAY_CLEAR:
            playlist_Clear( p_playlist, pl_Unlocked );
            break;
        case ACTIONID_VOL_UP:
        {
            float vol;
            if( playlist_VolumeUp( p_playlist, 1, &vol ) == 0 )
                DisplayVolume( p_vout, slider_chan, vol );
            break;
        }
        case ACTIONID_VOL_DOWN:
        {
            float vol;
            if( playlist_VolumeDown( p_playlist, 1, &vol ) == 0 )
                DisplayVolume( p_vout, slider_chan, vol );
            break;
        }
        case ACTIONID_VOL_MUTE:
        {
            int mute = playlist_MuteGet( p_playlist );
            if( mute < 0 )
                break;
            mute = !mute;
            if( playlist_MuteSet( p_playlist, mute ) )
                break;

            float vol = playlist_VolumeGet( p_playlist );
            if( mute || vol == 0.f )
            {
                ClearChannels( p_vout, slider_chan );
                DisplayIcon( p_vout, OSD_MUTE_ICON );
            }
            else
                DisplayVolume( p_vout, slider_chan, vol );
            break;
        }

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
                DisplayMessage( p_vout, _("Audio Device: %s"), names[idx] );
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
                ClearChannels( p_vout, slider_chan );

                int state = var_GetInteger( p_input, "state" );
                DisplayIcon( p_vout, state != PAUSE_S ? OSD_PAUSE_ICON : OSD_PLAY_ICON );
            }
            playlist_TogglePause( p_playlist );
            break;

        case ACTIONID_PLAY:
            if( p_input && var_GetFloat( p_input, "rate" ) != 1.f )
                /* Return to normal speed */
                var_SetFloat( p_input, "rate", 1.f );
            else
            {
                ClearChannels( p_vout, slider_chan );
                DisplayIcon( p_vout, OSD_PLAY_ICON );
                playlist_Play( p_playlist );
            }
            break;

        /* Playlist + video output actions */
        case ACTIONID_WALLPAPER:
        {
            bool wp = var_ToggleBool( p_playlist, "video-wallpaper" );
            if( p_vout )
                var_SetBool( p_vout, "video-wallpaper", wp );
            break;
        }

        /* Input actions */
        case ACTIONID_PAUSE:
            if( p_input && var_GetInteger( p_input, "state" ) != PAUSE_S )
            {
                ClearChannels( p_vout, slider_chan );
                DisplayIcon( p_vout, OSD_PAUSE_ICON );
                var_SetInteger( p_input, "state", PAUSE_S );
            }
            break;

        case ACTIONID_RECORD:
            if( p_input && var_GetBool( p_input, "can-record" ) )
            {
                const bool on = var_ToggleBool( p_input, "record" );
                DisplayMessage( p_vout, vlc_gettext(on
                                   ? N_("Recording") : N_("Recording done")) );
            }
            break;

        case ACTIONID_FRAME_NEXT:
            if( p_input )
            {
                var_TriggerCallback( p_input, "frame-next" );
                DisplayMessage( p_vout, _("Next frame") );
            }
            break;

        case ACTIONID_SUBSYNC_MARKAUDIO:
        {
            p_sys->subtitle_delaybookmarks.i_time_audio = vlc_tick_now();
            DisplayMessage( p_vout, _("Sub sync: bookmarked audio time"));
            break;
        }
        case ACTIONID_SUBSYNC_MARKSUB:
            if( p_input )
            {
                vlc_value_t val;
                vlc_value_t *list;
                size_t count;

                var_Get( p_input, "spu-es", &val );
                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &count, &list, (char ***)NULL );

                if( count < 1 || val.i_int < 0 )
                {
                    DisplayMessage( p_vout, _("No active subtitle") );
                }
                else
                {
                    p_sys->subtitle_delaybookmarks.i_time_subtitle = vlc_tick_now();
                    DisplayMessage(p_vout,
                                   _("Sub sync: bookmarked subtitle time"));
                }
                free(list);
            }
            break;
        case ACTIONID_SUBSYNC_APPLY:
        {
            /* Warning! Can yield a pause in the playback.
             * For example, the following succession of actions will yield a 5 second delay :
             * - Pressing Shift-H (ACTIONID_SUBSYNC_MARKAUDIO)
             * - wait 5 second
             * - Press Shift-J (ACTIONID_SUBSYNC_MARKSUB)
             * - Press Shift-K (ACTIONID_SUBSYNC_APPLY)
             * --> 5 seconds pause
             * This is due to var_SetTime() (and ultimately UpdatePtsDelay())
             * which causes the video to pause for an equivalent duration
             * (This problem is also present in the "Track synchronization" window) */
            if ( p_input )
            {
                if ( (p_sys->subtitle_delaybookmarks.i_time_audio == VLC_TICK_INVALID) || (p_sys->subtitle_delaybookmarks.i_time_subtitle == VLC_TICK_INVALID) )
                {
                    DisplayMessage( p_vout, _( "Sub sync: set bookmarks first!" ) );
                }
                else
                {
                    vlc_tick_t i_current_subdelay = var_GetInteger( p_input, "spu-delay" );
                    vlc_tick_t i_additional_subdelay = p_sys->subtitle_delaybookmarks.i_time_audio - p_sys->subtitle_delaybookmarks.i_time_subtitle;
                    vlc_tick_t i_total_subdelay = i_current_subdelay + i_additional_subdelay;
                    var_SetInteger( p_input, "spu-delay", i_total_subdelay);
                    ClearChannels( p_vout, slider_chan );
                    DisplayMessage( p_vout, _( "Sub sync: corrected %"PRId64" ms (total delay = %"PRId64" ms)" ),
                                            MS_FROM_VLC_TICK( i_additional_subdelay ),
                                            MS_FROM_VLC_TICK( i_total_subdelay ) );
                    p_sys->subtitle_delaybookmarks.i_time_audio = VLC_TICK_INVALID;
                    p_sys->subtitle_delaybookmarks.i_time_subtitle = VLC_TICK_INVALID;
                }
            }
            break;
        }
        case ACTIONID_SUBSYNC_RESET:
        {
            var_SetInteger( p_input, "spu-delay", 0);
            ClearChannels( p_vout, slider_chan );
            DisplayMessage( p_vout, _( "Sub sync: delay reset" ) );
            p_sys->subtitle_delaybookmarks.i_time_audio = VLC_TICK_INVALID;
            p_sys->subtitle_delaybookmarks.i_time_subtitle = VLC_TICK_INVALID;
            break;
        }

        case ACTIONID_SUBDELAY_DOWN:
        case ACTIONID_SUBDELAY_UP:
        {
            vlc_tick_t diff = (i_action == ACTIONID_SUBDELAY_UP) ? VLC_TICK_FROM_MS(50) : VLC_TICK_FROM_MS(-50);
            if( p_input )
            {
                vlc_value_t val;
                vlc_value_t *list;
                size_t count;

                var_Get( p_input, "spu-es", &val );
                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &count, &list, (char ***)NULL );

                if( count < 1 || val.i_int < 0 )
                {
                    DisplayMessage( p_vout, _("No active subtitle") );
                    free(list);
                    break;
                }
                vlc_tick_t i_delay = var_GetInteger( p_input, "spu-delay" ) + diff;

                var_SetInteger( p_input, "spu-delay", i_delay );
                ClearChannels( p_vout, slider_chan );
                DisplayMessage( p_vout, _( "Subtitle delay %i ms" ),
                                (int)MS_FROM_VLC_TICK(i_delay) );
                free(list);
            }
            break;
        }
        case ACTIONID_AUDIODELAY_DOWN:
        case ACTIONID_AUDIODELAY_UP:
        {
            vlc_tick_t diff = (i_action == ACTIONID_AUDIODELAY_UP) ? VLC_TICK_FROM_MS(50) : VLC_TICK_FROM_MS(-50);
            if( p_input )
            {
                vlc_tick_t i_delay = var_GetInteger( p_input, "audio-delay" )
                                  + diff;

                var_SetInteger( p_input, "audio-delay", i_delay );
                ClearChannels( p_vout, slider_chan );
                DisplayMessage( p_vout, _( "Audio delay %i ms" ),
                                 (int)MS_FROM_VLC_TICK(i_delay) );
            }
            break;
        }

        case ACTIONID_AUDIO_TRACK:
            if( p_input )
            {
                vlc_value_t val;
                vlc_value_t *list;
                char **list2;
                size_t count;

                var_Get( p_input, "audio-es", &val );
                var_Change( p_input, "audio-es", VLC_VAR_GETCHOICES,
                            &count, &list, &list2 );

                if( count > 1 )
                {
                    size_t i;

                    for( i = 0; i < count; i++ )
                        if( val.i_int == list[i].i_int )
                            break;
                    /* value of audio-es was not in choices list */
                    if( i == count )
                    {
                        msg_Warn( p_input,
                                  "invalid current audio track, selecting 0" );
                        i = 0;
                    }
                    else if( i == count - 1 )
                        i = 1;
                    else
                        i++;
                    var_Set( p_input, "audio-es", list[i] );
                    DisplayMessage( p_vout, _("Audio track: %s"), list2[i] );
                }
                var_FreeList( count, list, list2 );
            }
            break;

        case ACTIONID_SUBTITLE_TRACK:
        case ACTIONID_SUBTITLE_REVERSE_TRACK:
            if( p_input )
            {
                vlc_value_t val;
                vlc_value_t *list;
                char **list2;
                size_t count, i;
                var_Get( p_input, "spu-es", &val );

                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &count, &list, &list2 );

                if( count <= 1 )
                {
                    DisplayMessage( p_vout, _("Subtitle track: %s"),
                                    _("N/A") );
                    var_FreeList( count, list, list2 );
                    break;
                }
                for( i = 0; i < count; i++ )
                    if( val.i_int == list[i].i_int )
                        break;
                /* value of spu-es was not in choices list */
                if( i == count )
                {
                    msg_Warn( p_input,
                              "invalid current subtitle track, selecting 0" );
                    i = 0;
                }
                else if ((i == count - 1) && (i_action == ACTIONID_SUBTITLE_TRACK))
                    i = 0;
                else if ((i == 0) && (i_action == ACTIONID_SUBTITLE_REVERSE_TRACK))
                    i = count - 1;
                else
                    i = (i_action == ACTIONID_SUBTITLE_TRACK) ? i+1 : i-1;
                var_SetInteger( p_input, "spu-es", list[i].i_int );
                DisplayMessage( p_vout, _("Subtitle track: %s"), list2[i] );
                var_FreeList( count, list, list2 );
            }
            break;
        case ACTIONID_SUBTITLE_TOGGLE:
            if( p_input )
            {
                vlc_value_t *list;
                char **list2;
                size_t count;

                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &count, &list, &list2 );

                if( count <= 1 )
                {
                    DisplayMessage( p_vout, _("Subtitle track: %s"),
                                    _("N/A") );
                    var_FreeList( count, list, list2 );
                    break;
                }

                int i_cur_id = var_GetInteger( p_input, "spu-es" );
                int i_new_id;
                if( i_cur_id == -1 )
                {
                    /* subtitles were disabled: restore the saved track id */
                    i_new_id = var_GetInteger( p_input, "spu-choice" );
                    if( i_new_id != -1 )
                        var_SetInteger( p_input, "spu-choice", -1 );
                }
                else
                {
                    /* subtitles were enabled: save the track id and disable */
                    i_new_id = -1;
                    var_SetInteger( p_input, "spu-choice", i_cur_id );
                }

                int i_new_index = 1; /* select first track by default */
                /* if subtitles were disabled with no saved id, use the first track */
                if( i_cur_id != -1 || i_new_id != -1 )
                {
                    for( size_t i = 0; i < count; ++i )
                    {
                        if( i_new_id == list[i].i_int )
                        {
                            i_new_index = i;
                            break;
                        }
                    }
                }
                var_SetInteger( p_input, "spu-es", list[i_new_index].i_int );
                DisplayMessage( p_vout, _("Subtitle track: %s"),
                                list2[i_new_index] );
                var_FreeList( count, list, list2 );
            }
            break;
        case ACTIONID_PROGRAM_SID_NEXT:
        case ACTIONID_PROGRAM_SID_PREV:
            if( p_input )
            {
                vlc_value_t val;
                vlc_value_t *list;
                char **list2;
                size_t count, i;
                var_Get( p_input, "program", &val );

                var_Change( p_input, "program", VLC_VAR_GETCHOICES,
                            &count, &list, &list2 );

                if( count <= 1 )
                {
                    DisplayMessage( p_vout, _("Program Service ID: %s"),
                                    _("N/A") );
                    var_FreeList( count, list, list2 );
                    break;
                }
                for( i = 0; i < count; i++ )
                    if( val.i_int == list[i].i_int )
                        break;
                /* value of program sid was not in choices list */
                if( i == count )
                {
                    msg_Warn( p_input,
                              "invalid current program SID, selecting 0" );
                    i = 0;
                }
                else if( i_action == ACTIONID_PROGRAM_SID_NEXT ) {
                    if( i == count - 1 )
                        i = 0;
                    else
                        i++;
                    }
                else { /* ACTIONID_PROGRAM_SID_PREV */
                    if( i == 0 )
                        i = count - 1;
                    else
                        i--;
                    }
                var_Set( p_input, "program", list[i] );
                DisplayMessage( p_vout, _("Program Service ID: %s"),
                                list2[i] );
                var_FreeList( count, list, list2 );
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
                    /* fall through */
                case ACTIONID_JUMP_FORWARD_EXTRASHORT:
                    varname = "extrashort-jump-size";
                    break;
                case ACTIONID_JUMP_BACKWARD_SHORT:
                    sign = -1;
                    /* fall through */
                case ACTIONID_JUMP_FORWARD_SHORT:
                    varname = "short-jump-size";
                    break;
                case ACTIONID_JUMP_BACKWARD_MEDIUM:
                    sign = -1;
                    /* fall through */
                case ACTIONID_JUMP_FORWARD_MEDIUM:
                    varname = "medium-jump-size";
                    break;
                case ACTIONID_JUMP_BACKWARD_LONG:
                    sign = -1;
                    /* fall through */
                case ACTIONID_JUMP_FORWARD_LONG:
                    varname = "long-jump-size";
                    break;
            }

            int it = var_InheritInteger( p_input, varname );
            if( it < 0 )
                break;
            var_SetInteger( p_input, "time-offset", vlc_tick_from_sec( it * sign ) );
            DisplayPosition( p_vout, slider_chan, p_input );
            break;
        }

        /* Input navigation */
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
        case ACTIONID_NAV_ACTIVATE:
            if( p_input )
                input_Control( p_input, INPUT_NAV_ACTIVATE, NULL );
            break;
        case ACTIONID_NAV_UP:
            if( p_input )
                input_Control( p_input, INPUT_NAV_UP, NULL );
            break;
        case ACTIONID_NAV_DOWN:
            if( p_input )
                input_Control( p_input, INPUT_NAV_DOWN, NULL );
            break;
        case ACTIONID_NAV_LEFT:
            if( p_input )
                input_Control( p_input, INPUT_NAV_LEFT, NULL );
            break;
        case ACTIONID_NAV_RIGHT:
            if( p_input )
                input_Control( p_input, INPUT_NAV_RIGHT, NULL );
            break;

        /* Video Output actions */
        case ACTIONID_SNAPSHOT:
            if( p_vout )
                var_TriggerCallback( p_vout, "video-snapshot" );
            break;

        case ACTIONID_TOGGLE_FULLSCREEN:
        {
            if( p_vout )
            {
                bool fs = var_ToggleBool( p_vout, "fullscreen" );
                var_SetBool( p_playlist, "fullscreen", fs );
            }
            else
                var_ToggleBool( p_playlist, "fullscreen" );
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
                vlc_value_t val;
                vlc_value_t *val_list;
                char **text_list;
                size_t count;

                var_Get( p_vout, "aspect-ratio", &val );
                if( var_Change( p_vout, "aspect-ratio", VLC_VAR_GETCHOICES,
                                &count, &val_list, &text_list ) >= 0 )
                {
                    size_t i;
                    for( i = 0; i < count; i++ )
                    {
                        if( !strcmp( val_list[i].psz_string, val.psz_string ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == count ) i = 0;
                    var_SetString( p_vout, "aspect-ratio",
                                   val_list[i].psz_string );
                    DisplayMessage( p_vout, _("Aspect ratio: %s"),
                                    text_list[i] );

                    var_FreeStringList( count, val_list, text_list );
                }
                free( val.psz_string );
            }
            break;

        case ACTIONID_CROP:
            if( p_vout )
            {
                vlc_value_t val;
                vlc_value_t *val_list;
                char **text_list;
                size_t count;

                var_Get( p_vout, "crop", &val );
                if( var_Change( p_vout, "crop", VLC_VAR_GETCHOICES,
                                &count, &val_list, &text_list ) >= 0 )
                {
                    size_t i;
                    for( i = 0; i < count; i++ )
                    {
                        if( !strcmp( val_list[i].psz_string, val.psz_string ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == count ) i = 0;
                    var_SetString( p_vout, "crop", val_list[i].psz_string );
                    DisplayMessage( p_vout, _("Crop: %s"), text_list[i] );

                    var_FreeStringList( count, val_list, text_list );
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

        case ACTIONID_VIEWPOINT_FOV_IN:
            if( p_vout )
                input_UpdateViewpoint( p_input,
                                       &(vlc_viewpoint_t) { .fov = -1.f },
                                       false );
            break;
        case ACTIONID_VIEWPOINT_FOV_OUT:
            if( p_vout )
                input_UpdateViewpoint( p_input,
                                       &(vlc_viewpoint_t) { .fov = 1.f },
                                       false );
            break;

        case ACTIONID_VIEWPOINT_ROLL_CLOCK:
            if( p_vout )
                input_UpdateViewpoint( p_input,
                                       &(vlc_viewpoint_t) { .roll = -1.f },
                                       false );
            break;
        case ACTIONID_VIEWPOINT_ROLL_ANTICLOCK:
            if( p_vout )
                input_UpdateViewpoint( p_input,
                                       &(vlc_viewpoint_t) { .roll = 1.f },
                                       false );
            break;

         case ACTIONID_TOGGLE_AUTOSCALE:
            if( p_vout )
            {
                float f_scalefactor = var_GetFloat( p_vout, "zoom" );
                if ( f_scalefactor != 1.f )
                {
                    var_SetFloat( p_vout, "zoom", 1.f );
                    DisplayMessage( p_vout, _("Zooming reset") );
                }
                else
                {
                    bool b_autoscale = !var_GetBool( p_vout, "autoscale" );
                    var_SetBool( p_vout, "autoscale", b_autoscale );
                    if( b_autoscale )
                        DisplayMessage( p_vout, _("Scaled to screen") );
                    else
                        DisplayMessage( p_vout, _("Original Size") );
                }
            }
            break;
        case ACTIONID_SCALE_UP:
            if( p_vout )
            {
               float f_scalefactor = var_GetFloat( p_vout, "zoom" );

               if( f_scalefactor < 10.f )
                   f_scalefactor += .1f;
               var_SetFloat( p_vout, "zoom", f_scalefactor );
            }
            break;
        case ACTIONID_SCALE_DOWN:
            if( p_vout )
            {
               float f_scalefactor = var_GetFloat( p_vout, "zoom" );

               if( f_scalefactor > .3f )
                   f_scalefactor -= .1f;
               var_SetFloat( p_vout, "zoom", f_scalefactor );
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
                vlc_value_t val;
                vlc_value_t *val_list;
                char **text_list;
                size_t count;

                var_Get( p_vout, "zoom", &val );
                if( var_Change( p_vout, "zoom", VLC_VAR_GETCHOICES,
                                &count, &val_list, &text_list ) >= 0 )
                {
                    size_t i;
                    for( i = 0; i < count; i++ )
                    {
                        if( val_list[i].f_float == val.f_float )
                        {
                            if( i_action == ACTIONID_ZOOM )
                                i++;
                            else /* ACTIONID_UNZOOM */
                                i--;
                            break;
                        }
                    }
                    if( i == count ) i = 0;
                    if( i == (size_t)-1 ) i = count-1;
                    var_SetFloat( p_vout, "zoom", val_list[i].f_float );
                    DisplayMessage( p_vout, _("Zoom mode: %s"), text_list[i] );

                    var_FreeList( count, val_list, text_list );
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
                    DisplayMessage( p_vout, _("Deinterlace off") );
                }
                else
                {
                    var_SetInteger( p_vout, "deinterlace", 1 );

                    char *psz_mode = var_GetString( p_vout, "deinterlace-mode" );
                    vlc_value_t *vlist;
                    char **tlist;
                    size_t count;

                    if( psz_mode && !var_Change( p_vout, "deinterlace-mode", VLC_VAR_GETCHOICES, &count, &vlist, &tlist ) )
                    {
                        const char *psz_text = NULL;
                        for( size_t i = 0; i < count; i++ )
                        {
                            if( !strcmp( vlist[i].psz_string, psz_mode ) )
                            {
                                psz_text = tlist[i];
                                break;
                            }
                        }
                        DisplayMessage( p_vout, "%s (%s)", _("Deinterlace on"),
                                        psz_text ? psz_text : psz_mode );

                        var_FreeStringList( count, vlist, tlist );
                    }
                    free( psz_mode );
                }
            }
            break;
        case ACTIONID_DEINTERLACE_MODE:
            if( p_vout )
            {
                char *psz_mode = var_GetString( p_vout, "deinterlace-mode" );
                vlc_value_t *vlist;
                char **tlist;
                size_t count;

                if( psz_mode && !var_Change( p_vout, "deinterlace-mode", VLC_VAR_GETCHOICES, &count, &vlist, &tlist ))
                {
                    const char *psz_text = NULL;
                    size_t i;

                    for( i = 0; i < count; i++ )
                    {
                        if( !strcmp( vlist[i].psz_string, psz_mode ) )
                        {
                            i++;
                            break;
                        }
                    }
                    if( i == count ) i = 0;
                    psz_text = tlist[i];
                    var_SetString( p_vout, "deinterlace-mode", vlist[i].psz_string );

                    int i_deinterlace = var_GetInteger( p_vout, "deinterlace" );
                    if( i_deinterlace != 0 )
                    {
                      DisplayMessage( p_vout, "%s (%s)", _("Deinterlace on"),
                                      psz_text ? psz_text : psz_mode );
                    }
                    else
                    {
                      DisplayMessage( p_vout, "%s (%s)", _("Deinterlace off"),
                                      psz_text ? psz_text : psz_mode );
                    }

                    var_FreeStringList( count, vlist, tlist );
                }
                free( psz_mode );
            }
            break;

        case ACTIONID_SUBPOS_DOWN:
        case ACTIONID_SUBPOS_UP:
        {
            if( p_input )
            {
                vlc_value_t val;
                vlc_value_t *list;
                size_t count;

                var_Get( p_input, "spu-es", &val );

                var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &count, &list, (char ***)NULL );
                if( count < 1 || val.i_int < 0 )
                {
                    DisplayMessage( p_vout,
                                    _("Subtitle position: no active subtitle") );
                    free(list);
                    break;
                }

                int i_pos;
                if( i_action == ACTIONID_SUBPOS_DOWN )
                    i_pos = var_DecInteger( p_vout, "sub-margin" );
                else
                    i_pos = var_IncInteger( p_vout, "sub-margin" );

                ClearChannels( p_vout, slider_chan );
                DisplayMessage( p_vout, _( "Subtitle position %d px" ), i_pos );
                free(list);
            }
            break;
        }

        case ACTIONID_SUBTITLE_TEXT_SCALE_DOWN:
        case ACTIONID_SUBTITLE_TEXT_SCALE_UP:
        case ACTIONID_SUBTITLE_TEXT_SCALE_NORMAL:
            if( p_vout )
            {
                int i_scale;
                if( i_action == ACTIONID_SUBTITLE_TEXT_SCALE_NORMAL )
                {
                    i_scale = 100;
                }
                else
                {
                    i_scale = var_GetInteger( p_playlist, "sub-text-scale" );
                    unsigned increment = ((i_scale > 100 ? i_scale - 100 : 100 - i_scale) / 25) <= 1 ? 10 : 25;
                    i_scale += ((i_action == ACTIONID_SUBTITLE_TEXT_SCALE_UP) ? 1 : -1) * increment;
                    i_scale -= i_scale % increment;
                    i_scale = VLC_CLIP( i_scale, 25, 500 );
                }
                var_SetInteger( p_playlist, "sub-text-scale", i_scale );
                DisplayMessage( p_vout, _( "Subtitle text scale %d%%" ), i_scale );
            }
            break;

        /* Input + video output */
        case ACTIONID_POSITION:
            if( p_vout && vout_OSDEpg( p_vout, input_GetItem( p_input ) ) )
                DisplayPosition( p_vout, slider_chan, p_input );
            break;

        case ACTIONID_COMBO_VOL_FOV_UP:
            if( b_vrnav )
                DO_ACTION( ACTIONID_VIEWPOINT_FOV_IN );
            else
                DO_ACTION( ACTIONID_VOL_UP );
            break;
        case ACTIONID_COMBO_VOL_FOV_DOWN:
            if( b_vrnav )
                DO_ACTION( ACTIONID_VIEWPOINT_FOV_OUT );
            else
                DO_ACTION( ACTIONID_VOL_DOWN );
            break;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ActionEvent: callback for hotkey actions
 *****************************************************************************/
static int ActionEvent( vlc_object_t *libvlc, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void)libvlc;
    (void)psz_var;
    (void)oldval;

    vlc_mutex_lock( &p_intf->p_sys->lock );
    input_thread_t *p_input = p_sys->p_input ? input_Hold(p_sys->p_input)
                                             : NULL;
    vout_thread_t *p_vout = p_sys->p_vout ? vlc_object_hold( p_sys->p_vout )
                                          : NULL;
    int slider_chan = p_sys->slider_chan;
    bool b_vrnav = p_sys->vrnav.b_can_change;
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    int i_ret = PutAction( p_intf, p_input, p_vout, slider_chan, b_vrnav,
                           newval.i_int );

    if( p_input != NULL )
        input_Release(p_input);
    if( p_vout != NULL )
        vlc_object_release( p_vout );

    return i_ret;
}

static void PlayBookmark( intf_thread_t *p_intf, int i_num )
{
    char *psz_bookmark_name;
    if( asprintf( &psz_bookmark_name, "bookmark%i", i_num ) == -1 )
        return;

    playlist_t *p_playlist = pl_Get( p_intf );
    char *psz_bookmark = var_CreateGetString( p_intf, psz_bookmark_name );

    PL_LOCK;
    playlist_item_t *p_item;
    ARRAY_FOREACH( p_item, p_playlist->items )
    {
        char *psz_uri = input_item_GetURI( p_item->p_input );
        if( !strcmp( psz_bookmark, psz_uri ) )
        {
            free( psz_uri );
            playlist_ViewPlay( p_playlist, NULL, p_item );
            break;
        }
        else
            free( psz_uri );
    }
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
        config_PutPsz( psz_bookmark_name, psz_uri);
        msg_Info( p_intf, "setting playlist bookmark %i to %s", i_num, psz_uri);
    }

    free( psz_uri );
    free( psz_bookmark_name );
}

static void DisplayPosition( vout_thread_t *p_vout, int slider_chan,
                             input_thread_t *p_input )
{
    char psz_duration[MSTRTIME_MAX_SIZE];
    char psz_time[MSTRTIME_MAX_SIZE];

    if( p_vout == NULL ) return;

    ClearChannels( p_vout, slider_chan );

    int64_t t = SEC_FROM_VLC_TICK(var_GetInteger( p_input, "time" ));
    int64_t l = SEC_FROM_VLC_TICK(var_GetInteger( p_input, "length" ));

    secstotimestr( psz_time, t );

    if( l > 0 )
    {
        secstotimestr( psz_duration, l );
        DisplayMessage( p_vout, "%s / %s", psz_time, psz_duration );
    }
    else if( t > 0 )
    {
        DisplayMessage( p_vout, "%s", psz_time );
    }

    if( var_GetBool( p_vout, "fullscreen" ) )
    {
        vlc_value_t pos;
        var_Get( p_input, "position", &pos );
        vout_OSDSlider( p_vout, slider_chan,
                        pos.f_float * 100, OSD_HOR_SLIDER );
    }
}

static void DisplayVolume( vout_thread_t *p_vout, int slider_chan, float vol )
{
    if( p_vout == NULL )
        return;
    ClearChannels( p_vout, slider_chan );

    if( var_GetBool( p_vout, "fullscreen" ) )
        vout_OSDSlider( p_vout, slider_chan,
                        lroundf(vol * 100.f), OSD_VERT_SLIDER );
    DisplayMessage( p_vout, _( "Volume %ld%%" ), lroundf(vol * 100.f) );
}

static void DisplayRate( vout_thread_t *p_vout, float f_rate )
{
    DisplayMessage( p_vout, _("Speed: %.2fx"), (double) f_rate );
}

static float AdjustRateFine( vlc_object_t *p_obj, const int i_dir )
{
    const float f_rate_min = INPUT_RATE_MIN;
    const float f_rate_max = INPUT_RATE_MAX;
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

static void ClearChannels( vout_thread_t *p_vout, int slider_chan )
{
    if( p_vout )
    {
        vout_FlushSubpictureChannel( p_vout, VOUT_SPU_CHANNEL_OSD );
        vout_FlushSubpictureChannel( p_vout, slider_chan );
    }
}
