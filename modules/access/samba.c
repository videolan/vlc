/*****************************************************************************
 * samba.c: Samba / libsmbclient input module
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
#include <libsmbclient.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_keystore.h>

#include "smb_common.h"

typedef struct
{
    SMBCCTX *ctx;
    SMBCFILE *file;

    vlc_credential credential;
    char *var_domain;

    union
    {
        struct {
            smbc_close_fn close;
            smbc_read_fn read;
            smbc_lseek_fn lseek;
        } filevt;
        struct {
            smbc_closedir_fn closedir;
            smbc_stat_fn stat;
            smbc_readdir_fn readdir;
        } dirvt;
    };

    uint64_t size;
    vlc_url_t url;
} access_sys_t;

/* Build an SMB URI
 * smb://server[/share[/path[/file]]]] */
static char *smb_get_uri(const char *psz_server, const char *psz_share_path,
                         const char *psz_name)
{
    char *uri;

    assert(psz_server);
    if (asprintf( &uri, "smb://%s%s%s%s", psz_server,
                  psz_share_path ? psz_share_path : "",
                  psz_name ? "/" : "", psz_name ? psz_name : "" ) == -1)
        return NULL;
    return uri;
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

    i_ret = p_sys->filevt.lseek( p_sys->ctx, p_sys->file, i_pos, SEEK_SET );
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

    /* cf. DEFAULT_SMB2_MAX_READ (= 8MB) from the samba project. Reading more
     * than this limit will likely result on a ECONNABORTED
     * (STATUS_CONNECTION_ABORTED) error. Since this value can be lowered by
     * the server, let decrease this limit (/8) to have more chance to get a
     * working limit on our side. */
    if( i_len > (1024 << 10) ) /* 8MB / 8 = 1MB */
        i_len = 1024 << 10;

    i_read = p_sys->filevt.read( p_sys->ctx, p_sys->file, p_buffer, i_len );
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

    struct smbc_dirent *p_entry;

    while( i_ret == VLC_SUCCESS && ( p_entry = p_sys->dirvt.readdir( p_sys->ctx, p_sys->file ) ) )
    {
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
            /* fall through */
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

        char *uri = smb_get_uri(psz_server, psz_path, psz_encoded_name);
        if (uri == NULL) {
            free(psz_encoded_name);
            i_ret = VLC_ENOMEM;
            break;
        }
        struct stat st;
        p_sys->dirvt.stat(p_sys->ctx, uri, &st);

        input_item_t *p_item;
        free(psz_encoded_name);
        i_ret = vlc_readdir_helper_additem(&rdh, uri, NULL, p_entry->name,
                                           i_type, ITEM_NET, &p_item);
        if (i_ret == VLC_SUCCESS && p_item)
        {
            input_item_AddStat( p_item, "mtime", st.st_mtime );
            input_item_AddStat( p_item, "size", st.st_size );
        }
        free(uri);
    }

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

static void smb_auth(SMBCCTX *ctx, const char *srv, const char *shr,
                     char *wg, int wglen,
                     char *un, int unlen,
                     char *pw, int pwlen)
{
    VLC_UNUSED(srv);
    VLC_UNUSED(shr);

    access_sys_t *sys = smbc_getOptionUserData(ctx);
    assert(sys != NULL);

    if (sys->credential.psz_realm != NULL)
        strlcpy(wg, sys->credential.psz_realm, wglen);
    if (sys->credential.psz_username != NULL)
        strlcpy(un, sys->credential.psz_username, unlen);
    if (sys->credential.psz_password != NULL)
        strlcpy(pw, sys->credential.psz_password, pwlen);
    else
    {
        /* Since last Windows 11 update (KB5026436), Windows SMB servers need a
         * valid Auth (user + password) even for a guest/anonymous login.
         * Therefore, store the user in the password to fake a valid password.
         * */
        strlcpy(pw, un, pwlen);
    }
}

static int Open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    vlc_url_t url;
    char *psz_decoded_path = NULL, *uri;
    bool is_dir;
    int ret = VLC_EGENERIC;

    /* Init access */
    access_sys_t *sys = vlc_obj_calloc(obj, 1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->ctx = smbc_new_context();
    if (sys->ctx == NULL)
        return VLC_ENOMEM;

    smbc_setDebug(sys->ctx, 0);
    smbc_setOptionUserData(sys->ctx, sys);
    smbc_setFunctionAuthDataWithContext(sys->ctx, smb_auth);

    if (!smbc_init_context(sys->ctx))
        goto error;

    if (vlc_UrlParseFixup(&url, access->psz_url) != 0)
    {
        vlc_UrlClean(&url);
        goto error;
    }

    if (url.psz_path != NULL)
    {
        psz_decoded_path = vlc_uri_decode_duplicate(url.psz_path);
        if (psz_decoded_path == NULL)
        {
            vlc_UrlClean(&url);
            goto error;
        }
    }

    vlc_credential_init(&sys->credential, &url);
    sys->var_domain = var_InheritString(access, "smb-domain");
    sys->credential.psz_realm = sys->var_domain;
    if (vlc_credential_get(&sys->credential, access, "smb-user", "smb-pwd",
                           NULL, NULL)
        == -EINTR)
    {
        vlc_UrlClean(&url);
        goto error;
    }

    smbc_stat_fn stat_fn = smbc_getFunctionStat(sys->ctx);
    assert(stat_fn);

    for (;;)
    {
        struct stat st;

        uri = smb_get_uri(url.psz_host, psz_decoded_path, NULL);
        if (uri == NULL)
        {
            free(psz_decoded_path);
            vlc_UrlClean(&url);
            ret = VLC_ENOMEM;
            goto error;
        }


        if (stat_fn(sys->ctx, uri, &st) == 0)
        {
            is_dir = S_ISDIR(st.st_mode) != 0;
            sys->size = st.st_size;
            break;
        }

        /* smbc_stat() fails with servers or shares. Assume directory. */
        is_dir = true;
        sys->size = 0;

        if (errno != EACCES && errno != EPERM)
            break;

        errno = 0;
        if (vlc_credential_get(&sys->credential, access, "smb-user",
                               "smb-pwd", SMB_LOGIN_DIALOG_TITLE,
                               SMB_LOGIN_DIALOG_TEXT, url.psz_host) != 0)
            break;
    }

    free(psz_decoded_path);

    access->p_sys = sys;

    if (is_dir)
    {
        smbc_opendir_fn opendir_fn = smbc_getFunctionOpendir(sys->ctx);
        assert(opendir_fn);

        sys->dirvt.closedir = smbc_getFunctionClosedir(sys->ctx);
        assert(sys->dirvt.closedir);
        sys->dirvt.readdir = smbc_getFunctionReaddir(sys->ctx);
        assert(sys->dirvt.readdir);
        sys->dirvt.stat = stat_fn;

        sys->url = url;
        access->pf_readdir = DirRead;
        access->pf_control = access_vaDirectoryControlHelper;
        sys->file = opendir_fn(sys->ctx, uri);
        if (sys->file == NULL)
            vlc_UrlClean(&sys->url);
    }
    else
    {
        smbc_open_fn open_fn = smbc_getFunctionOpen(sys->ctx);
        assert(open_fn);

        sys->filevt.close = smbc_getFunctionClose(sys->ctx);
        assert(sys->filevt.close);
        sys->filevt.read = smbc_getFunctionRead(sys->ctx);
        assert(sys->filevt.read);
        sys->filevt.lseek = smbc_getFunctionLseek(sys->ctx);
        assert(sys->filevt.lseek);

        access->pf_read = Read;
        access->pf_control = Control;
        access->pf_seek = Seek;
        sys->file = open_fn(sys->ctx, uri, O_RDONLY, 0);
        vlc_UrlClean(&url);
    }
    free(uri);

    if (sys->file == NULL)
    {
        msg_Err(obj, "cannot open %s: %s",
                access->psz_location, vlc_strerror_c(errno));
        goto error;
    }

    vlc_credential_store(&sys->credential, access);
    return VLC_SUCCESS;

error:
    vlc_credential_clean(&sys->credential);
    free(sys->var_domain);
    smbc_free_context(sys->ctx, 1);
    return ret;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    vlc_credential_clean(&sys->credential);
    free(sys->var_domain);

    vlc_UrlClean(&sys->url);

    if (access->pf_readdir != NULL)
        sys->dirvt.closedir(sys->ctx, sys->file);
    else
        sys->filevt.close(sys->ctx, sys->file);

    smbc_free_context(sys->ctx, 1);
}

vlc_module_begin()
    set_shortname("SMB")
    set_description(N_("SMB input"))
    set_help(N_("Samba (Windows network shares) input"))
    set_capability("access", 0)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_string("smb-user", NULL, SMB_USER_TEXT, SMB_USER_LONGTEXT)
    add_password("smb-pwd", NULL, SMB_PASS_TEXT, SMB_PASS_LONGTEXT)
    add_string("smb-domain", NULL, SMB_DOMAIN_TEXT, SMB_DOMAIN_LONGTEXT)
    add_shortcut("smb")
    set_callbacks(Open, Close)
vlc_module_end()
