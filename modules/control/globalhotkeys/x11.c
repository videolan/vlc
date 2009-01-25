/*****************************************************************************
 * x11.c: Global-Hotkey X11 handling for vlc
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
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/XF86keysym.h>
#include <poll.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_keys.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( _("Global Hotkeys") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_HOTKEYS );
    set_description( _("Global Hotkeys interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct
{
    KeyCode     i_x11;
    unsigned    i_modifier;
    int         i_action;
} hotkey_mapping_t;

struct intf_sys_t
{
    vlc_thread_t thread;
    Display *p_display;

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

    Display *p_display = XOpenDisplay( NULL );
    if( !p_display )
        return VLC_EGENERIC;

    p_intf->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
    {
        XCloseDisplay( p_display );
        return VLC_ENOMEM;
    }
    p_sys->p_display = p_display;
    Mapping( p_intf );
    Register( p_intf );

    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        Unregister( p_intf );
        XCloseDisplay( p_display );
        free( p_sys->p_map );
        free( p_sys );
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
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
    XCloseDisplay( p_sys->p_display );
    free( p_sys->p_map );

    free( p_sys );
}

/*****************************************************************************
 * Thread:
 *****************************************************************************/
static unsigned GetModifier( Display *p_display, KeySym sym )
{
    static const unsigned pi_mask[8] = {
        ShiftMask, LockMask, ControlMask, Mod1Mask,
        Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
    };

    const KeyCode key = XKeysymToKeycode( p_display, sym );
    if( key == 0 )
        return 0;

    XModifierKeymap *p_map = XGetModifierMapping( p_display );
    if( !p_map )
        return 0;

    unsigned i_mask = 0;
    for( int i = 0; i < 8 * p_map->max_keypermod; i++ )
    {
        if( p_map->modifiermap[i] == key )
        {
            i_mask = pi_mask[i / p_map->max_keypermod];
            break;
        }
    }

    XFreeModifiermap( p_map );
    return i_mask;
}
static unsigned GetX11Modifier( Display *p_display, unsigned i_vlc )
{
    unsigned i_mask = 0;

    if( i_vlc & KEY_MODIFIER_ALT )
        i_mask |= GetModifier( p_display, XK_Alt_L ) |
                  GetModifier( p_display, XK_Alt_R );
    if( i_vlc & KEY_MODIFIER_CTRL )
        i_mask |= GetModifier( p_display, XK_Control_L ) |
                  GetModifier( p_display, XK_Control_R );
    if( i_vlc & KEY_MODIFIER_SHIFT )
        i_mask |= GetModifier( p_display, XK_Shift_L ) |
                  GetModifier( p_display, XK_Shift_R );
    return i_mask;
}

/* FIXME this table is also used by the vout */
static const struct
{
    KeySym   i_x11;
    unsigned i_vlc;

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
static KeySym GetX11Key( unsigned i_vlc )
{
    for( int i = 0; x11keys_to_vlckeys[i].i_vlc != 0; i++ )
    {
        if( x11keys_to_vlckeys[i].i_vlc == i_vlc )
            return x11keys_to_vlckeys[i].i_x11;
    }

    char psz_key[2];
    psz_key[0] = i_vlc;
    psz_key[1] = '\0';
    return XStringToKeysym( psz_key );
}

static void Mapping( intf_thread_t *p_intf )
{
    static const KeySym p_x11_modifier_ignored[] = {
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

        const KeyCode key = XKeysymToKeycode( p_sys->p_display,
                                              GetX11Key( i_vlc_key & ~KEY_MODIFIER ) );
        const unsigned i_modifier = GetX11Modifier( p_sys->p_display, i_vlc_key & KEY_MODIFIER );

        for( int j = 0; j < sizeof(p_x11_modifier_ignored)/sizeof(*p_x11_modifier_ignored); j++ )
        {
            const unsigned i_ignored = GetModifier( p_sys->p_display, p_x11_modifier_ignored[j] );
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
        hotkey_mapping_t *p_map = &p_sys->p_map[i];
        XGrabKey( p_sys->p_display, p_map->i_x11, p_map->i_modifier,
                  DefaultRootWindow( p_sys->p_display ), True, GrabModeAsync, GrabModeAsync );
    }
}
static void Unregister( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    for( int i = 0; i < p_sys->i_map; i++ )
    {
        hotkey_mapping_t *p_map = &p_sys->p_map[i];
        XUngrabKey( p_sys->p_display, p_map->i_x11, p_map->i_modifier,
                    DefaultRootWindow( p_sys->p_display ) );
    }
}

static void *Thread( void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    Display *p_display = p_sys->p_display;

    int canc = vlc_savecancel();

    if( !p_display )
        return NULL;

    /* */
    XFlush( p_display );

    /* */
    int fd = ConnectionNumber( p_display );
    for( ;; )
    {
        /* Wait for x11 event */
        vlc_restorecancel( canc );
        struct pollfd fds = { .fd = fd, .events = POLLIN, };
        if( poll( &fds, 1, -1 ) < 0 )
        {
            if( errno != EINTR )
                break;
            continue;
        }
        canc = vlc_savecancel();

        while( XPending( p_display ) > 0 )
        {
            XEvent e;

            XNextEvent( p_display, &e );
            if( e.type != KeyPress )
                continue;

            for( int i = 0; i < p_sys->i_map; i++ )
            {
                hotkey_mapping_t *p_map = &p_sys->p_map[i];

                if( p_map->i_x11 == e.xkey.keycode &&
                    p_map->i_modifier == e.xkey.state )
                {
                    var_SetInteger( p_intf->p_libvlc, "key-action", p_map->i_action );
                    break;
                }
            }
        }
    }

    return NULL;
}

