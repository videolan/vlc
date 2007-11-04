/*****************************************************************************
 * intf.c: Generic lua inteface functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
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
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#include <vlc/vlc.h>
#include <vlc_meta.h>
#include <vlc_charset.h>

#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_osd.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

struct intf_sys_t
{
    char *psz_filename;
    lua_State *L;
};

/*****************************************************************************
 * Internal lua<->vlc utils
 *****************************************************************************/
static inline playlist_t *vlclua_get_playlist_internal( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    return pl_Yield( p_this );
}

static input_thread_t * vlclua_get_input_internal( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    input_thread_t *p_input = p_playlist->p_input;
    if( p_input ) vlc_object_yield( p_input );
    vlc_object_release( p_playlist );
    return p_input;
}

/* FIXME: This is high level logic. Should be implemented in lua */
#define vlclua_var_toggle_or_set(a,b,c) \
        __vlclua_var_toggle_or_set(a,VLC_OBJECT(b),c)
static int __vlclua_var_toggle_or_set( lua_State *L, vlc_object_t *p_obj,
                                       const char *psz_name )
{
    vlc_bool_t b_bool;
    if( lua_gettop( L ) > 1 ) return vlclua_error( L );

    if( lua_gettop( L ) == 0 )
        b_bool = !var_GetBool( p_obj, psz_name );
    else /* lua_gettop( L ) == 1 */
    {
        b_bool = luaL_checkboolean( L, -1 )?VLC_TRUE:VLC_FALSE;
        lua_pop( L, 1 );
    }

    if( b_bool != var_GetBool( p_obj, psz_name ) )
        var_SetBool( p_obj, psz_name, b_bool );

    lua_pushboolean( L, b_bool );
    return 1;
}

/*****************************************************************************
 * Libvlc TODO: move to vlc.c
 *****************************************************************************/
static int vlclua_get_libvlc( lua_State *L )
{
    vlclua_push_vlc_object( L, vlclua_get_this( L )->p_libvlc,
                            NULL );
    return 1;
}

/*****************************************************************************
 * Input handling
 *****************************************************************************/
static int vlclua_get_input( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( p_input )
    {
        vlclua_push_vlc_object( L, p_input, vlclua_gc_release );
    }
    else lua_pushnil( L );
    return 1;
}

static int vlclua_input_info( lua_State *L )
{
    input_thread_t * p_input = vlclua_get_input_internal( L );
    int i_cat;
    int i;
    if( !p_input ) return vlclua_error( L );
    vlc_mutex_lock( &input_GetItem(p_input)->lock );
    i_cat = input_GetItem(p_input)->i_categories;
    lua_createtable( L, 0, i_cat );
    for( i = 0; i < i_cat; i++ )
    {
        info_category_t *p_category = input_GetItem(p_input)->pp_categories[i];
        int i_infos = p_category->i_infos;
        int j;
        lua_pushstring( L, p_category->psz_name );
        lua_createtable( L, 0, i_infos );
        for( j = 0; j < i_infos; j++ )
        {
            info_t *p_info = p_category->pp_infos[j];
            lua_pushstring( L, p_info->psz_name );
            lua_pushstring( L, p_info->psz_value );
            lua_settable( L, -3 );
        }
        lua_settable( L, -3 );
    }
    vlc_object_release( p_input );
    return 1;
}

static int vlclua_is_playing( lua_State *L )
{
    input_thread_t * p_input = vlclua_get_input_internal( L );
    lua_pushboolean( L, !!p_input );
    return 1;
}

static int vlclua_get_title( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        lua_pushnil( L );
    else
    {
        lua_pushstring( L, input_GetItem(p_input)->psz_name );
        vlc_object_release( p_input );
    }
    return 1;
}

/*****************************************************************************
 * Vout control
 *****************************************************************************/
static int vlclua_fullscreen( lua_State *L )
{
    vout_thread_t *p_vout;
    int i_ret;

    input_thread_t * p_input = vlclua_get_input_internal( L );
    if( !p_input ) return vlclua_error( L );

    p_vout = vlc_object_find( p_input, VLC_OBJECT_VOUT, FIND_CHILD );
    if( !p_vout ) return vlclua_error( L );

    i_ret = vlclua_var_toggle_or_set( L, p_vout, "fullscreen" );
    vlc_object_release( p_vout );
    vlc_object_release( p_input );
    return i_ret;
}

