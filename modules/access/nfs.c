/*****************************************************************************
 * nfs.c: NFS VLC access plug-in
 *****************************************************************************
 * Copyright © 2016  VLC authors, VideoLAN and VideoLabs
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
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_input_item.h>
#include <vlc_plugin.h>
#include <vlc_url.h>
#include <vlc_interrupt.h>

#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw.h>
#include <nfsc/libnfs-raw-nfs.h>
#include <nfsc/libnfs-raw-mount.h>

#define AUTO_GUID_TEXT N_("Set NFS uid/guid automatically")
#define AUTO_GUID_LONGTEXT N_("If uid/gid are not specified in " \
    "the url, VLC will automatically set a uid/gid.")

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("NFS"))
    set_description(N_("NFS input"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_bool("nfs-auto-guid", true, AUTO_GUID_TEXT, AUTO_GUID_LONGTEXT, true)
    set_capability("access", 2)
    add_shortcut("nfs")
    set_callbacks(Open, Close)
vlc_module_end()

struct access_sys_t
{
    struct rpc_context *    p_mount; /* used to to get exports mount point */
    struct nfs_context *    p_nfs;
    struct nfs_url *        p_nfs_url;
    struct nfs_stat_64      stat;
    struct nfsfh *          p_nfsfh;
    struct nfsdir *         p_nfsdir;
    vlc_url_t               encoded_url;
    char *                  psz_url_decoded;
    char *                  psz_url_decoded_slash;
    bool                    b_eof;
    bool                    b_error;
    bool                    b_auto_guid;

    union {
        struct
        {
            char **         ppsz_names;
            int             i_count;
        } exports;
        struct
        {
            uint8_t *p_buf;
            size_t i_len;
        } read;
        struct
        {
            bool b_done;
        } seek;
    } res;
};

static bool
nfs_check_status(stream_t *p_access, int i_status, const char *psz_error,
                 const char *psz_func)
{
    access_sys_t *sys = p_access->p_sys;

    if (i_status < 0)
    {
        if (i_status != -EINTR)
        {
            msg_Err(p_access, "%s failed: %d, '%s'", psz_func, i_status,
                    psz_error);
            if (!sys->b_error)
                vlc_dialog_display_error(p_access,
                                         _("NFS operation failed"), "%s",
                                         psz_error);
        }
        else
            msg_Warn(p_access, "%s interrupted", psz_func);
        sys->b_error = true;
        return true;
    }
    else
        return false;
}
#define NFS_CHECK_STATUS(p_access, i_status, p_data) \
    nfs_check_status(p_access, i_status, (const char *)p_data, __func__)

static int
vlc_rpc_mainloop(stream_t *p_access, struct rpc_context *p_rpc_ctx,
                 bool (*pf_until_cb)(stream_t *))
{
    access_sys_t *p_sys = p_access->p_sys;

    while (!p_sys->b_error && !pf_until_cb(p_access))
    {
        struct pollfd p_fds[1];
        int i_ret;
        p_fds[0].fd = rpc_get_fd(p_rpc_ctx);
        p_fds[0].events = rpc_which_events(p_rpc_ctx);

        if ((i_ret = vlc_poll_i11e(p_fds, 1, -1)) < 0)
        {
            if (errno == EINTR)
                msg_Warn(p_access, "vlc_poll_i11e interrupted");
            else
                msg_Err(p_access, "vlc_poll_i11e failed");
            p_sys->b_error = true;
        }
        else if (i_ret > 0 && p_fds[0].revents
             && rpc_service(p_rpc_ctx, p_fds[0].revents) < 0)
        {
            msg_Err(p_access, "nfs_service failed");
            p_sys->b_error = true;
        }
    }
    return p_sys->b_error ? -1 : 0;
}

static int
vlc_nfs_mainloop(stream_t *p_access, bool (*pf_until_cb)(stream_t *))
{
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs != NULL);
    return vlc_rpc_mainloop(p_access, nfs_get_rpc_context(p_sys->p_nfs),
                            pf_until_cb);
}

static int
vlc_mount_mainloop(stream_t *p_access, bool (*pf_until_cb)(stream_t *))
{
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_mount != NULL);
    return vlc_rpc_mainloop(p_access, p_sys->p_mount, pf_until_cb);
}

static void
nfs_read_cb(int i_status, struct nfs_context *p_nfs, void *p_data,
            void *p_private_data)
{
    VLC_UNUSED(p_nfs);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs == p_nfs);
    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    if (i_status == 0)
        p_sys->b_eof = true;
    else
    {
        p_sys->res.read.i_len = i_status;
        memcpy(p_sys->res.read.p_buf, p_data, i_status);
    }
}

