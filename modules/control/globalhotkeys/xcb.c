/*****************************************************************************
 * xcb.c: Global-Hotkey X11 using xcb handling for vlc
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_keys.h>
#include <ctype.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <poll.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
    set_shortname( N_("Global Hotkeys") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_HOTKEYS )
    set_description( N_("Global Hotkeys interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end()

typedef struct
{
    xcb_keycode_t i_x11;
    unsigned      i_modifier;
    int           i_action;
} hotkey_mapping_t;

struct intf_sys_t
{
    vlc_thread_t thread;

    xcb_connection_t  *p_connection;
    xcb_window_t      root;
    xcb_key_symbols_t *p_symbols;

    int              i_map;
    hotkey_mapping_t *p_map;
};

static void Mapping( intf_thread_t *p_intf );
static void Register( intf_thread_t *p_intf );
static void Unregister( intf_thread_t *p_intf );
static void *Thread( void *p_data );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;

    p_intf->p_sys = p_sys = calloc( 1, sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    char *psz_display = var_CreateGetNonEmptyString( p_intf, "x11-display" );

    int i_screen_default;
    p_sys->p_connection = xcb_connect( psz_display, &i_screen_default );
    free( psz_display );

    if( !p_sys->p_connection )
        goto error;

    /* Get the root windows of the default screen */
    const xcb_setup_t* xcbsetup = xcb_get_setup( p_sys->p_connection );
    if( !xcbsetup )
        goto error;
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator( xcbsetup );
    for( int i = 0; i < i_screen_default; i++ )
    {
        if( !iter.rem )
            break;
        xcb_screen_next( &iter );
    }
    if( !iter.rem )
        goto error;
    p_sys->root = iter.data->root;

    /* */
    p_sys->p_symbols = xcb_key_symbols_alloc( p_sys->p_connection ); // FIXME
    if( !p_sys->p_symbols )
        goto error;

    Mapping( p_intf );
    Register( p_intf );

    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        Unregister( p_intf );
        free( p_sys->p_map );
        goto error;
    }
    return VLC_SUCCESS;

