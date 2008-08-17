/*****************************************************************************
 * smb.c: SMB input module
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_access.h>

#ifdef WIN32
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#   ifdef HAVE_SYS_STAT_H
#       include <sys/stat.h>
#   endif
#   include <io.h>
#   define smbc_open(a,b,c) open(a,b,c)
#   define stat _stati64
#   define smbc_fstat(a,b) _fstati64(a,b)
#   define smbc_read read
#   define smbc_lseek _lseeki64
#   define smbc_close close
#else
#   include <libsmbclient.h>
#   define USE_CTX 1
#endif

#include <errno.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for SMB streams. This " \
    "value should be set in milliseconds." )
#define USER_TEXT N_("SMB user name")
#define USER_LONGTEXT N_("User name that will " \
    "be used for the connection.")
#define PASS_TEXT N_("SMB password")
#define PASS_LONGTEXT N_("Password that will be " \
    "used for the connection.")
#define DOMAIN_TEXT N_("SMB domain")
#define DOMAIN_LONGTEXT N_("Domain/Workgroup that " \
    "will be used for the connection.")

vlc_module_begin();
    set_shortname( "SMB" );
    set_description( N_("SMB input") );
    set_capability( "access", 0 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "smb-caching", 2 * DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, true );
    add_string( "smb-user", NULL, NULL, USER_TEXT, USER_LONGTEXT,
                false );
    add_string( "smb-pwd", NULL, NULL, PASS_TEXT,
                PASS_LONGTEXT, false );
    add_string( "smb-domain", NULL, NULL, DOMAIN_TEXT,
                DOMAIN_LONGTEXT, false );
    add_shortcut( "smb" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read( access_t *, uint8_t *, size_t );
static int Seek( access_t *, int64_t );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
#ifdef USE_CTX
    SMBCCTX *p_smb;
    SMBCFILE *p_file;
#else
    int i_smb;
#endif
};

#ifdef WIN32
static void Win32AddConnection( access_t *, char *, char *, char *, char * );
#else
static void smb_auth( const char *srv, const char *shr, char *wg, int wglen,
                      char *un, int unlen, char *pw, int pwlen )
{
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
    char         *psz_path, *psz_uri;
    char         *psz_user = 0, *psz_pwd = 0, *psz_domain = 0;
    int          i_ret;

#ifdef USE_CTX
    SMBCCTX      *p_smb;
    SMBCFILE     *p_file;
#else
    int          i_smb;
#endif

    /* Parse input URI
     * [[[domain;]user[:password@]]server[/share[/path[/file]]]] */

    psz_path = strchr( p_access->psz_path, '/' );
    if( !psz_path )
    {
        msg_Err( p_access, "invalid SMB URI: smb://%s", psz_path );
        return VLC_EGENERIC;
    }
    else
    {
        char *psz_tmp = strdup( p_access->psz_path );
        char *psz_parser;

        psz_tmp[ psz_path - p_access->psz_path ] = 0;
        psz_path = p_access->psz_path;
        psz_parser = strchr( psz_tmp, '@' );
        if( psz_parser )
        {
            /* User info is there */
            *psz_parser = 0;
            psz_path = p_access->psz_path + (psz_parser - psz_tmp) + 1;

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

    if( !psz_user ) psz_user = var_CreateGetString( p_access, "smb-user" );
    if( psz_user && !*psz_user ) { free( psz_user ); psz_user = 0; }
    if( !psz_pwd ) psz_pwd = var_CreateGetString( p_access, "smb-pwd" );
    if( psz_pwd && !*psz_pwd ) { free( psz_pwd ); psz_pwd = 0; }
    if( !psz_domain ) psz_domain = var_CreateGetString( p_access, "smb-domain" );
    if( psz_domain && !*psz_domain ) { free( psz_domain ); psz_domain = 0; }

#ifdef WIN32
    if( psz_user )
        Win32AddConnection( p_access, psz_path, psz_user, psz_pwd, psz_domain);
    i_ret = asprintf( &psz_uri, "//%s", psz_path );
#else
    if( psz_user )
        i_ret = asprintf( &psz_uri, "smb://%s%s%s%s%s@%s",
                          psz_domain ? psz_domain : "", psz_domain ? ";" : "",
                          psz_user, psz_pwd ? ":" : "",
                          psz_pwd ? psz_pwd : "", psz_path );
    else
        i_ret = asprintf( &psz_uri, "smb://%s", psz_path );
#endif

    free( psz_user );
    free( psz_pwd );
    free( psz_domain );

    if( i_ret == -1 )
        return VLC_ENOMEM;

#ifdef USE_CTX
    if( !(p_smb = smbc_new_context()) )
    {
        free( psz_uri );
        return VLC_ENOMEM;
    }
    p_smb->debug = 1;
    p_smb->callbacks.auth_fn = smb_auth;

    if( !smbc_init_context( p_smb ) )
    {
        msg_Err( p_access, "cannot initialize context (%m)" );
        smbc_free_context( p_smb, 1 );
        free( psz_uri );
        return VLC_EGENERIC;
    }

    if( !(p_file = (p_smb->open)( p_smb, psz_uri, O_RDONLY, 0 )) )
    {
        msg_Err( p_access, "open failed for '%s' (%m)",
                 p_access->psz_path );
        smbc_free_context( p_smb, 1 );
        free( psz_uri );
        return VLC_EGENERIC;
    }

    /* Init p_access */
    STANDARD_READ_ACCESS_INIT;

    i_ret = p_smb->fstat( p_smb, p_file, &filestat );
    if( i_ret ) msg_Err( p_access, "stat failed (%m)" );
    else p_access->info.i_size = filestat.st_size;
#else

#ifndef WIN32
    if( smbc_init( smb_auth, 1 ) )
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
                 p_access->psz_path );
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
    else p_access->info.i_size = filestat.st_size;
#endif

    free( psz_uri );

#ifdef USE_CTX
    p_sys->p_smb = p_smb;
    p_sys->p_file = p_file;
#else
    p_sys->i_smb = i_smb;
#endif

    /* Update default_pts to a suitable value for smb access */
    var_Create( p_access, "smb-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

#ifdef USE_CTX
#  ifndef HAVE__SMBCCTX_CLOSE_FN
    p_sys->p_smb->close( p_sys->p_smb, p_sys->p_file );
#  else
    p_sys->p_smb->close_fn( p_sys->p_smb, p_sys->p_file );
#  endif
    smbc_free_context( p_sys->p_smb, 1 );
#else
    smbc_close( p_sys->i_smb );
#endif

    free( p_sys );
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int64_t      i_ret;

    if( i_pos < 0 ) return VLC_EGENERIC;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

#ifdef USE_CTX
    i_ret = p_sys->p_smb->lseek(p_sys->p_smb, p_sys->p_file, i_pos, SEEK_SET);
#else
    i_ret = smbc_lseek( p_sys->i_smb, i_pos, SEEK_SET );
#endif
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

#ifdef USE_CTX
    i_read = p_sys->p_smb->read(p_sys->p_smb, p_sys->p_file, p_buffer, i_len);
#else
    i_read = smbc_read( p_sys->i_smb, p_buffer, i_len );
#endif
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
    bool   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
    case ACCESS_CAN_SEEK:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;
    case ACCESS_CAN_FASTSEEK:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;
    case ACCESS_CAN_PAUSE:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;
    case ACCESS_CAN_CONTROL_PACE:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;

    case ACCESS_GET_MTU:
        pi_int = (int*)va_arg( args, int * );
        *pi_int = 0;
        break;

    case ACCESS_GET_PTS_DELAY:
        pi_64 = (int64_t*)va_arg( args, int64_t * );
        *pi_64 = (int64_t)var_GetInteger( p_access, "smb-caching" ) * 1000;
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

#ifdef WIN32
static void Win32AddConnection( access_t *p_access, char *psz_path,
                                char *psz_user, char *psz_pwd,
                                char *psz_domain )
{
    DWORD (*OurWNetAddConnection2)( LPNETRESOURCE, LPCTSTR, LPCTSTR, DWORD );
    char psz_remote[MAX_PATH], psz_server[MAX_PATH], psz_share[MAX_PATH];
    NETRESOURCE net_resource;
    DWORD i_result;
    char *psz_parser;
    VLC_UNUSED( psz_domain );

    HINSTANCE hdll = LoadLibrary(_T("MPR.DLL"));
    if( !hdll )
    {
        msg_Warn( p_access, "couldn't load mpr.dll" );
        return;
    }

    OurWNetAddConnection2 =
      (void *)GetProcAddress( hdll, _T("WNetAddConnection2A") );
    if( !OurWNetAddConnection2 )
    {
        msg_Warn( p_access, "couldn't find WNetAddConnection2 in mpr.dll" );
        return;
    }

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

    sprintf( psz_remote, "\\\\%s\\%s", psz_server, psz_share );
    net_resource.lpRemoteName = psz_remote;

    i_result = OurWNetAddConnection2( &net_resource, psz_pwd, psz_user, 0 );

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

    FreeLibrary( hdll );
}
#endif // WIN32