static int vlc_osd_icon_from_string( const char *psz_name )
{
    static const struct
    {
        int i_icon;
        const char *psz_name;
    } pp_icons[] =
        { { OSD_PAUSE_ICON, "pause" },
          { OSD_PLAY_ICON, "play" },
          { OSD_SPEAKER_ICON, "speaker" },
          { OSD_MUTE_ICON, "mute" },
          { 0, NULL } };
    int i;
    for( i = 0; pp_icons[i].psz_name; i++ )
    {
        if( !strcmp( psz_name, pp_icons[i].psz_name ) )
            return pp_icons[i].i_icon;
    }
    return 0;
}

static int vlclua_osd_icon( lua_State *L )
{
    const char *psz_icon = luaL_checkstring( L, 1 );
    int i_icon = vlc_osd_icon_from_string( psz_icon );
    int i_chan = luaL_optint( L, 2, DEFAULT_CHAN );
    if( !i_icon )
        return luaL_error( L, "\"%s\" is not a valid osd icon.", psz_icon );
    else
    {
        vlc_object_t *p_this = vlclua_get_this( L );
        vout_OSDIcon( p_this, i_chan, i_icon );
        return 0;
    }
}

static int vlclua_osd_message( lua_State *L )
{
    const char *psz_message = luaL_checkstring( L, 1 );
    int i_chan = luaL_optint( L, 2, DEFAULT_CHAN );
    vlc_object_t *p_this = vlclua_get_this( L );
    vout_OSDMessage( p_this, i_chan, psz_message );
    return 0;
}

static int vlc_osd_slider_type_from_string( const char *psz_name )
{
    static const struct
    {
        int i_type;
        const char *psz_name;
    } pp_types[] =
        { { OSD_HOR_SLIDER, "horizontal" },
          { OSD_VERT_SLIDER, "vertical" },
          { 0, NULL } };
    int i;
    for( i = 0; pp_types[i].psz_name; i++ )
    {
        if( !strcmp( psz_name, pp_types[i].psz_name ) )
            return pp_types[i].i_type;
    }
    return 0;
}

static int vlclua_osd_slider( lua_State *L )
{
    int i_position = luaL_checkint( L, 1 );
    const char *psz_type = luaL_checkstring( L, 2 );
    int i_type = vlc_osd_slider_type_from_string( psz_type );
    int i_chan = luaL_optint( L, 3, DEFAULT_CHAN );
    if( !i_type )
        return luaL_error( L, "\"%s\" is not a valid slider type.",
                           psz_type );
    else
    {
        vlc_object_t *p_this = vlclua_get_this( L );
        vout_OSDSlider( p_this, i_chan, i_position, i_type );
        return 0;
    }
}

static int vlclua_spu_channel_register( lua_State *L )
{
    int i_chan;
    vlc_object_t *p_this = vlclua_get_this( L );
    vout_thread_t *p_vout = vlc_object_find( p_this, VLC_OBJECT_VOUT,
                                             FIND_ANYWHERE );
    if( !p_vout )
        return luaL_error( L, "Unable to find vout." );

    spu_Control( p_vout->p_spu, SPU_CHANNEL_REGISTER, &i_chan );
    vlc_object_release( p_vout );
    lua_pushinteger( L, i_chan );
    return 1;
}

static int vlclua_spu_channel_clear( lua_State *L )
{
    int i_chan = luaL_checkint( L, 1 );
    vlc_object_t *p_this = vlclua_get_this( L );
    vout_thread_t *p_vout = vlc_object_find( p_this, VLC_OBJECT_VOUT,
                                             FIND_ANYWHERE );
    if( !p_vout )
        return luaL_error( L, "Unable to find vout." );

    spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR, i_chan );
    vlc_object_release( p_vout );
    return 0;
}

/*****************************************************************************
 * Playlist control
 *****************************************************************************/
static int vlclua_get_playlist( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    if( p_playlist )
    {
        vlclua_push_vlc_object( L, p_playlist, vlclua_gc_release );
    }
    else lua_pushnil( L );
    return 1;
}

static int vlclua_playlist_prev( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
    return 0;
}

static int vlclua_playlist_next( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
    return 0;
}