error:
    if( p_sys->p_symbols )
        xcb_key_symbols_free( p_sys->p_symbols );
    if( p_sys->p_connection )
        xcb_disconnect( p_sys->p_connection );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    Unregister( p_intf );
    free( p_sys->p_map );

    xcb_key_symbols_free( p_sys->p_symbols );
    xcb_disconnect( p_sys->p_connection );
    free( p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static unsigned GetModifier( xcb_connection_t *p_connection, xcb_key_symbols_t *p_symbols, xcb_keysym_t sym )
{
    static const unsigned pi_mask[8] = {
        XCB_MOD_MASK_SHIFT, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_CONTROL, XCB_MOD_MASK_1,
        XCB_MOD_MASK_2, XCB_MOD_MASK_3, XCB_MOD_MASK_4, XCB_MOD_MASK_5
    };

    const xcb_keycode_t key = xcb_key_symbols_get_keycode( p_symbols, sym );
    if( key == 0 )
        return 0;

    xcb_get_modifier_mapping_cookie_t r = xcb_get_modifier_mapping( p_connection );
    xcb_get_modifier_mapping_reply_t *p_map = xcb_get_modifier_mapping_reply( p_connection, r, NULL );
    if( !p_map )
        return 0;

    xcb_keycode_t *p_keycode = xcb_get_modifier_mapping_keycodes( p_map );
    if( !p_keycode )
        return 0;

    unsigned i_mask = 0;
    for( int i = 0; i < 8; i++ )
    {
        for( int j = 0; j < p_map->keycodes_per_modifier; j++ )
        {
            if( p_keycode[i * p_map->keycodes_per_modifier + j] == key )
                i_mask = pi_mask[i];
        }
    }

    free( p_map ); // FIXME to check
    return i_mask;
}
static unsigned GetX11Modifier( xcb_connection_t *p_connection, xcb_key_symbols_t *p_symbols, unsigned i_vlc )
{
    unsigned i_mask = 0;

    if( i_vlc & KEY_MODIFIER_ALT )
        i_mask |= GetModifier( p_connection, p_symbols, XK_Alt_L ) |
                  GetModifier( p_connection, p_symbols, XK_Alt_R );
    if( i_vlc & KEY_MODIFIER_CTRL )
        i_mask |= GetModifier( p_connection, p_symbols, XK_Control_L ) |
                  GetModifier( p_connection, p_symbols, XK_Control_R );
    if( i_vlc & KEY_MODIFIER_SHIFT )
        i_mask |= GetModifier( p_connection, p_symbols, XK_Shift_L ) |
                  GetModifier( p_connection, p_symbols, XK_Shift_R );
    return i_mask;
}

/* FIXME this table is also used by the vout */
static const struct
{
    xcb_keysym_t i_x11;
    unsigned     i_vlc;

} x11keys_to_vlckeys[] =
{
    { XK_F1, KEY_F1 }, { XK_F2, KEY_F2 }, { XK_F3, KEY_F3 }, { XK_F4, KEY_F4 },
    { XK_F5, KEY_F5 }, { XK_F6, KEY_F6 }, { XK_F7, KEY_F7 }, { XK_F8, KEY_F8 },
    { XK_F9, KEY_F9 }, { XK_F10, KEY_F10 }, { XK_F11, KEY_F11 },
    { XK_F12, KEY_F12 },

    { XK_Return, KEY_ENTER },
    { XK_KP_Enter, KEY_ENTER },
    { XK_space, KEY_SPACE },
    { XK_Escape, KEY_ESC },

    { XK_Menu, KEY_MENU },
    { XK_Left, KEY_LEFT },
    { XK_Right, KEY_RIGHT },
    { XK_Up, KEY_UP },
    { XK_Down, KEY_DOWN },

    { XK_Home, KEY_HOME },
    { XK_End, KEY_END },
    { XK_Page_Up, KEY_PAGEUP },
    { XK_Page_Down, KEY_PAGEDOWN },

    { XK_Insert, KEY_INSERT },
    { XK_Delete, KEY_DELETE },
    { XF86XK_AudioNext, KEY_MEDIA_NEXT_TRACK},
    { XF86XK_AudioPrev, KEY_MEDIA_PREV_TRACK},
    { XF86XK_AudioMute, KEY_VOLUME_MUTE },
    { XF86XK_AudioLowerVolume, KEY_VOLUME_DOWN },
    { XF86XK_AudioRaiseVolume, KEY_VOLUME_UP },
    { XF86XK_AudioPlay, KEY_MEDIA_PLAY_PAUSE },
    { XF86XK_AudioPause, KEY_MEDIA_PLAY_PAUSE },

    { 0, 0 }
};
static xcb_keysym_t GetX11Key( unsigned i_vlc )
{
    for( int i = 0; x11keys_to_vlckeys[i].i_vlc != 0; i++ )
    {
        if( x11keys_to_vlckeys[i].i_vlc == i_vlc )
            return x11keys_to_vlckeys[i].i_x11;
    }

    /* Copied from xcb, it seems that xcb use ascii code for ascii characters */
    if( isascii( i_vlc ) )
        return i_vlc;

    return XK_VoidSymbol;
}

static void Mapping( intf_thread_t *p_intf )
{
    static const xcb_keysym_t p_x11_modifier_ignored[] = {
        0,
        XK_Num_Lock,
        XK_Scroll_Lock,
        XK_Caps_Lock,
    };

    intf_sys_t *p_sys = p_intf->p_sys;

    p_sys->i_map = 0;
    p_sys->p_map = NULL;

    /* Registering of Hotkeys */
    for( struct hotkey *p_hotkey = p_intf->p_libvlc->p_hotkeys;
            p_hotkey->psz_action != NULL;
            p_hotkey++ )
    {
        char *psz_hotkey;
        if( asprintf( &psz_hotkey, "global-%s", p_hotkey->psz_action ) < 0 )
            break;

        const int i_vlc_action = p_hotkey->i_action;
        const int i_vlc_key = config_GetInt( p_intf, psz_hotkey );

        free( psz_hotkey );

        if( !i_vlc_key )
            continue;

        const xcb_keycode_t key = xcb_key_symbols_get_keycode( p_sys->p_symbols, GetX11Key( i_vlc_key & ~KEY_MODIFIER ) );
        const unsigned i_modifier = GetX11Modifier( p_sys->p_connection, p_sys->p_symbols, i_vlc_key & KEY_MODIFIER );

        for( int j = 0; j < sizeof(p_x11_modifier_ignored)/sizeof(*p_x11_modifier_ignored); j++ )
        {
            const unsigned i_ignored = GetModifier( p_sys->p_connection, p_sys->p_symbols, p_x11_modifier_ignored[j] );
            if( j != 0 && i_ignored == 0x00)
                continue;

            hotkey_mapping_t *p_map_old = p_sys->p_map;
            p_sys->p_map = realloc( p_sys->p_map, sizeof(*p_sys->p_map) * (p_sys->i_map+1) );
            if( !p_sys->p_map )
            {
                p_sys->p_map = p_map_old;
                break;
            }
            hotkey_mapping_t *p_map = &p_sys->p_map[p_sys->i_map++];

            p_map->i_x11 = key;
            p_map->i_modifier = i_modifier|i_ignored;
            p_map->i_action = i_vlc_action;
        }
    }
}

static void Register( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    for( int i = 0; i < p_sys->i_map; i++ )
    {
        const hotkey_mapping_t *p_map = &p_sys->p_map[i];
        xcb_grab_key( p_sys->p_connection, true, p_sys->root,
                      p_map->i_modifier, p_map->i_x11,
                      XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC );
    }
}
static void Unregister( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    for( int i = 0; i < p_sys->i_map; i++ )
    {
        const hotkey_mapping_t *p_map = &p_sys->p_map[i];
        xcb_ungrab_key( p_sys->p_connection, p_map->i_x11, p_sys->root, p_map->i_modifier );
    }
}

static void *Thread( void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    xcb_connection_t *p_connection = p_sys->p_connection;

    int canc = vlc_savecancel();

    /* */
    xcb_flush( p_connection );

    /* */
    int fd = xcb_get_file_descriptor( p_connection );
    for( ;; )
    {
        /* Wait for x11 event */
        vlc_restorecancel( canc );
        struct pollfd fds = { .fd = fd, .events = POLLIN, };
        if( poll( &fds, 1, -1 ) < 0 )
        {
            if( errno != EINTR )
                break;
            canc = vlc_savecancel();
            continue;
        }
        canc = vlc_savecancel();

        xcb_generic_event_t *p_event;
        while( ( p_event = xcb_poll_for_event( p_connection ) ) )
        {
            if( ( p_event->response_type & 0x7f ) != XCB_KEY_PRESS )
            {
                free( p_event );
                continue;
            }

            xcb_key_press_event_t *e = (xcb_key_press_event_t *)p_event;

            for( int i = 0; i < p_sys->i_map; i++ )
            {
                hotkey_mapping_t *p_map = &p_sys->p_map[i];

                if( p_map->i_x11 == e->detail &&
                    p_map->i_modifier == e->state )
                {
                    var_SetInteger( p_intf->p_libvlc, "key-action", p_map->i_action );
                    break;
                }
            }
            free( p_event );
        }
    }

    return NULL;
}