static bool
nfs_read_finished_cb(stream_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    return p_sys->res.read.i_len > 0 || p_sys->b_eof;
}

static ssize_t
FileRead(stream_t *p_access, void *p_buf, size_t i_len)
{
    access_sys_t *p_sys = p_access->p_sys;

    if (p_sys->b_eof)
        return 0;

    p_sys->res.read.i_len = 0;
    p_sys->res.read.p_buf = p_buf;
    if (nfs_read_async(p_sys->p_nfs, p_sys->p_nfsfh, i_len, nfs_read_cb,
                       p_access) < 0)
    {
        msg_Err(p_access, "nfs_read_async failed");
        return -1;
    }

    if (vlc_nfs_mainloop(p_access, nfs_read_finished_cb) < 0)
        return -1;

    return p_sys->res.read.i_len;
}

static void
nfs_seek_cb(int i_status, struct nfs_context *p_nfs, void *p_data,
            void *p_private_data)
{
    VLC_UNUSED(p_nfs);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs == p_nfs);
    (void) p_data;
    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    p_sys->res.seek.b_done = true;
}

static bool
nfs_seek_finished_cb(stream_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    return p_sys->res.seek.b_done;
}

static int
FileSeek(stream_t *p_access, uint64_t i_pos)
{
    access_sys_t *p_sys = p_access->p_sys;

    p_sys->res.seek.b_done = false;
    if (nfs_lseek_async(p_sys->p_nfs, p_sys->p_nfsfh, i_pos, SEEK_SET,
                        nfs_seek_cb, p_access) < 0)
    {
        msg_Err(p_access, "nfs_seek_async failed");
        return VLC_EGENERIC;
    }

    if (vlc_nfs_mainloop(p_access, nfs_seek_finished_cb) < 0)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int
FileControl(stream_t *p_access, int i_query, va_list args)
{
    access_sys_t *p_sys = p_access->p_sys;

    switch (i_query)
    {
        case STREAM_CAN_SEEK:
            *va_arg(args, bool *) = true;
            break;

        case STREAM_CAN_FASTSEEK:
            *va_arg(args, bool *) = false;
            break;

        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = true;
            break;

        case STREAM_GET_SIZE:
        {
            *va_arg(args, uint64_t *) = p_sys->stat.nfs_size;
            break;
        }

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = var_InheritInteger(p_access,
                                                          "network-caching");
            break;

        case STREAM_SET_PAUSE_STATE:
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static char *
NfsGetUrl(vlc_url_t *p_url, const char *psz_file)
{
    /* nfs://<psz_host><psz_path><psz_file>?<psz_option> */
    char *psz_url;
    if (asprintf(&psz_url, "nfs://%s%s%s%s%s%s", p_url->psz_host,
                 p_url->psz_path != NULL ? p_url->psz_path : "",
                 p_url->psz_path != NULL && p_url->psz_path[0] != '\0' &&
                 p_url->psz_path[strlen(p_url->psz_path) - 1] != '/' ? "/" : "",
                 psz_file,
                 p_url->psz_option != NULL ? "?" : "",
                 p_url->psz_option != NULL ? p_url->psz_option : "") == -1)
        return NULL;
    else
        return psz_url;
}

static int
DirRead(stream_t *p_access, input_item_node_t *p_node)
{
    access_sys_t *p_sys = p_access->p_sys;
    struct nfsdirent *p_nfsdirent;
    int i_ret = VLC_SUCCESS;
    assert(p_sys->p_nfsdir);

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init(&rdh, p_access, p_node);

    while (i_ret == VLC_SUCCESS
        && (p_nfsdirent = nfs_readdir(p_sys->p_nfs, p_sys->p_nfsdir)) != NULL)
    {
        char *psz_name_encoded = vlc_uri_encode(p_nfsdirent->name);
        if (psz_name_encoded == NULL)
        {
            i_ret = VLC_ENOMEM;
            break;
        }
        char *psz_url = NfsGetUrl(&p_sys->encoded_url, psz_name_encoded);
        free(psz_name_encoded);
        if (psz_url == NULL)
        {
            i_ret = VLC_ENOMEM;
            break;
        }

        int i_type;
        switch (p_nfsdirent->type)
        {
        case NF3REG:
            i_type = ITEM_TYPE_FILE;
            break;
        case NF3DIR:
            i_type = ITEM_TYPE_DIRECTORY;
            break;
        default:
            i_type = ITEM_TYPE_UNKNOWN;
        }
        i_ret = vlc_readdir_helper_additem(&rdh, psz_url, NULL, p_nfsdirent->name,
                                           i_type, ITEM_NET);
        free(psz_url);
    }

    vlc_readdir_helper_finish(&rdh, i_ret == VLC_SUCCESS);

    return i_ret;
}

static int
MountRead(stream_t *p_access, input_item_node_t *p_node)
{
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_mount != NULL && p_sys->res.exports.i_count >= 0);
    int i_ret = VLC_SUCCESS;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init(&rdh, p_access, p_node);

    for (int i = 0; i < p_sys->res.exports.i_count && i_ret == VLC_SUCCESS; ++i)
    {
        char *psz_name = p_sys->res.exports.ppsz_names[i];

        char *psz_url = NfsGetUrl(&p_sys->encoded_url, psz_name);
        if (psz_url == NULL)
        {
            i_ret = VLC_ENOMEM;
            break;
        }
        i_ret = vlc_readdir_helper_additem(&rdh, psz_url, NULL, psz_name,
                                            ITEM_TYPE_DIRECTORY, ITEM_NET);
        free(psz_url);
    }

    vlc_readdir_helper_finish(&rdh, i_ret == VLC_SUCCESS);

    return i_ret;
}

static void
nfs_opendir_cb(int i_status, struct nfs_context *p_nfs, void *p_data,
               void *p_private_data)
{
    VLC_UNUSED(p_nfs);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs == p_nfs);
    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    p_sys->p_nfsdir = p_data;
}

static void
nfs_open_cb(int i_status, struct nfs_context *p_nfs, void *p_data,
            void *p_private_data)
{
    VLC_UNUSED(p_nfs);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs == p_nfs);
    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    p_sys->p_nfsfh = p_data;
}