static int vlclua_playlist_play( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Play( p_playlist );
    vlc_object_release( p_playlist );
    return 0;
}

static int vlclua_playlist_stop( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
    return 0;
}

static int vlclua_playlist_clear( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Stop( p_playlist ); /* Isn't this already implied by Clear? */
    playlist_Clear( p_playlist, VLC_FALSE );
    vlc_object_release( p_playlist );
    return 0;
}

static int vlclua_playlist_repeat( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "repeat" );
    vlc_object_release( p_playlist );
    return i_ret;
}

static int vlclua_playlist_loop( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "loop" );
    vlc_object_release( p_playlist );
    return i_ret;
}

static int vlclua_playlist_random( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "random" );
    vlc_object_release( p_playlist );
    return i_ret;
}

static int vlclua_playlist_goto( lua_State * L )
{
    /* XXX: logic copied from rc.c ... i'm not sure that it's ok as it
     *      implies knowledge of the playlist internals. */
    playlist_t *p_playlist;
    int i_size;
    playlist_item_t *p_item, *p_parent;

    int i_pos;
    if( lua_gettop( L ) != 1 ) return vlclua_error( L );
    i_pos = luaL_checkint( L, -1 );
    lua_pop( L, 1 );
    if( i_pos <= 0 ) return 0;

    p_playlist = vlclua_get_playlist_internal( L );
    /* The playlist stores 2 times the same item: onelevel & category */
    i_size = p_playlist->items.i_size / 2;

    if( i_pos > i_size )
    {
        vlc_object_release( p_playlist );
        return 0;
    }

    p_item = p_parent = p_playlist->items.p_elems[i_pos*2-1];
    while( p_parent->p_parent )
        p_parent = p_parent->p_parent;
    playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, VLC_TRUE,
                      p_parent, p_item );

    vlc_object_release( p_playlist );
    lua_pushboolean( L, 1 );
    return 1;
}

static int vlclua_playlist_add( lua_State *L )
{
    int i_count;
    vlc_object_t *p_this = vlclua_get_this( L );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    i_count = vlclua_playlist_add_internal( p_this, L, p_playlist,
                                            NULL, VLC_TRUE );
    vlc_object_release( p_playlist );
    lua_pushinteger( L, i_count );
    return 1;
}

static int vlclua_playlist_enqueue( lua_State *L )
{
    int i_count;
    vlc_object_t *p_this = vlclua_get_this( L );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    i_count = vlclua_playlist_add_internal( p_this, L, p_playlist,
                                            NULL, VLC_FALSE );
    vlc_object_release( p_playlist );
    lua_pushinteger( L, i_count );
    return 1;
}

static int vlclua_playlist_get( lua_State *L )
{
    /* TODO: make it possible to get the tree playlist */
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_item_t *p_root;
    int i;
    if( lua_isboolean( L, 1 ) && lua_toboolean( L, 1 ) )
        p_root = p_playlist->p_ml_onelevel; /* media library */
    else
        p_root = p_playlist->p_local_onelevel; /* local/normal playlist */
    lua_createtable( L, p_root->i_children, 0 );
    for( i = 0; i < p_root->i_children; i++ )
    {
        playlist_item_t *p_item = p_root->pp_children[i];
        input_item_t *p_input = p_item->p_input;
        lua_pushinteger( L, i+1 );
        lua_newtable( L );
        lua_pushstring( L, p_input->psz_name );
        lua_setfield( L, -2, "name" );
        lua_pushstring( L, p_input->psz_uri );
        lua_setfield( L, -2, "path" );
        lua_pushnumber( L, ((double)p_input->i_duration)*1e-6 );
        lua_setfield( L, -2, "duration" );
        lua_pushinteger( L, p_input->i_nb_played );
        lua_setfield( L, -2, "nb_played" );
        /* TODO: add (optional) info categories, meta, options, es */
        lua_settable( L, -3 );
    }
    vlc_object_release( p_playlist );
    return 1;
}

static int vlclua_playlist_sort( lua_State *L )
{
    /* allow setting the different sort keys */
    return 0;
}

