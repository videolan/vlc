/*****************************************************************************
 * smb.c: SMB input module
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#ifdef _WIN32
#   include <fcntl.h>
#   include <sys/stat.h>
#   include <io.h>
#   define smbc_open(a,b,c) vlc_open(a,b,c)
#   define smbc_fstat(a,b) _fstati64(a,b)
#   define smbc_read read
#   define smbc_lseek _lseeki64
#   define smbc_close close
#else
#   include <libsmbclient.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define USER_TEXT N_("SMB user name")
#define USER_LONGTEXT N_("User name that will " \
    "be used for the connection.")
#define PASS_TEXT N_("SMB password")
#define PASS_LONGTEXT N_("Password that will be " \
    "used for the connection.")
#define DOMAIN_TEXT N_("SMB domain")
#define DOMAIN_LONGTEXT N_("Domain/Workgroup that " \
    "will be used for the connection.")

#define SMB_HELP N_("Samba (Windows network shares) input")
vlc_module_begin ()
    set_shortname( "SMB" )
    set_description( N_("SMB input") )
    set_help(SMB_HELP)
    set_capability( "access", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_string( "smb-user", NULL, USER_TEXT, USER_LONGTEXT,
                false )
    add_password( "smb-pwd", NULL, PASS_TEXT,
                  PASS_LONGTEXT, false )
    add_string( "smb-domain", NULL, DOMAIN_TEXT,
                DOMAIN_LONGTEXT, false )
    add_shortcut( "smb" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read( access_t *, uint8_t *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
    int i_smb;
};

#ifdef _WIN32
static void Win32AddConnection( access_t *, char *, char *, char *, char * );
#else
static void smb_auth( const char *srv, const char *shr, char *wg, int wglen,
                      char *un, int unlen, char *pw, int pwlen )
{
    VLC_UNUSED(srv);
    VLC_UNUSED(shr);
    VLC_UNUSED(wg);
    VLC_UNUSED(wglen);
    VLC_UNUSED(un);
    VLC_UNUSED(unlen);
    VLC_UNUSED(pw);
    VLC_UNUSED(pwlen);
    //wglen = unlen = pwlen = 0;
}
#endif

/****************************************************************************
 * Open: connect to smb server and ask for file
 ****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    struct stat  filestat;
    char         *psz_location, *psz_uri;
    char         *psz_user = NULL, *psz_pwd = NULL, *psz_domain = NULL;
    int          i_ret;
    int          i_smb;

    /* Parse input URI
     * [[[domain;]user[:password@]]server[/share[/path[/file]]]] */
    psz_location = strchr( p_access->psz_location, '/' );
    if( !psz_location )
    {
        msg_Err( p_access, "invalid SMB URI: smb://%s", psz_location );
        return VLC_EGENERIC;
    }
    else
    {
        char *psz_tmp = strdup( p_access->psz_location );
        char *psz_parser;

        psz_tmp[ psz_location - p_access->psz_location ] = 0;
        psz_location = p_access->psz_location;
        psz_parser = strchr( psz_tmp, '@' );
        if( psz_parser )
        {
            /* User info is there */
            *psz_parser = 0;
            psz_location = p_access->psz_location + (psz_parser - psz_tmp) + 1;

            psz_parser = strchr( psz_tmp, ':' );
            if( psz_parser )
            {
                /* Password found */
                psz_pwd = strdup( psz_parser+1 );
                *psz_parser = 0;
            }

            psz_parser = strchr( psz_tmp, ';' );
            if( psz_parser )
            {
                /* Domain found */
                *psz_parser = 0; psz_parser++;
                psz_domain = strdup( psz_tmp );
            }
            else psz_parser = psz_tmp;

            psz_user = strdup( psz_parser );
        }

        free( psz_tmp );
    }

    /* Build an SMB URI
     * smb://[[[domain;]user[:password@]]server[/share[/path[/file]]]] */

    if( !psz_user ) psz_user = var_InheritString( p_access, "smb-user" );
    if( psz_user && !*psz_user ) { free( psz_user ); psz_user = NULL; }
    if( !psz_pwd ) psz_pwd = var_InheritString( p_access, "smb-pwd" );
    if( psz_pwd && !*psz_pwd ) { free( psz_pwd ); psz_pwd = NULL; }
    if( !psz_domain ) psz_domain = var_InheritString( p_access, "smb-domain" );
    if( psz_domain && !*psz_domain ) { free( psz_domain ); psz_domain = NULL; }

#ifdef _WIN32
    if( psz_user )
        Win32AddConnection( p_access, psz_location, psz_user, psz_pwd, psz_domain);
    i_ret = asprintf( &psz_uri, "//%s", psz_location );