static void
nfs_stat64_cb(int i_status, struct nfs_context *p_nfs, void *p_data,
              void *p_private_data)
{
    VLC_UNUSED(p_nfs);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs == p_nfs);
    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    struct nfs_stat_64 *p_stat = p_data;
    p_sys->stat = *p_stat;

    if (p_sys->b_auto_guid)
    {
        nfs_set_uid(p_sys->p_nfs, p_sys->stat.nfs_uid);
        nfs_set_gid(p_sys->p_nfs, p_sys->stat.nfs_gid);
    }

    if (S_ISDIR(p_sys->stat.nfs_mode))
    {
        msg_Dbg(p_access, "nfs_opendir: '%s'", p_sys->p_nfs_url->file);
        if (nfs_opendir_async(p_sys->p_nfs, p_sys->p_nfs_url->file,
                              nfs_opendir_cb, p_access) != 0)
        {
            msg_Err(p_access, "nfs_opendir_async failed");
            p_sys->b_error = true;
        }
    }
    else if (S_ISREG(p_sys->stat.nfs_mode))
    {
        msg_Dbg(p_access, "nfs_open: '%s'", p_sys->p_nfs_url->file);
        if (nfs_open_async(p_sys->p_nfs, p_sys->p_nfs_url->file, O_RDONLY,
                           nfs_open_cb, p_access) < 0)
        {
            msg_Err(p_access, "nfs_open_async failed");
            p_sys->b_error = true;
        }
    }
    else
    {
        msg_Err(p_access, "nfs_stat64_cb: file type not handled");
        p_sys->b_error = true;
    }
}

static void
nfs_mount_cb(int i_status, struct nfs_context *p_nfs, void *p_data,
             void *p_private_data)
{
    VLC_UNUSED(p_nfs);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_nfs == p_nfs);
    (void) p_data;

    /* If a directory url doesn't end with '/', there is no way to know which
     * part of the url is the export point and which part is the path. An
     * example with "nfs://myhost/mnt/data": we can't know if /mnt or /mnt/data
     * is the export point. Therefore, in case of EACCES error, retry to mount
     * the url by adding a '/' to the decoded path. */
    if (i_status == -EACCES && p_sys->psz_url_decoded_slash == NULL)
    {
        vlc_url_t url;
        vlc_UrlParseFixup(&url, p_access->psz_url);
        if (url.psz_path == NULL || url.psz_path[0] == '\0'
         || url.psz_path[strlen(url.psz_path) - 1] == '/'
         || (p_sys->psz_url_decoded_slash = NfsGetUrl(&url, "/")) == NULL)
        {
            vlc_UrlClean(&url);
            NFS_CHECK_STATUS(p_access, i_status, p_data);
            return;
        }
        else
        {
            vlc_UrlClean(&url);
            msg_Warn(p_access, "trying to mount '%s' again by adding a '/'",
                     p_access->psz_url);
            return;
        }
    }

    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    if (nfs_stat64_async(p_sys->p_nfs, p_sys->p_nfs_url->file, nfs_stat64_cb,
                         p_access) < 0)
    {
        msg_Err(p_access, "nfs_stat64_async failed");
        p_sys->b_error = true;
    }
}