/* FIXME: split this in 3 different functions? */
static int vlclua_playlist_status( lua_State *L )
{
    intf_thread_t *p_intf = (intf_thread_t *)vlclua_get_this( L );
    playlist_t *p_playlist = pl_Yield( p_intf );
    int i_count = 0;
    lua_settop( L, 0 );
    if( p_playlist->p_input )
    {
        char *psz_uri =
            input_item_GetURI( input_GetItem( p_playlist->p_input ) );
        lua_pushstring( L, psz_uri );
        free( psz_uri );
        lua_pushnumber( L, config_GetInt( p_intf, "volume" ) );
        vlc_mutex_lock( &p_playlist->object_lock );
        switch( p_playlist->status.i_status )
        {
            case PLAYLIST_STOPPED:
                lua_pushstring( L, "stopped" );
                break;
            case PLAYLIST_RUNNING:
                lua_pushstring( L, "running" );
                break;
            case PLAYLIST_PAUSED:
                lua_pushstring( L, "paused" );
                break;
            default:
                lua_pushstring( L, "unknown" );
                break;
        }
        vlc_mutex_unlock( &p_playlist->object_lock );
        i_count += 3;
    }
    vlc_object_release( p_playlist );
    return i_count;
}


static int vlclua_lock_and_wait( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    int b_quit = vlc_object_lock_and_wait( p_this );
    lua_pushboolean( L, b_quit );
    return 1;
}

static int vlclua_signal( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_object_signal( p_this );
    return 0;
}

static int vlclua_mdate( lua_State *L )
{
    lua_pushnumber( L, mdate() );
    return 1;
}

static int vlclua_intf_should_die( lua_State *L )
{
    intf_thread_t *p_intf = (intf_thread_t*)vlclua_get_this( L );
    lua_pushboolean( L, intf_ShouldDie( p_intf ) );
    return 1;
}

static luaL_Reg p_reg[] =
{
    { "input_info", vlclua_input_info },
    { "is_playing", vlclua_is_playing },
    { "get_title", vlclua_get_title },

    { "fullscreen", vlclua_fullscreen },

    { "mdate", vlclua_mdate },

    { "module_command", vlclua_module_command },
    { "libvlc_command", vlclua_libvlc_command },

    { "decode_uri", vlclua_decode_uri },
    { "resolve_xml_special_chars", vlclua_resolve_xml_special_chars },

    { "lock_and_wait", vlclua_lock_and_wait },
    { "signal", vlclua_signal },

    { "version", vlclua_version },
    { "should_die", vlclua_intf_should_die },
    { "quit", vlclua_quit },

    { NULL, NULL }
};

static luaL_Reg p_reg_object[] =
{
    { "input", vlclua_get_input },              /* This is fast */
    { "playlist", vlclua_get_playlist },        /* This is fast */
    { "libvlc", vlclua_get_libvlc },            /* This is fast */

    { "find", vlclua_object_find },             /* This is slow */
    { "find_name", vlclua_object_find_name },   /* This is slow */

    { NULL, NULL }
};

static luaL_Reg p_reg_var[] =
{
    { "get", vlclua_var_get },
    { "get_list", vlclua_var_get_list },
    { "set", vlclua_var_set },
    { "add_callback", vlclua_add_callback },
    { "del_callback", vlclua_del_callback },

    { NULL, NULL }
};

static luaL_Reg p_reg_config[] =
{
    { "get", vlclua_config_get },
    { "set", vlclua_config_set },

    { NULL, NULL }
};

static luaL_Reg p_reg_msg[] =
{
    { "dbg", vlclua_msg_dbg },
    { "warn", vlclua_msg_warn },
    { "err", vlclua_msg_err },
    { "info", vlclua_msg_info },

    { NULL, NULL }
};

static luaL_Reg p_reg_playlist[] =
{
    { "prev", vlclua_playlist_prev },
    { "next", vlclua_playlist_next },
    { "play", vlclua_playlist_play },
    { "stop", vlclua_playlist_stop },
    { "clear", vlclua_playlist_clear },
    { "repeat_", vlclua_playlist_repeat },
    { "loop", vlclua_playlist_loop },
    { "random", vlclua_playlist_random },
    { "goto", vlclua_playlist_goto },
    { "status", vlclua_playlist_status },
    { "add", vlclua_playlist_add },
    { "enqueue", vlclua_playlist_enqueue },
    { "get", vlclua_playlist_get },

    { NULL, NULL }
};