#else
    if( psz_user )
        i_ret = asprintf( &psz_uri, "smb://%s%s%s%s%s@%s",
                          psz_domain ? psz_domain : "", psz_domain ? ";" : "",
                          psz_user, psz_pwd ? ":" : "",
                          psz_pwd ? psz_pwd : "", psz_location );
    else
        i_ret = asprintf( &psz_uri, "smb://%s", psz_location );
#endif

    free( psz_user );
    free( psz_pwd );
    free( psz_domain );

    if( i_ret == -1 )
        return VLC_ENOMEM;

#ifndef _WIN32
    if( smbc_init( smb_auth, 0 ) )
    {
        free( psz_uri );
        return VLC_EGENERIC;
    }
#endif

/*
** some version of glibc defines open as a macro, causing havoc
** with other macros using 'open' under the hood, such as the
** following one:
*/
#if defined(smbc_open) && defined(open)
# undef open
#endif
    if( (i_smb = smbc_open( psz_uri, O_RDONLY, 0 )) < 0 )
    {
        msg_Err( p_access, "open failed for '%s' (%m)",
                 p_access->psz_location );
        free( psz_uri );
        return VLC_EGENERIC;
    }

    /* Init p_access */
    STANDARD_READ_ACCESS_INIT;

    i_ret = smbc_fstat( i_smb, &filestat );
    if( i_ret )
    {
        errno = i_ret;
        msg_Err( p_access, "stat failed (%m)" );
    }
    else
        p_access->info.i_size = filestat.st_size;

    free( psz_uri );

    p_sys->i_smb = i_smb;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    smbc_close( p_sys->i_smb );
    free( p_sys );
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int64_t      i_ret;

    if( i_pos >= INT64_MAX )
        return VLC_EGENERIC;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

    i_ret = smbc_lseek( p_sys->i_smb, i_pos, SEEK_SET );
    if( i_ret == -1 )
    {
        msg_Err( p_access, "seek failed (%m)" );
        return VLC_EGENERIC;
    }

    p_access->info.b_eof = false;
    p_access->info.i_pos = i_ret;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_access->info.b_eof ) return 0;

    i_read = smbc_read( p_sys->i_smb, p_buffer, i_len );
    if( i_read < 0 )
    {
        msg_Err( p_access, "read failed (%m)" );
        return -1;
    }

    if( i_read == 0 ) p_access->info.b_eof = true;
    else if( i_read > 0 ) p_access->info.i_pos += i_read;

    return i_read;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
    case ACCESS_CAN_SEEK:
    case ACCESS_CAN_FASTSEEK:
    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE:
        *va_arg( args, bool* ) = true;
        break;

    case ACCESS_GET_PTS_DELAY:
        *va_arg( args, int64_t * ) = INT64_C(1000)
            * var_InheritInteger( p_access, "network-caching" );
        break;

    case ACCESS_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    case ACCESS_GET_TITLE_INFO:
    case ACCESS_SET_TITLE:
    case ACCESS_SET_SEEKPOINT:
    case ACCESS_SET_PRIVATE_ID_STATE:
    case ACCESS_GET_CONTENT_TYPE:
        return VLC_EGENERIC;

    default:
        msg_Warn( p_access, "unimplemented query in control" );
        return VLC_EGENERIC;

    }

    return VLC_SUCCESS;
}

#ifdef _WIN32
static void Win32AddConnection( access_t *p_access, char *psz_path,
                                char *psz_user, char *psz_pwd,
                                char *psz_domain )
{
    char psz_remote[MAX_PATH], psz_server[MAX_PATH], psz_share[MAX_PATH];
    NETRESOURCE net_resource;
    DWORD i_result;
    char *psz_parser;
    VLC_UNUSED( psz_domain );

    memset( &net_resource, 0, sizeof(net_resource) );
    net_resource.dwType = RESOURCETYPE_DISK;

    /* Find out server and share names */
    strlcpy( psz_server, psz_path, sizeof( psz_server ) );
    psz_share[0] = 0;
    psz_parser = strchr( psz_path, '/' );
    if( psz_parser )
    {
        char *psz_parser2 = strchr( ++psz_parser, '/' );
        if( psz_parser2 )
            strlcpy( psz_share, psz_parser, sizeof( psz_share ) );
   }

    snprintf( psz_remote, sizeof( psz_remote ), "\\\\%s\\%s", psz_server, psz_share );
    net_resource.lpRemoteName = psz_remote;

    i_result = WNetAddConnection2( &net_resource, psz_pwd, psz_user, 0 );

    if( i_result != NO_ERROR )
    {
        msg_Dbg( p_access, "connected to %s", psz_remote );
    }
    else if( i_result != ERROR_ALREADY_ASSIGNED &&
             i_result != ERROR_DEVICE_ALREADY_REMEMBERED )
    {
        msg_Dbg( p_access, "already connected to %s", psz_remote );
    }
    else
    {
        msg_Dbg( p_access, "failed to connect to %s", psz_remote );
    }
}
#endif // _WIN32