static bool
nfs_mount_open_finished_cb(stream_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    return p_sys->p_nfsfh != NULL || p_sys->p_nfsdir != NULL
        || p_sys->psz_url_decoded_slash != NULL;
}

static bool
nfs_mount_open_slash_finished_cb(stream_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    return p_sys->p_nfsfh != NULL || p_sys->p_nfsdir != NULL;
}

static void
mount_export_cb(struct rpc_context *p_ctx, int i_status, void *p_data,
                void *p_private_data)
{
    VLC_UNUSED(p_ctx);
    stream_t *p_access = p_private_data;
    access_sys_t *p_sys = p_access->p_sys;
    assert(p_sys->p_mount == p_ctx);
    if (NFS_CHECK_STATUS(p_access, i_status, p_data))
        return;

    exports p_export = *(exports *)p_data;
    p_sys->res.exports.i_count = 0;

    /* Dup the export linked list into an array of const char * */
    while (p_export != NULL)
    {
        p_sys->res.exports.i_count++;
        p_export = p_export->ex_next;
    }
    if (p_sys->res.exports.i_count == 0)
        return;

    p_sys->res.exports.ppsz_names = calloc(p_sys->res.exports.i_count,
                                           sizeof(char *));
    if (p_sys->res.exports.ppsz_names == NULL)
    {
        p_sys->b_error = true;
        return;
    }

    p_export = *(exports *)p_data;
    unsigned int i_idx = 0;
    while (p_export != NULL)
    {
        p_sys->res.exports.ppsz_names[i_idx] = strdup(p_export->ex_dir);
        if (p_sys->res.exports.ppsz_names[i_idx] == NULL)
        {
            for (unsigned int i = 0; i < i_idx; ++i)
                free(p_sys->res.exports.ppsz_names[i]);
            free(p_sys->res.exports.ppsz_names);
            p_sys->res.exports.ppsz_names = NULL;
            p_sys->res.exports.i_count = 0;
            p_sys->b_error = true;
            return;
        }
        i_idx++;
        p_export = p_export->ex_next;
    }
}

static bool
mount_getexports_finished_cb(stream_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    return p_sys->res.exports.i_count != -1;
}

static int
NfsInit(stream_t *p_access, const char *psz_url_decoded)
{
    access_sys_t *p_sys = p_access->p_sys;
    p_sys->p_nfs = nfs_init_context();
    if (p_sys->p_nfs == NULL)
    {
        msg_Err(p_access, "nfs_init_context failed");
        return -1;
    }

    p_sys->p_nfs_url = nfs_parse_url_incomplete(p_sys->p_nfs, psz_url_decoded);
    if (p_sys->p_nfs_url == NULL || p_sys->p_nfs_url->server == NULL)
    {
        msg_Err(p_access, "nfs_parse_url_incomplete failed: '%s'",
                nfs_get_error(p_sys->p_nfs));
        return -1;
    }
    return 0;
}

