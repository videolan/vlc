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
#   include <windows.h>
#   include <lm.h>
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
#include <vlc_charset.h>

#include "smb_common.h"

struct access_sys_t
{
    int i_smb;
    uint64_t size;
    vlc_url_t url;
};

#ifdef _WIN32
static void Win32AddConnection(stream_t *access, const char *server,
                               const char *share, const char *user,
                               const char *pwd, const char *domain)
{
    NETRESOURCE net_resource;
    char remote_name[MAX_PATH];

    VLC_UNUSED(domain);

    memset(&net_resource, 0, sizeof (net_resource));
    net_resource.dwType = RESOURCETYPE_DISK;

    snprintf(remote_name, sizeof (remote_name), "\\\\%s\\%s", server,
             (share != NULL) ? share + 1 /* skip leading '/' */: "");

    /* remove trailings '/' */
    char *delim = strchr(remote_name, '/');
    if (delim != NULL)
        *delim = '\0';

    const char *msg;
    net_resource.lpRemoteName = remote_name;

    switch (WNetAddConnection2(&net_resource, pwd, user, 0))
    {
        case NO_ERROR:
            msg = "connected to %s";
            break;
        case ERROR_ALREADY_ASSIGNED:
        case ERROR_DEVICE_ALREADY_REMEMBERED:
            msg = "already connected to %s";
            break;
        default:
            msg = "failed to connect to %s";
    }
    msg_Dbg(access, msg, remote_name);
}
#endif // _WIN32

/* Build an SMB URI
 * smb://[[[domain;]user[:password@]]server[/share[/path[/file]]]] */
static int smb_get_uri( stream_t *p_access, char **ppsz_uri,
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

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( stream_t *p_access, uint64_t i_pos )
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

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( stream_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    i_read = smbc_read( p_sys->i_smb, p_buffer, i_len );
    if( i_read < 0 )
    {
        msg_Err( p_access, "read failed (%s)", vlc_strerror_c(errno) );
        i_read = 0;
    }

    return i_read;
}

/*****************************************************************************
 * DirRead:
 *****************************************************************************/
static int DirRead (stream_t *p_access, input_item_node_t *p_node )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret = VLC_SUCCESS;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_access, p_node );

#ifndef _WIN32
    struct smbc_dirent *p_entry;

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
        i_ret = vlc_readdir_helper_additem( &rdh, psz_uri, NULL, p_entry->name,
                                            i_type, ITEM_NET );
        free( psz_uri );
    }
#else
    // Handle share listing from here. Directory browsing is handled by the
    // usual filesystem module.
    SHARE_INFO_1 *p_info;
    DWORD i_share_enum_res;
    DWORD i_nb_elem;
    DWORD i_resume_handle = 0;
    DWORD i_total_elements; // Unused, but needs to be passed
    wchar_t *wpsz_host = ToWide( p_sys->url.psz_host );
    if( wpsz_host == NULL )
        return VLC_ENOMEM;
    do
    {
        i_share_enum_res = NetShareEnum( wpsz_host, 1, (LPBYTE*)&p_info,
                              MAX_PREFERRED_LENGTH, &i_nb_elem,
                              &i_total_elements, &i_resume_handle );
        if( i_share_enum_res == ERROR_SUCCESS ||
            i_share_enum_res == ERROR_MORE_DATA )
        {
            for ( DWORD i = 0; i < i_nb_elem; ++i )
            {
                SHARE_INFO_1 *p_current = p_info + i;
                if( p_current->shi1_type & STYPE_SPECIAL )
                    continue;
                char* psz_name = FromWide( p_current->shi1_netname );
                if( psz_name == NULL )
                {
                    i_ret = VLC_ENOMEM;
                    break;
                }

                char* psz_path;
                if( smb_get_uri( p_access, &psz_path, NULL, NULL, NULL,
                                 p_sys->url.psz_host, p_sys->url.psz_path,
                                 psz_name ) < 0 )
                {
                    free( psz_name );
                    i_ret = VLC_ENOMEM;
                    break;
                }
                // We need to concatenate the scheme before, as the window version
                // of smb_get_uri generates a path (and the other call site needs
                // a path). The path is already prefixed by "//" so we just need
                // to add "file:"
                char* psz_uri;
                if( asprintf( &psz_uri, "file:%s", psz_path ) < 0 )
                {
                    free( psz_name );
                    free( psz_path );
                    i_ret = VLC_ENOMEM;
                    break;
                }
                free( psz_path );

                i_ret = vlc_readdir_helper_additem( &rdh, psz_uri, NULL,
                                    psz_name, ITEM_TYPE_DIRECTORY, ITEM_NET );
                free( psz_name );
                free( psz_uri );
            }
        }
        NetApiBufferFree( p_info );
    } while( i_share_enum_res == ERROR_MORE_DATA && i_ret == VLC_SUCCESS );

    free( wpsz_host );
