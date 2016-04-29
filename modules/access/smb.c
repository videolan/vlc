/*****************************************************************************
 * smb.c: SMB input module
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
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

#include <assert.h>
#include <errno.h>
#ifdef _WIN32
#   include <fcntl.h>
#   include <sys/stat.h>
#   include <io.h>
#   define smbc_open(a,b,c) vlc_open(a,b,c)
#   define smbc_stat(a,b) _stati64(a,b)
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
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_keystore.h>

#include "smb_common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SMB_HELP N_("Samba (Windows network shares) input")
vlc_module_begin ()
    set_shortname( "SMB" )
    set_description( N_("SMB input") )
    set_help(SMB_HELP)
    set_capability( "access", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_string( "smb-user", NULL, SMB_USER_TEXT, SMB_USER_LONGTEXT,
                false )
    add_password( "smb-pwd", NULL, SMB_PASS_TEXT,
                  SMB_PASS_LONGTEXT, false )
    add_string( "smb-domain", NULL, SMB_DOMAIN_TEXT,
                SMB_DOMAIN_LONGTEXT, false )
    add_shortcut( "smb" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read( access_t *, uint8_t *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );
#ifndef _WIN32
static int DirRead( access_t *, input_item_node_t * );
static int DirControl( access_t *, int, va_list );
#endif

struct access_sys_t
{
    int i_smb;
    uint64_t size;
    vlc_url_t url;
};

#ifdef _WIN32
static void Win32AddConnection( access_t *, const char *, const char *, const char *, const char *, const char * );
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

/* Build an SMB URI
 * smb://[[[domain;]user[:password@]]server[/share[/path[/file]]]] */
static int smb_get_uri( access_t *p_access, char **ppsz_uri,
                        const char *psz_domain,
                        const char *psz_user, const char *psz_pwd,
                        const char *psz_server, const char *psz_share_path,
                        const char *psz_name )
{
    assert(psz_server);
#define PSZ_SHARE_PATH_OR_NULL psz_share_path ? psz_share_path : ""
#define PSZ_NAME_OR_NULL psz_name ? "/" : "", psz_name ? psz_name : ""
#ifdef _WIN32
    if( psz_user )
        Win32AddConnection( p_access, psz_server, psz_share_path,
                            psz_user, psz_pwd, psz_domain );
    return asprintf( ppsz_uri, "//%s%s%s%s", psz_server, PSZ_SHARE_PATH_OR_NULL,
                     PSZ_NAME_OR_NULL );
#else
    (void) p_access;
    if( psz_user )
        return asprintf( ppsz_uri, "smb://%s%s%s%s%s@%s%s%s%s",
                         psz_domain ? psz_domain : "", psz_domain ? ";" : "",
                         psz_user, psz_pwd ? ":" : "",
                         psz_pwd ? psz_pwd : "", psz_server,
                         PSZ_SHARE_PATH_OR_NULL, PSZ_NAME_OR_NULL );
    else
        return asprintf( ppsz_uri, "smb://%s%s%s%s", psz_server,
                         PSZ_SHARE_PATH_OR_NULL, PSZ_NAME_OR_NULL );
#endif
}

/****************************************************************************
 * Open: connect to smb server and ask for file
 ****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    struct stat  filestat;
    vlc_url_t    url;
    vlc_credential credential;
    char         *psz_decoded_path = NULL, *psz_uri = NULL,
                 *psz_var_domain = NULL;
    int          i_ret;
    int          i_smb;
    uint64_t     i_size;
    bool         b_is_dir = false;

#ifndef _WIN32
    if( smbc_init( smb_auth, 0 ) )
        return VLC_EGENERIC;
#endif

/*
** some version of glibc defines open as a macro, causing havoc
** with other macros using 'open' under the hood, such as the
** following one:
*/
#if defined(smbc_open) && defined(open)
# undef open
#endif

    vlc_UrlParse( &url, p_access->psz_url );
    if( url.psz_path )
    {
        psz_decoded_path = vlc_uri_decode_duplicate( url.psz_path );
        if( !psz_decoded_path )
        {
            vlc_UrlClean( &url );
            return VLC_EGENERIC;
        }
    }

    vlc_credential_init( &credential, &url );
    psz_var_domain = var_InheritString( p_access, "smb-domain" );
    credential.psz_realm = psz_var_domain;
    vlc_credential_get( &credential, p_access, "smb-user", "smb-pwd",
                        NULL, NULL );
    for (;;)
    {
        if( smb_get_uri( p_access, &psz_uri, credential.psz_realm,
                         credential.psz_username, credential.psz_password,
                         url.psz_host, psz_decoded_path, NULL ) == -1 )
        {
            vlc_credential_clean( &credential );
            free(psz_var_domain);
            free( psz_decoded_path );
            vlc_UrlClean( &url );
            return VLC_ENOMEM;
        }

        if( ( i_ret = smbc_stat( psz_uri, &filestat ) ) && errno == EACCES )
        {
            errno = 0;
            if( vlc_credential_get( &credential, p_access, "smb-user", "smb-pwd",
                                    SMB_LOGIN_DIALOG_TITLE,
                                    SMB_LOGIN_DIALOG_TEXT, url.psz_host) )
                continue;
        }

        /* smbc_stat fails with servers or shares. Assume they are directory */
        if( i_ret || S_ISDIR( filestat.st_mode ) )
            b_is_dir = true;
        break;
    }

    vlc_credential_store( &credential, p_access );
    vlc_credential_clean( &credential );
    free(psz_var_domain);
    free( psz_decoded_path );

    /* Init p_access */
    access_InitFields( p_access );
    p_sys =
    p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) );
    if( !p_sys )
    {
        free( psz_uri );
        vlc_UrlClean( &url );
        return VLC_ENOMEM;
    }

    if( b_is_dir )
    {
#ifdef _WIN32
        free( p_sys );
        free( psz_uri );
        vlc_UrlClean( &url );
        return VLC_EGENERIC;
#else
        p_sys->url = url;
        p_access->pf_readdir = DirRead;
        p_access->pf_control = DirControl;
        i_smb = smbc_opendir( psz_uri );
        i_size = 0;
#endif
    }
    else
    {
        ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek );
        i_smb = smbc_open( psz_uri, O_RDONLY, 0 );
        i_size = filestat.st_size;
    }
    free( psz_uri );

    if( i_smb < 0 )
    {
        msg_Err( p_access, "open failed for '%s' (%s)",
                 p_access->psz_location, vlc_strerror_c(errno) );
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->size = i_size;
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

    vlc_UrlClean( &p_sys->url );

#ifndef _WIN32
    if( p_access->pf_readdir )
        smbc_closedir( p_sys->i_smb );
    else
#endif
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
        msg_Err( p_access, "seek failed (%s)", vlc_strerror_c(errno) );
        return VLC_EGENERIC;
    }

    p_access->info.b_eof = false;

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
        msg_Err( p_access, "read failed (%s)", vlc_strerror_c(errno) );
        p_access->info.b_eof = true;
        return -1;
    }

    if( i_read == 0 ) p_access->info.b_eof = true;

    return i_read;
}

