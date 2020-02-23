/*****************************************************************************
 * unc.c: Microsoft Windows Universal Naming Convention input module
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include <lm.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_keystore.h>
#include <vlc_charset.h>

#include "smb_common.h"

typedef struct
{
    int i_smb;
    uint64_t size;
    vlc_url_t url;
} access_sys_t;

static void Win32AddConnection(stream_t *access, const char *server,
                               const char *share, const char *user,
                               const char *pwd)
{
    char *remote_name;

    if (share != NULL)
    {   /* skip leading and remove trailing slashes */
        int slen = strcspn(++share, "/");

        if (asprintf(&remote_name, "\\\\%s\\%.*s", server, slen, share) < 0)
            return;
    }
    else
    {
        if (asprintf(&remote_name, "\\\\%s\\", server) < 0)
            return;
    }

    NETRESOURCE net_resource = {
        .dwType = RESOURCETYPE_DISK,
        .lpRemoteName = ToWide(remote_name),
    };

    free(remote_name);

    const char *msg;
    wchar_t *wpwd  = pwd  ? ToWide(pwd)  : NULL;
    wchar_t *wuser = user ? ToWide(user) : NULL;

    switch (WNetAddConnection2(&net_resource, wpwd, wuser, 0))
    {
        case NO_ERROR:
            msg = "connected to %ls";
            break;
        case ERROR_ALREADY_ASSIGNED:
        case ERROR_DEVICE_ALREADY_REMEMBERED:
            msg = "already connected to %ls";
            break;
        default:
            msg = "failed to connect to %ls";
    }
    msg_Dbg(access, msg, net_resource.lpRemoteName);
    free(net_resource.lpRemoteName);
    free(wpwd);
    free(wuser);
}

/* Build an SMB URI
 * smb://[[[domain;]user[:password@]]server[/share[/path[/file]]]] */
static int smb_get_uri( stream_t *p_access, char **ppsz_uri,
                        const char *psz_user, const char *psz_pwd,
                        const char *psz_server, const char *psz_share_path,
                        const char *psz_name )
{
    assert(psz_server);
#define PSZ_SHARE_PATH_OR_NULL psz_share_path ? psz_share_path : ""
#define PSZ_NAME_OR_NULL psz_name ? "/" : "", psz_name ? psz_name : ""
    if( psz_user )
        Win32AddConnection( p_access, psz_server, psz_share_path,
                            psz_user, psz_pwd );
    return asprintf( ppsz_uri, "//%s%s%s%s", psz_server, PSZ_SHARE_PATH_OR_NULL,
                     PSZ_NAME_OR_NULL );
}

static int Seek( stream_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int64_t      i_ret;

    if( i_pos >= INT64_MAX )
        return VLC_EGENERIC;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

    i_ret = lseek( p_sys->i_smb, i_pos, SEEK_SET );
    if( i_ret == -1 )
    {
        msg_Err( p_access, "seek failed (%s)", vlc_strerror_c(errno) );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static ssize_t Read( stream_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    i_read = read( p_sys->i_smb, p_buffer, i_len );
    if( i_read < 0 )
    {
        msg_Err( p_access, "read failed (%s)", vlc_strerror_c(errno) );
        i_read = 0;
    }

    return i_read;
}

static int DirRead(stream_t *p_access, input_item_node_t *p_node)
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret = VLC_SUCCESS;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_access, p_node );

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
                if( smb_get_uri( p_access, &psz_path, NULL, NULL,
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

    vlc_readdir_helper_finish( &rdh, i_ret == VLC_SUCCESS );

    return i_ret;
}

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
        *va_arg( args, vlc_tick_t * ) = VLC_TICK_FROM_MS(
            var_InheritInteger( p_access, "network-caching" ) );
        break;

    case STREAM_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    vlc_url_t url;
    vlc_credential credential;
    char *psz_decoded_path = NULL, *psz_uri = NULL, *psz_var_domain = NULL;
    int fd;
    uint64_t size;
    bool is_dir;

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

        if (smb_get_uri(access, &psz_uri,
                        credential.psz_username, credential.psz_password,
                        url.psz_host, psz_decoded_path, NULL ) == -1 )
        {
            vlc_credential_clean(&credential);
            free(psz_var_domain);
            free(psz_decoded_path);
            vlc_UrlClean(&url);
            return VLC_ENOMEM;
        }

        if (stat(psz_uri, &st) == 0)
        {
            is_dir = S_ISDIR(st.st_mode) != 0;
            size = st.st_size;
            break;
        }

        /* stat() fails with servers or shares. Assume directory. */
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
        fd = -1;
    }
    else
    {
        access->pf_read = Read;
        access->pf_control = Control;
        access->pf_seek = Seek;
        fd = open(psz_uri, O_RDONLY, 0);
        vlc_UrlClean(&url);
    }
    free(psz_uri);

    sys->size = size;
    sys->i_smb = fd;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    vlc_UrlClean(&sys->url);
    close(sys->i_smb);
}

vlc_module_begin()
    set_shortname("UNC")
    set_description(N_("UNC input"))
    set_help(N_("Microsoft Windows networking (UNC) input"))
    set_capability("access", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_string("smb-user", NULL, SMB_USER_TEXT, SMB_USER_LONGTEXT, false)
    add_password("smb-pwd", NULL, SMB_PASS_TEXT, SMB_PASS_LONGTEXT)
    add_string("smb-domain", NULL, SMB_DOMAIN_TEXT, SMB_DOMAIN_LONGTEXT, false)
    add_shortcut("smb")
    set_callbacks(Open, Close)
vlc_module_end()
