/*****************************************************************************
 * intf.c: Generic lua interface functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_fs.h>

#include "vlc.h"
#include "libs.h"

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static void *Run( void * );

static const char * const ppsz_intf_options[] = { "intf", "config", NULL };

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct intf_sys_t
{
    char *psz_filename;
    lua_State *L;
    vlc_thread_t thread;
    vlclua_dtable_t dtable;
};
/*****************************************************************************
 *
 *****************************************************************************/

static char *MakeConfig( intf_thread_t *p_intf, const char *name )
{
    char *psz_config = NULL;

    if( !strcmp( name, "http" ) )
    {
        char *psz_http_src = var_InheritString( p_intf, "http-src" );
        bool b_http_index = var_InheritBool( p_intf, "http-index" );
        if( psz_http_src )
        {
            char *psz_esc = config_StringEscape( psz_http_src );

            if( asprintf( &psz_config, "http={dir='%s',no_index=%s}", psz_esc,
                          b_http_index ? "true" : "false" ) == -1 )
                psz_config = NULL;
            free( psz_esc );
            free( psz_http_src );
        }
        else
        {
            if( asprintf( &psz_config, "http={no_index=%s}",
                          b_http_index ? "true" : "false" ) == -1 )
                psz_config = NULL;
        }
    }
    else if( !strcmp( name, "telnet" ) )
    {
        char *psz_host = var_InheritString( p_intf, "telnet-host" );
        if( !strcmp( psz_host, "*console" ) )
            ;
        else
        {
            vlc_url_t url;
            vlc_UrlParse( &url, psz_host );
            unsigned i_port = var_InheritInteger( p_intf, "telnet-port" );
            if ( url.i_port != 0 )
            {
                if ( i_port == TELNETPORT_DEFAULT )
                    i_port = url.i_port;
                else if ( url.i_port != i_port )
                    msg_Warn( p_intf, "ignoring port %d (using %d)",
                              url.i_port, i_port );
            }

            char *psz_esc_host = config_StringEscape( url.psz_host );
            free( psz_host );
            vlc_UrlClean( &url );

            if( asprintf( &psz_host, "telnet://%s:%d",
                          psz_esc_host ? psz_esc_host : "", i_port ) == -1 )
                psz_host = NULL;
            free( psz_esc_host );
        }

        char *psz_passwd = var_InheritString( p_intf, "telnet-password" );

        char *psz_esc_passwd = config_StringEscape( psz_passwd );

        if( asprintf( &psz_config, "telnet={host='%s',password='%s'}",
                      psz_host, psz_esc_passwd ) == -1 )
            psz_config = NULL;

        free( psz_esc_passwd );
        free( psz_passwd );
        free( psz_host );
    }

    return psz_config;
}

static char *StripPasswords( const char *psz_config )
{
    unsigned n = 0;
    const char *p = psz_config;
    while ((p = strstr(p, "password=")) != NULL)
    {
        n++;
        p++;
    }
    if (n == 0)
        return strdup(psz_config);

    char *psz_log = malloc(strlen(psz_config) + n * strlen("******") + 1);
    if (psz_log == NULL)
        return NULL;
    psz_log[0] = '\0';

    for (p = psz_config; ; )
    {
        const char *pwd = strstr(p, "password=");
        if (pwd == NULL)
        {
            /* Copy the last, ending bit */
            strcat(psz_log, p);
            break;
        }
        pwd += strlen("password=");

        char delim[3] = ",}";
        if (*pwd == '\'' || *pwd == '"')
        {
            delim[0] = *pwd++;
            delim[1] = '\0';
        }

        strncat(psz_log, p, pwd - p);
        strcat(psz_log, "******");

        /* Advance to the delimiter at the end of the password */
        p = pwd - 1;
        do
        {
            p = strpbrk(p + 1, delim);
            if (p == NULL)
                /* Oops, unbalanced quotes or brackets */
                return psz_log;
        }
        while (*(p - 1) == '\\');
    }
    return psz_log;
}

static const luaL_Reg p_reg[] = { { NULL, NULL } };

