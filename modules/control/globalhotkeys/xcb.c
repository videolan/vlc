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
#include <errno.h>

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
    xcb_keycode_t *p_keys;
    unsigned      i_modifier;
    uint32_t      i_vlc;
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

static bool Mapping( intf_thread_t *p_intf );
static void Register( intf_thread_t *p_intf );
static void *Thread( void *p_data );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;
    int ret = VLC_EGENERIC;

    p_intf->p_sys = p_sys = calloc( 1, sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    int i_screen_default;
    p_sys->p_connection = xcb_connect( NULL, &i_screen_default );

    if( xcb_connection_has_error( p_sys->p_connection ) )
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

    if( !Mapping( p_intf ) )
    {
        ret = VLC_SUCCESS;
        p_intf->p_sys = NULL; /* for Close() */
        goto error;
    }
    Register( p_intf );

    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        if( p_sys->p_map )
        {
            free( p_sys->p_map->p_keys );
            free( p_sys->p_map );
        }
        goto error;
    }
    return VLC_SUCCESS;

error:
    if( p_sys->p_symbols )
        xcb_key_symbols_free( p_sys->p_symbols );
    xcb_disconnect( p_sys->p_connection );
    free( p_sys );
    return ret;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    if( !p_sys )
        return; /* if we were running disabled */

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    if( p_sys->p_map )
    {
        free( p_sys->p_map->p_keys );
        free( p_sys->p_map );
    }
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
        XCB_MOD_MASK_SHIFT, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_CONTROL,
        XCB_MOD_MASK_1, XCB_MOD_MASK_2, XCB_MOD_MASK_3,
        XCB_MOD_MASK_4, XCB_MOD_MASK_5
    };

    if( sym == 0 )
        return 0; /* no modifier */

    xcb_get_modifier_mapping_cookie_t r =
            xcb_get_modifier_mapping( p_connection );
    xcb_get_modifier_mapping_reply_t *p_map =
            xcb_get_modifier_mapping_reply( p_connection, r, NULL );
    if( !p_map )
        return 0;

    xcb_keycode_t *p_keys = xcb_key_symbols_get_keycode( p_symbols, sym );
    if( !p_keys )
        goto end;

    int i = 0;
    bool no_modifier = true;
    while( p_keys[i] != XCB_NO_SYMBOL )
    {
        if( p_keys[i] != 0 )
        {
            no_modifier = false;
            break;
        }
        i++;
    }

    if( no_modifier )
        goto end;

    xcb_keycode_t *p_keycode = xcb_get_modifier_mapping_keycodes( p_map );
    if( !p_keycode )
        goto end;

    for( int i = 0; i < 8; i++ )
        for( int j = 0; j < p_map->keycodes_per_modifier; j++ )
            for( int k = 0; p_keys[k] != XCB_NO_SYMBOL; k++ )
                if( p_keycode[i*p_map->keycodes_per_modifier + j] == p_keys[k])
                {
                    free( p_keys );
                    free( p_map );
                    return pi_mask[i];
                }

end:
    free( p_keys );
    free( p_map ); // FIXME to check
    return 0;
}


static unsigned GetX11Modifier( xcb_connection_t *p_connection,
        xcb_key_symbols_t *p_symbols, unsigned i_vlc )
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
#include "../../video_output/xcb/xcb_keysym.h"
    { 0, 0 }
};
static xcb_keysym_t GetX11Key( unsigned i_vlc )
{
    /* X11 and VLC use ASCII for printable ASCII characters */
    if( i_vlc >= 32 && i_vlc <= 127 )
        return i_vlc;

    for( int i = 0; x11keys_to_vlckeys[i].i_vlc != 0; i++ )
    {
        if( x11keys_to_vlckeys[i].i_vlc == i_vlc )
            return x11keys_to_vlckeys[i].i_x11;
    }

    return XK_VoidSymbol;
}

static bool Mapping( intf_thread_t *p_intf )
{
    static const xcb_keysym_t p_x11_modifier_ignored[] = {
        0,
        XK_Num_Lock,
        XK_Scroll_Lock,
        XK_Caps_Lock,
    };

    intf_sys_t *p_sys = p_intf->p_sys;
    bool active = false;

    p_sys->i_map = 0;
    p_sys->p_map = NULL;

    /* Registering of Hotkeys */
    for( const struct hotkey *p_hotkey = p_intf->p_libvlc->p_hotkeys;
            p_hotkey->psz_action != NULL;
            p_hotkey++ )
    {
        char varname[12 + strlen( p_hotkey->psz_action )];
        sprintf( varname, "global-key-%s", p_hotkey->psz_action );

        char *key = var_InheritString( p_intf, varname );
        if( key == NULL )
            continue;

        uint_fast32_t i_vlc_key = vlc_str2keycode( key );
        free( key );
        if( i_vlc_key == KEY_UNSET )
            continue;

        xcb_keycode_t *p_keys = xcb_key_symbols_get_keycode(
                p_sys->p_symbols, GetX11Key( i_vlc_key & ~KEY_MODIFIER ) );
        if( !p_keys )
            continue;

        const unsigned i_modifier = GetX11Modifier( p_sys->p_connection,
                p_sys->p_symbols, i_vlc_key & KEY_MODIFIER );

        const size_t max = sizeof(p_x11_modifier_ignored) /
                sizeof(*p_x11_modifier_ignored);
        for( unsigned int i = 0; i < max; i++ )
        {
            const unsigned i_ignored = GetModifier( p_sys->p_connection,
                    p_sys->p_symbols, p_x11_modifier_ignored[i] );
            if( i != 0 && i_ignored == 0)
                continue;

            hotkey_mapping_t *p_map_old = p_sys->p_map;
            p_sys->p_map = realloc( p_sys->p_map,
                    sizeof(*p_sys->p_map) * (p_sys->i_map+1) );
            if( !p_sys->p_map )
            {
                p_sys->p_map = p_map_old;
                break;
            }
            hotkey_mapping_t *p_map = &p_sys->p_map[p_sys->i_map++];

            p_map->p_keys = p_keys;
            p_map->i_modifier = i_modifier|i_ignored;
            p_map->i_vlc = i_vlc_key;
            active = true;
        }
    }
    return active;
}

static void Register( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    for( int i = 0; i < p_sys->i_map; i++ )
    {
        const hotkey_mapping_t *p_map = &p_sys->p_map[i];
        for( int j = 0; p_map->p_keys[j] != XCB_NO_SYMBOL; j++ )
        {
            xcb_grab_key( p_sys->p_connection, true, p_sys->root,
                          p_map->i_modifier, p_map->p_keys[j],
                          XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC );
        }
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

                for( int j = 0; p_map->p_keys[j] != XCB_NO_SYMBOL; j++ )
                    if( p_map->p_keys[j] == e->detail &&
                        p_map->i_modifier == e->state )
                    {
                        var_SetInteger( p_intf->p_libvlc, "global-key-pressed",
                                        p_map->i_vlc );
                        goto done;
                    }
            }
        done:
            free( p_event );
        }
    }

    return NULL;
}