#endif

    vlc_readdir_helper_finish( &rdh, i_ret == VLC_SUCCESS );

    return i_ret;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    access_sys_t *sys = p_access->p_sys;

    switch( i_query )
    {
    case STREAM_CAN_SEEK:
    case STREAM_CAN_PAUSE:
    case STREAM_CAN_CONTROL_PACE:
        *va_arg( args, bool* ) = true;
        break;

    case STREAM_CAN_FASTSEEK:
        *va_arg( args, bool* ) = false;
        break;

    case STREAM_GET_SIZE:
        if( p_access->pf_readdir != NULL )
            return VLC_EGENERIC;
        *va_arg( args, uint64_t * ) = sys->size;
        break;

    case STREAM_GET_PTS_DELAY:
        *va_arg( args, int64_t * ) = INT64_C(1000)
            * var_InheritInteger( p_access, "network-caching" );
        break;

    case STREAM_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#ifndef _WIN32
static void smb_auth(const char *srv, const char *shr, char *wg, int wglen,
                     char *un, int unlen, char *pw, int pwlen)
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

static int Open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    vlc_url_t url;
    vlc_credential credential;
    char *psz_decoded_path = NULL, *psz_uri = NULL, *psz_var_domain = NULL;
    int fd;
    uint64_t size;
    bool is_dir;

#ifndef _WIN32
    if (smbc_init(smb_auth, 0))
        return VLC_EGENERIC;
#endif

    if (vlc_UrlParseFixup(&url, access->psz_url) != 0)
    {
        vlc_UrlClean(&url);
        return VLC_EGENERIC;
    }

    if (url.psz_path != NULL)
    {
        psz_decoded_path = vlc_uri_decode_duplicate(url.psz_path);
        if (psz_decoded_path == NULL)
        {
            vlc_UrlClean(&url);
            return VLC_EGENERIC;
        }
    }

    vlc_credential_init(&credential, &url);
    psz_var_domain = var_InheritString(access, "smb-domain");
    credential.psz_realm = psz_var_domain;
    vlc_credential_get(&credential, access, "smb-user", "smb-pwd", NULL, NULL);

    for (;;)
    {
        struct stat st;

        if (smb_get_uri(access, &psz_uri, credential.psz_realm,
                        credential.psz_username, credential.psz_password,
                        url.psz_host, psz_decoded_path, NULL ) == -1 )
        {
            vlc_credential_clean(&credential);
            free(psz_var_domain);
            free(psz_decoded_path);
            vlc_UrlClean(&url);
            return VLC_ENOMEM;
        }

        if (smbc_stat(psz_uri, &st) == 0)
        {
            is_dir = S_ISDIR(st.st_mode) != 0;
            size = st.st_size;
            break;
        }

        /* smbc_stat() fails with servers or shares. Assume directory. */
        is_dir = true;
        size = 0;

        if (errno != EACCES)
            break;

        errno = 0;
        if (!vlc_credential_get(&credential, access, "smb-user",
                                "smb-pwd", SMB_LOGIN_DIALOG_TITLE,
                                SMB_LOGIN_DIALOG_TEXT, url.psz_host))
            break;
    }

    vlc_credential_store(&credential, access);
    vlc_credential_clean(&credential);
    free(psz_var_domain);
    free(psz_decoded_path);

    /* Init access */
    access_sys_t *sys = vlc_obj_calloc(obj, 1, sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        free(psz_uri);
        vlc_UrlClean(&url);
        return VLC_ENOMEM;
    }

    access->p_sys = sys;

    if (is_dir)
    {
        sys->url = url;
        access->pf_readdir = DirRead;
        access->pf_control = access_vaDirectoryControlHelper;
#ifndef _WIN32
        fd = smbc_opendir(psz_uri);
        if (fd < 0)
            vlc_UrlClean(&sys->url);
#endif
    }
    else
    {
        access->pf_read = Read;
        access->pf_control = Control;
        access->pf_seek = Seek;
        fd = smbc_open(psz_uri, O_RDONLY, 0);
        vlc_UrlClean(&url);
    }
    free(psz_uri);

#ifndef _WIN32
    if (fd < 0)
    {
        msg_Err(obj, "cannot open %s: %s",
                access->psz_location, vlc_strerror_c(errno));
        return VLC_EGENERIC;
    }
#endif

    sys->size = size;
    sys->i_smb = fd;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    vlc_UrlClean(&sys->url);

#ifndef _WIN32
    if (access->pf_readdir != NULL)
        smbc_closedir(sys->i_smb);
    else
#endif
        smbc_close(sys->i_smb);
}

vlc_module_begin()
    set_shortname("SMB")
    set_description(N_("SMB input"))
    set_help(N_("Samba (Windows network shares) input"))
    set_capability("access", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_string("smb-user", NULL, SMB_USER_TEXT, SMB_USER_LONGTEXT, false)
    add_password("smb-pwd", NULL, SMB_PASS_TEXT, SMB_PASS_LONGTEXT, false)
    add_string("smb-domain", NULL, SMB_DOMAIN_TEXT, SMB_DOMAIN_LONGTEXT, false)
    add_shortcut("smb")
    set_callbacks(Open, Close)
vlc_module_end()