static int Start_LuaIntf( vlc_object_t *p_this, const char *name )
{
    if( lua_Disabled( p_this ) )
        return VLC_EGENERIC;

    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    struct vlc_logger *logger = p_intf->obj.logger;
    lua_State *L;
    char *namebuf = NULL;

    config_ChainParse( p_intf, "lua-", ppsz_intf_options, p_intf->p_cfg );

    if( name == NULL )
    {
        namebuf = var_InheritString( p_this, "lua-intf" );
        if( unlikely(namebuf == NULL) )
            return VLC_EGENERIC;
        name = namebuf;
    }

    intf_sys_t *p_sys = malloc( sizeof(*p_sys) );
    if( unlikely(p_sys == NULL) )
    {
        free( namebuf );
        return VLC_ENOMEM;
    }
    p_intf->p_sys = p_sys;

    p_intf->obj.logger = vlc_LogHeaderCreate( logger, name );
    if( p_intf->obj.logger == NULL )
    {
        p_intf->obj.logger = logger;
        free( p_sys );
        free( namebuf );
        return VLC_ENOMEM;
    }

    p_sys->psz_filename = vlclua_find_file( "intf", name );
    if( !p_sys->psz_filename )
    {
        msg_Err( p_intf, "Couldn't find lua interface script \"%s\".",
                 name );
        goto error;
    }
    msg_Dbg( p_intf, "Found lua interface script: %s", p_sys->psz_filename );

    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_intf, "Could not create new Lua State" );
        goto error;
    }

    vlclua_set_this( L, p_intf );
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(p_intf);
    vlclua_set_playlist_internal(L, playlist);

    luaL_openlibs( L );

    /* register our functions */
    luaL_register_namespace( L, "vlc", p_reg );

    /* register submodules */
    luaopen_config( L );
    luaopen_httpd( L );
    luaopen_input( L );
    luaopen_msg( L );
    luaopen_misc( L );
    if( vlclua_fd_init( L, &p_sys->dtable ) )
    {
        lua_close( L );
        goto error;
    }
    luaopen_object( L );
    luaopen_osd( L );
    luaopen_playlist( L );
    luaopen_stream( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_video( L );
    luaopen_vlm( L );
    luaopen_volume( L );
    luaopen_gettext( L );
    luaopen_xml( L );
    luaopen_equalizer( L );
    luaopen_vlcio( L );
    luaopen_errno( L );
    luaopen_rand( L );
    luaopen_rd( L );
#if defined(_WIN32) && !VLC_WINSTORE_APP
    luaopen_win( L );
#endif

    /* clean up */
    lua_pop( L, 1 );

    /* Setup the module search path */
    if( vlclua_add_modules_path( L, p_sys->psz_filename ) )
    {
        msg_Warn( p_intf, "Error while setting the module search path for %s",
                  p_sys->psz_filename );
        lua_close( L );
        goto error;
    }

    /*
     * Get the lua-config string.
     * If the string is empty, try with the old http-* or telnet-* options
     * and build the right configuration line
     */
    bool b_config_set = false;
    char *psz_config = var_InheritString( p_intf, "lua-config" );
    if( !psz_config )
        psz_config = MakeConfig( p_intf, name );

    if( psz_config )
    {
        char *psz_buffer;
        if( asprintf( &psz_buffer, "config={%s}", psz_config ) != -1 )
        {
            char *psz_log = StripPasswords( psz_buffer );
            if( psz_log != NULL )
            {
                msg_Dbg( p_intf, "Setting config variable: %s", psz_log );
                free( psz_log );
            }

            if( luaL_dostring( L, psz_buffer ) == 1 )
                msg_Err( p_intf, "Error while parsing \"lua-config\"." );
            free( psz_buffer );
            lua_getglobal( L, "config" );
            if( lua_istable( L, -1 ) )
            {
                if( !strcmp( name, "cli" ) )
                {
                    lua_getfield( L, -1, "rc" );
                    if( lua_istable( L, -1 ) )
                    {
                        /* msg_Warn( p_intf, "The `rc' lua interface script "
                                          "was renamed `cli', please update "
                                          "your configuration!" ); */
                        lua_setfield( L, -2, "cli" );
                    }
                    else
                        lua_pop( L, 1 );
                }
                lua_getfield( L, -1, name );
                if( lua_istable( L, -1 ) )
                {
                    lua_setglobal( L, "config" );
                    b_config_set = true;
                }
            }
        }
        free( psz_config );
    }

    if( !b_config_set )
    {
        lua_newtable( L );
        lua_setglobal( L, "config" );
    }

    /* Wrapper for legacy telnet config */
    if ( !strcmp( name, "telnet" ) )
    {
        /* msg_Warn( p_intf, "The `telnet' lua interface script was replaced "
                          "by `cli', please update your configuration!" ); */

        char *wrapped_file = vlclua_find_file( "intf", "cli" );
        if( !wrapped_file )
        {
            msg_Err( p_intf, "Couldn't find lua interface script \"cli\", "
                             "needed by telnet wrapper" );
            lua_close( p_sys->L );
            goto error;
        }
        lua_pushstring( L, wrapped_file );
        lua_setglobal( L, "wrapped_file" );
        free( wrapped_file );
    }

    p_sys->L = L;

    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        vlclua_fd_cleanup( &p_sys->dtable );
        lua_close( p_sys->L );
        goto error;
    }
    free( namebuf );
    return VLC_SUCCESS;
error:
    free( p_sys->psz_filename );
    free( p_sys );
    vlc_LogDestroy( p_intf->obj.logger );
    p_intf->obj.logger = logger;
    free( namebuf );
    return VLC_EGENERIC;
}

void Close_LuaIntf( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlclua_fd_interrupt( &p_sys->dtable );
    vlc_join( p_sys->thread, NULL );

    lua_close( p_sys->L );
    vlclua_fd_cleanup( &p_sys->dtable );
    free( p_sys->psz_filename );
    free( p_sys );
    vlc_LogDestroy( p_intf->obj.logger );
}

static void *Run( void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;
    lua_State *L = p_sys->L;

    if( vlclua_dofile( VLC_OBJECT(p_intf), L, p_sys->psz_filename ) )
    {
        msg_Err( p_intf, "Error loading script %s: %s", p_sys->psz_filename,
                 lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
    }
    return NULL;
}

int Open_LuaIntf( vlc_object_t *p_this )
{
    return Start_LuaIntf( p_this, NULL );
}

int Open_LuaHTTP( vlc_object_t *p_this )
{
    return Start_LuaIntf( p_this, "http" );
}

int Open_LuaCLI( vlc_object_t *p_this )
{
    return Start_LuaIntf( p_this, "cli" );
}

int Open_LuaTelnet( vlc_object_t *p_this )
{
    char *pw = var_CreateGetNonEmptyString( p_this, "telnet-password" );
    if( pw == NULL )
    {
        msg_Err( p_this, "password not configured" );
        msg_Info( p_this, "Please specify the password in the preferences." );
        return VLC_EGENERIC;
    }
    free( pw );
    return Start_LuaIntf( p_this, "telnet" );
}