#ifndef _WIN32
/*****************************************************************************
 * DirRead:
 *****************************************************************************/
static int DirRead (access_t *p_access, input_item_node_t *p_node )
{
    access_sys_t *p_sys = p_access->p_sys;
    struct smbc_dirent *p_entry;
    int i_ret = VLC_SUCCESS;

    struct access_fsdir fsdir;
    access_fsdir_init( &fsdir, p_access, p_node );

    while( i_ret == VLC_SUCCESS && ( p_entry = smbc_readdir( p_sys->i_smb ) ) )
    {
        char *psz_uri;
        const char *psz_server = p_sys->url.psz_host;
        const char *psz_path = p_sys->url.psz_path;
        const char *psz_name = p_entry->name;
        int i_type;

        switch( p_entry->smbc_type )
        {
        case SMBC_SERVER:
        case SMBC_WORKGROUP:
            psz_server = p_sys->url.psz_host;
            psz_path = NULL;
            psz_name = NULL;
        case SMBC_FILE_SHARE:
        case SMBC_DIR:
            i_type = ITEM_TYPE_DIRECTORY;
            break;
        case SMBC_FILE:
            i_type = ITEM_TYPE_FILE;
            break;
        default:
        case SMBC_PRINTER_SHARE:
        case SMBC_COMMS_SHARE:
        case SMBC_IPC_SHARE:
        case SMBC_LINK:
            continue;
        }

        char *psz_encoded_name = NULL;
        if( psz_name != NULL
         && ( psz_encoded_name = vlc_uri_encode( psz_name ) ) == NULL )
        {
            i_ret = VLC_ENOMEM;
            break;
        }
        if( smb_get_uri( p_access, &psz_uri, NULL, NULL, NULL,
                         psz_server, psz_path, psz_encoded_name ) < 0 )
        {
            free(psz_encoded_name);
            i_ret = VLC_ENOMEM;
            break;
        }
        free(psz_encoded_name);
        i_ret = access_fsdir_additem( &fsdir, psz_uri, p_entry->name,
                                      i_type, ITEM_NET );
        free( psz_uri );
    }

    access_fsdir_finish( &fsdir, i_ret == VLC_SUCCESS );

    return i_ret;
}

static int DirControl( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
    case ACCESS_IS_DIRECTORY:
        *va_arg( args, bool * ) = true; /* might loop */
        break;
    default:
        return access_vaDirectoryControlHelper( p_access, i_query, args );
    }

    return VLC_SUCCESS;
}
#endif

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
    case ACCESS_CAN_SEEK:
    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE:
        *va_arg( args, bool* ) = true;
        break;

    case ACCESS_CAN_FASTSEEK:
        *va_arg( args, bool* ) = false;
        break;

    case ACCESS_GET_SIZE:
        if( p_access->pf_readdir != NULL )
            return VLC_EGENERIC;
        *va_arg( args, uint64_t * ) = p_access->p_sys->size;
        break;

    case ACCESS_GET_PTS_DELAY:
        *va_arg( args, int64_t * ) = INT64_C(1000)
            * var_InheritInteger( p_access, "network-caching" );
        break;

    case ACCESS_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#ifdef _WIN32
static void Win32AddConnection( access_t *p_access, const char *psz_server,
                                const char *psz_share, const char *psz_user,
                                const char *psz_pwd, const char *psz_domain )
{
    char psz_remote[MAX_PATH];
    NETRESOURCE net_resource;
    DWORD i_result;
    VLC_UNUSED( psz_domain );

    memset( &net_resource, 0, sizeof(net_resource) );
    net_resource.dwType = RESOURCETYPE_DISK;

    if (psz_share)
        psz_share = psz_share + 1; /* skip first '/' */
    else
        psz_share = "";

    snprintf( psz_remote, sizeof( psz_remote ), "\\\\%s\\%s", psz_server, psz_share );
    /* remove trailings '/' */
    char *psz_delim = strchr( psz_remote, '/' );
    if( psz_delim )
        *psz_delim = '\0';

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