static luaL_Reg p_reg_volume[] =
{
    { "get", vlclua_volume_get },
    { "set", vlclua_volume_set },
    { "up", vlclua_volume_up },
    { "down", vlclua_volume_down },

    { NULL, NULL }
};

static luaL_Reg p_reg_osd[] =
{
    { "icon", vlclua_osd_icon },
    { "message", vlclua_osd_message },
    { "slider", vlclua_osd_slider },
    { "channel_register", vlclua_spu_channel_register },
    { "channel_clear", vlclua_spu_channel_clear },

    { NULL, NULL }
};

static luaL_Reg p_reg_net[] =
{
    { "url_parse", vlclua_url_parse },
    { "listen_tcp", vlclua_net_listen_tcp },
    { "listen_close", vlclua_net_listen_close },
    { "accept", vlclua_net_accept },
    { "close", vlclua_net_close },
    { "send", vlclua_net_send },
    { "recv", vlclua_net_recv },
    { "select", vlclua_net_select },

    { NULL, NULL }
};

static luaL_Reg p_reg_fd[] =
{
/*    { "open", vlclua_fd_open },*/
    { "read", vlclua_fd_read },
    { "write", vlclua_fd_write },

    { "new_fd_set", vlclua_fd_set_new },
    { "fd_clr", vlclua_fd_clr },
    { "fd_isset", vlclua_fd_isset },
    { "fd_set", vlclua_fd_set },
    { "fd_zero", vlclua_fd_zero },

    { NULL, NULL }
};

static luaL_Reg p_reg_vlm[] =
{
    { "new", vlclua_vlm_new },
    { "delete", vlclua_vlm_delete },
    { "execute_command", vlclua_vlm_execute_command },

    { NULL, NULL }
};


static void Run( intf_thread_t *p_intf );