static int
Open(vlc_object_t *p_obj)
{
    stream_t *p_access = (stream_t *)p_obj;
    access_sys_t *p_sys = vlc_obj_calloc(p_obj, 1, sizeof (*p_sys));

    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;
    p_access->p_sys = p_sys;

    p_sys->b_auto_guid = var_InheritBool(p_obj, "nfs-auto-guid");

    /* nfs_* functions need a decoded url */
    p_sys->psz_url_decoded = vlc_uri_decode_duplicate(p_access->psz_url);
    if (p_sys->psz_url_decoded == NULL)
        goto error;

    /* Parse the encoded URL */
    if (vlc_UrlParseFixup(&p_sys->encoded_url, p_access->psz_url) != 0)
        goto error;
    if (p_sys->encoded_url.psz_option)
    {
        if (strstr(p_sys->encoded_url.psz_option, "uid")
         || strstr(p_sys->encoded_url.psz_option, "gid"))
            p_sys->b_auto_guid = false;
    }

    if (NfsInit(p_access, p_sys->psz_url_decoded) == -1)
        goto error;

    if (p_sys->p_nfs_url->path != NULL && p_sys->p_nfs_url->file != NULL)
    {
        /* The url has a valid path and file, mount the path and open/opendir
         * the file */
        msg_Dbg(p_access, "nfs_mount: server: '%s', path: '%s'",
                p_sys->p_nfs_url->server, p_sys->p_nfs_url->path);

        if (nfs_mount_async(p_sys->p_nfs, p_sys->p_nfs_url->server,
                            p_sys->p_nfs_url->path, nfs_mount_cb, p_access) < 0)
        {
            msg_Err(p_access, "nfs_mount_async failed");
            goto error;
        }

        if (vlc_nfs_mainloop(p_access, nfs_mount_open_finished_cb) < 0)
            goto error;

        if (p_sys->psz_url_decoded_slash != NULL)
        {
            /* Retry to mount by adding a '/' to the path, see comment in
             * nfs_mount_cb */
            nfs_destroy_url(p_sys->p_nfs_url);
            nfs_destroy_context(p_sys->p_nfs);
            p_sys->p_nfs_url = NULL;
            p_sys->p_nfs = NULL;

            if (NfsInit(p_access, p_sys->psz_url_decoded_slash) == -1
             || p_sys->p_nfs_url->path == NULL || p_sys->p_nfs_url->file == NULL)
                goto error;

            if (nfs_mount_async(p_sys->p_nfs, p_sys->p_nfs_url->server,
                                p_sys->p_nfs_url->path, nfs_mount_cb, p_access) < 0)
            {
                msg_Err(p_access, "nfs_mount_async failed");
                goto error;
            }

            if (vlc_nfs_mainloop(p_access, nfs_mount_open_slash_finished_cb) < 0)
                goto error;
        }

        if (p_sys->p_nfsfh != NULL)
        {
            p_access->pf_read = FileRead;
            p_access->pf_seek = FileSeek;
            p_access->pf_control = FileControl;
        }
        else if (p_sys->p_nfsdir != NULL)
        {
            p_access->pf_readdir = DirRead;
            p_access->pf_seek = NULL;
            p_access->pf_control = access_vaDirectoryControlHelper;
        }
        else
            vlc_assert_unreachable();
    }
    else
    {
        /* url is just a server: fetch exports point */
        nfs_destroy_context(p_sys->p_nfs);
        p_sys->p_nfs = NULL;

        p_sys->p_mount = rpc_init_context();
        if (p_sys->p_mount == NULL)
        {
            msg_Err(p_access, "rpc_init_context failed");
            goto error;
        }

        p_sys->res.exports.ppsz_names = NULL;
        p_sys->res.exports.i_count = -1;

        if (mount_getexports_async(p_sys->p_mount, p_sys->p_nfs_url->server,
                                   mount_export_cb, p_access) < 0)
        {
            msg_Err(p_access, "mount_getexports_async failed");
            goto error;
        }

        if (vlc_mount_mainloop(p_access, mount_getexports_finished_cb) < 0)
            goto error;

        p_access->pf_readdir = MountRead;
        p_access->pf_seek = NULL;
        p_access->pf_control = access_vaDirectoryControlHelper;
    }

    return VLC_SUCCESS;

error:
    Close(p_obj);
    return VLC_EGENERIC;
}

static void
Close(vlc_object_t *p_obj)
{
    stream_t *p_access = (stream_t *)p_obj;
    access_sys_t *p_sys = p_access->p_sys;

    if (p_sys->p_nfsfh != NULL)
        nfs_close(p_sys->p_nfs, p_sys->p_nfsfh);

    if (p_sys->p_nfsdir != NULL)
        nfs_closedir(p_sys->p_nfs, p_sys->p_nfsdir);

    if (p_sys->p_nfs != NULL)
        nfs_destroy_context(p_sys->p_nfs);

    if (p_sys->p_mount != NULL)
    {
        for (int i = 0; i < p_sys->res.exports.i_count; ++i)
            free(p_sys->res.exports.ppsz_names[i]);
        free(p_sys->res.exports.ppsz_names);
        rpc_destroy_context(p_sys->p_mount);
    }

    if (p_sys->p_nfs_url != NULL)
        nfs_destroy_url(p_sys->p_nfs_url);

    vlc_UrlClean(&p_sys->encoded_url);

    free(p_sys->psz_url_decoded);
    free(p_sys->psz_url_decoded_slash);
}