static char *FindFile( intf_thread_t *p_intf, const char *psz_name )
{
    char  *ppsz_dir_list[] = { NULL, NULL, NULL, NULL };
    char **ppsz_dir;
    vlclua_dir_list( VLC_OBJECT(p_intf), "luaintf", ppsz_dir_list );
    for( ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
    {
        char *psz_filename;
        FILE *fp;
        if( asprintf( &psz_filename, "%s"DIR_SEP"%s.lua", *ppsz_dir,
                      psz_name ) < 0 )
        {
            return NULL;
        }
        fp = fopen( psz_filename, "r" );
        if( fp )
        {
            fclose( fp );
            return psz_filename;
        }
        free( psz_filename );
    }
    return NULL;
}

static inline void luaL_register_submodule( lua_State *L, const char *psz_name,
                                            const luaL_Reg *l )
{
    lua_newtable( L );
    luaL_register( L, NULL, l );
    lua_setfield( L, -2, psz_name );
}

static struct
{
    const char *psz_shortcut;
    const char *psz_name;
} pp_shortcuts[] = {
    { "luarc", "rc" },
    /* { "rc", "rc" }, */
    { "luahotkeys", "hotkeys" },
    /* { "hotkeys", "hotkeys" }, */
    { "luatelnet", "telnet" },
    /* { "telnet", "telnet" }, */
    { NULL, NULL } };

static vlc_bool_t WordInList( const char *psz_list, const char *psz_word )
{
    const char *psz_str = strstr( psz_list, psz_word );
    int i_len = strlen( psz_word );
    while( psz_str )
    {
        if( (psz_str == psz_list || *(psz_str-1) == ',' )
         /* it doesn't start in middle of a word */
         /* it doest end in middle of a word */
         && ( psz_str[i_len] == '\0' || psz_str[i_len] == ',' ) )
            return VLC_TRUE;
        psz_str = strstr( psz_str, psz_word );
    }
    return VLC_FALSE;
}

static const char *GetModuleName( intf_thread_t *p_intf )
{
    int i;
    const char *psz_intf;
    if( *p_intf->psz_intf == '$' )
        psz_intf = var_GetString( p_intf, p_intf->psz_intf+1 );
    else
        psz_intf = p_intf->psz_intf;
    for( i = 0; pp_shortcuts[i].psz_name; i++ )
    {
        if( WordInList( psz_intf, pp_shortcuts[i].psz_shortcut ) )
            return pp_shortcuts[i].psz_name;
    }

    return config_GetPsz( p_intf, "lua-intf" );
}

int E_(Open_LuaIntf)( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys;
    lua_State *L;

    const char *psz_name = GetModuleName( p_intf );
    const char *psz_config;
    vlc_bool_t b_config_set = VLC_FALSE;
    if( !psz_name ) psz_name = "dummy";

    p_intf->p_sys = (intf_sys_t*)malloc( sizeof(intf_sys_t*) );
    if( !p_intf->p_sys )
    {
        return VLC_ENOMEM;
    }
    p_sys = p_intf->p_sys;
    p_sys->psz_filename = FindFile( p_intf, psz_name );
    if( !p_sys->psz_filename )
    {
        msg_Err( p_intf, "Couldn't find lua interface script \"%s\".",
                 psz_name );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_intf, "Found lua interface script: %s", p_sys->psz_filename );

    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_intf, "Could not create new Lua State" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    luaL_openlibs( L ); /* FIXME: we don't want to have all the libs */

    /* register our functions */
    luaL_register( L, "vlc", p_reg );
    /* store a pointer to p_intf */
    lua_pushlightuserdata( L, p_intf );
    lua_setfield( L, -2, "private" );
    /* register submodules */
    luaL_register_submodule( L, "object", p_reg_object );
    luaL_register_submodule( L, "var", p_reg_var );
    luaL_register_submodule( L, "config", p_reg_config );
    luaL_register_submodule( L, "msg", p_reg_msg );
    luaL_register_submodule( L, "playlist", p_reg_playlist );
    luaL_register_submodule( L, "volume", p_reg_volume );
    luaL_register_submodule( L, "osd", p_reg_osd );
    luaL_register_submodule( L, "net", p_reg_net );
    luaL_register_submodule( L, "fd", p_reg_fd );
    luaL_register_submodule( L, "vlm", p_reg_vlm );
    /* clean up */
    lua_pop( L, 1 );

    /* <gruik> */
    /* Setup the module search path */
    {
    char *psz_command;
    char *psz_char = strrchr(p_sys->psz_filename,DIR_SEP_CHAR);
    *psz_char = '\0';
    /* FIXME: don't use luaL_dostring */
    if( asprintf( &psz_command,
                  "package.path = \"%s"DIR_SEP"modules"DIR_SEP"?.lua;\"..package.path",
                  p_sys->psz_filename ) < 0 )
        return VLC_EGENERIC;
    *psz_char = DIR_SEP_CHAR;
    if( luaL_dostring( L, psz_command ) )
        return VLC_EGENERIC;
    }
    /* </gruik> */

    psz_config = config_GetPsz( p_intf, "lua-config" );
    if( psz_config && *psz_config )
    {
        char *psz_buffer;
        if( asprintf( &psz_buffer, "config={%s}", psz_config ) != -1 )
        {
            printf("%s\n", psz_buffer);
            if( luaL_dostring( L, psz_buffer ) == 1 )
                msg_Err( p_intf, "Error while parsing \"lua-config\"." );
            free( psz_buffer );
            lua_getglobal( L, "config" );
            if( lua_istable( L, -1 ) )
            {
                lua_getfield( L, -1, psz_name );
                if( lua_istable( L, -1 ) )
                {
                    lua_setglobal( L, "config" );
                    b_config_set = VLC_TRUE;
                }
            }
        }
    }
    if( b_config_set == VLC_FALSE )
    {
        lua_newtable( L );
        lua_setglobal( L, "config" );
    }

    p_sys->L = L;

    p_intf->pf_run = Run;
    p_intf->psz_header = strdup( psz_name ); /* Do I need to clean that up myself in E_(Close_LuaIntf)? */

    return VLC_SUCCESS;
}

void E_(Close_LuaIntf)( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    lua_close( p_intf->p_sys->L );
    free( p_intf->p_sys );
}

static void Run( intf_thread_t *p_intf )
{
    lua_State *L = p_intf->p_sys->L;

    if( luaL_dofile( L, p_intf->p_sys->psz_filename ) )
    {
        msg_Err( p_intf, "Error loading script %s: %s",
                 p_intf->p_sys->psz_filename,
                 lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        p_intf->b_die = VLC_TRUE;
        return;
    }
    p_intf->b_die = VLC_TRUE;
}
