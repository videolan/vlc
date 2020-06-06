/*****************************************************************************
 * smb2.c: SMB2 access plug-in
 *****************************************************************************
 * Copyright © 2018  VLC authors, VideoLAN and VideoLabs
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
#ifdef HAVE_POLL_H
# include <poll.h>
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_input_item.h>
#include <vlc_plugin.h>
#include <vlc_url.h>
#include <vlc_keystore.h>
#include <vlc_interrupt.h>
#include <vlc_network.h>
#include <vlc_memstream.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#ifdef HAVE_DSM
# include <bdsm/netbios_ns.h>
# include <bdsm/netbios_defs.h>

# ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
# endif
#endif

#include "smb_common.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname("smb2")
    set_description(N_("SMB2 / SMB3 input"))
    set_help(N_("Samba (Windows network shares) input via libsmb2"))
    set_capability("access", 21)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_string("smb-user", NULL, SMB_USER_TEXT, SMB_USER_LONGTEXT, false)
    add_password("smb-pwd", NULL, SMB_PASS_TEXT, SMB_PASS_LONGTEXT)
    add_string("smb-domain", NULL, SMB_DOMAIN_TEXT, SMB_DOMAIN_LONGTEXT, false)
    add_shortcut("smb", "smb2")
    set_callbacks(Open, Close)
vlc_module_end()

struct access_sys
{
    struct smb2_context *   smb2;
    struct smb2fh *         smb2fh;
    struct smb2dir *        smb2dir;
    struct srvsvc_netshareenumall_rep *share_enum;
    uint64_t                smb2_size;
    vlc_url_t               encoded_url;
    bool                    eof;
    bool                    smb2_connected;
    int                     error_status;

    bool res_done;
    union {
        struct
        {
            size_t len;
        } read;
    } res;
};

static int
smb2_check_status(stream_t *access, int status, const char *psz_func)
{
    struct access_sys *sys = access->p_sys;

    if (status < 0)
    {
        const char *psz_error = smb2_get_error(sys->smb2);
        msg_Warn(access, "%s failed: %d, '%s'", psz_func, status, psz_error);
        sys->error_status = status;
        return -1;
    }
    else
    {
        sys->res_done = true;
        return 0;
    }
}

static void
smb2_set_error(stream_t *access, const char *psz_func, int err)
{
    struct access_sys *sys = access->p_sys;

    msg_Err(access, "%s failed: %d, %s", psz_func, err,
            smb2_get_error(sys->smb2));
    sys->error_status = err;
}

#define VLC_SMB2_CHECK_STATUS(access, status) \
    smb2_check_status(access, status, __func__)

#define VLC_SMB2_SET_ERROR(access, func, err) \
    smb2_set_error(access, func, err)

#define VLC_SMB2_STATUS_DENIED(x) (x == -ECONNREFUSED || x == -EACCES)

static int
vlc_smb2_mainloop(stream_t *access, bool teardown)
{
#define TEARDOWN_TIMEOUT 250 /* in ms */
    struct access_sys *sys = access->p_sys;

    int timeout = -1;
    int (*poll_func)(struct pollfd *, unsigned, int) = vlc_poll_i11e;

    /* vlc_smb2_mainloop() can be called to clean-up after an error, but this
     * function can override the error_status (from async cbs). Therefore,
     * store the original error_status in order to restore it at the end of
     * this call (since we want to keep the first and original error status). */
    int original_error_status = sys->error_status;

    if (teardown)
    {
        /* Don't use vlc_poll_i11e that will return immediately with the EINTR
         * errno if VLC's input is interrupted. Use the posix poll with a
         * timeout to let a chance for a clean teardown. */
        timeout = TEARDOWN_TIMEOUT;
        poll_func = (void *)poll;
        sys->error_status = 0;
    }

    sys->res_done = false;
    while (sys->error_status == 0 && !sys->res_done)
    {
        struct pollfd p_fds[1];
        int ret;
        p_fds[0].fd = smb2_get_fd(sys->smb2);
        p_fds[0].events = smb2_which_events(sys->smb2);

        if (p_fds[0].fd == -1 || (ret = poll_func(p_fds, 1, timeout)) < 0)
        {
            if (errno == EINTR)
            {
                msg_Warn(access, "vlc_poll_i11e interrupted");
                if (poll_func != (void *) poll)
                {
                    /* Try again with a timeout to let the command complete.
                     * Indeed, if this command is interrupted, every future
                     * commands will fail and we won't be able to teardown. */
                    timeout = TEARDOWN_TIMEOUT;
                    poll_func = (void *) poll;
                }
                else
                    sys->error_status = -errno;
            }
            else
            {
                msg_Err(access, "vlc_poll_i11e failed");
                sys->error_status = -errno;
            }
        }
        else if (ret == 0)
            sys->error_status = -ETIMEDOUT;
        else if (ret > 0 && p_fds[0].revents
             && smb2_service(sys->smb2, p_fds[0].revents) < 0)
            VLC_SMB2_SET_ERROR(access, "smb2_service", 1);
    }

    int ret = sys->error_status == 0 ? 0 : -1;
    if (original_error_status != 0)
        sys->error_status = original_error_status;
    return ret;
}

#define VLC_SMB2_GENERIC_CB() \
    VLC_UNUSED(smb2); \
    stream_t *access = private_data; \
    struct access_sys *sys = access->p_sys; \
    assert(sys->smb2 == smb2); \
    if (VLC_SMB2_CHECK_STATUS(access, status)) \
        return

static void
smb2_generic_cb(struct smb2_context *smb2, int status, void *data,
                void *private_data)
{
    VLC_UNUSED(data);
    VLC_SMB2_GENERIC_CB();
}

static void
smb2_read_cb(struct smb2_context *smb2, int status, void *data,
             void *private_data)
{
    VLC_UNUSED(data);
    VLC_SMB2_GENERIC_CB();

    if (status == 0)
        sys->eof = true;
    else
        sys->res.read.len = status;
}

static ssize_t
FileRead(stream_t *access, void *buf, size_t len)
{
    struct access_sys *sys = access->p_sys;

    if (sys->error_status != 0)
        return -1;

    if (sys->eof)
        return 0;

    /* Limit the read size since smb2_read_async() will complete only after
     * reading the whole requested data and not when whatever data is available
     * (high read size means a faster I/O but a higher latency). */
    if (len > 262144)
        len = 262144;

    sys->res.read.len = 0;
    if (smb2_read_async(sys->smb2, sys->smb2fh, buf, len,
                        smb2_read_cb, access) < 0)
    {
        VLC_SMB2_SET_ERROR(access, "smb2_read_async", 1);
        return -1;
    }

    if (vlc_smb2_mainloop(access, false) < 0)
        return -1;

    return sys->res.read.len;
}

static int
FileSeek(stream_t *access, uint64_t i_pos)
{
    struct access_sys *sys = access->p_sys;

    if (sys->error_status != 0)
        return VLC_EGENERIC;

    if (smb2_lseek(sys->smb2, sys->smb2fh, i_pos, SEEK_SET, NULL) < 0)
    {
        VLC_SMB2_SET_ERROR(access, "smb2_seek_async", 1);
        return VLC_EGENERIC;
    }
    sys->eof = false;

    return VLC_SUCCESS;
}

static int
FileControl(stream_t *access, int i_query, va_list args)
{
    struct access_sys *sys = access->p_sys;

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
            *va_arg(args, uint64_t *) = sys->smb2_size;
            break;
        }

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t * ) = VLC_TICK_FROM_MS(
                var_InheritInteger(access, "network-caching"));
            break;

        case STREAM_SET_PAUSE_STATE:
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static char *
vlc_smb2_get_url(vlc_url_t *url, const char *file)
{
    /* smb2://<psz_host><i_port><psz_path><file>?<psz_option> */
    struct vlc_memstream buf;
    vlc_memstream_open(&buf);
    if (strchr(url->psz_host, ':') != NULL)
        vlc_memstream_printf(&buf, "smb://[%s]", url->psz_host);
    else
        vlc_memstream_printf(&buf, "smb://%s", url->psz_host);

    if (url->i_port != 0)
        vlc_memstream_printf(&buf, ":%d", url->i_port);

    if (url->psz_path != NULL)
    {
        vlc_memstream_puts(&buf, url->psz_path);
        if (url->psz_path[0] != '\0' && url->psz_path[strlen(url->psz_path) - 1] != '/')
            vlc_memstream_putc(&buf, '/');
    }
    else
        vlc_memstream_putc(&buf, '/');

    vlc_memstream_puts(&buf, file);

    if (url->psz_option)
        vlc_memstream_printf(&buf, "?%s", url->psz_option);

    if (vlc_memstream_close(&buf))
        return NULL;
    return buf.ptr;
}

static int AddItem(stream_t *access, struct vlc_readdir_helper *rdh,
                   const char *name, int i_type)
{
    struct access_sys *sys = access->p_sys;
    char *name_encoded = vlc_uri_encode(name);
    if (name_encoded == NULL)
        return VLC_ENOMEM;

    char *url = vlc_smb2_get_url(&sys->encoded_url, name_encoded);
    free(name_encoded);
    if (url == NULL)
        return VLC_ENOMEM;

    int ret = vlc_readdir_helper_additem(rdh, url, NULL, name, i_type,
                                         ITEM_NET);
    free(url);
    return ret;
}

static int
DirRead(stream_t *access, input_item_node_t *p_node)
{
    struct access_sys *sys = access->p_sys;
    struct smb2dirent *smb2dirent;
    int ret = VLC_SUCCESS;
    assert(sys->smb2dir);

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init(&rdh, access, p_node);

    while (ret == VLC_SUCCESS
        && (smb2dirent = smb2_readdir(sys->smb2, sys->smb2dir)) != NULL)
    {
        int i_type;
        switch (smb2dirent->st.smb2_type)
        {
        case SMB2_TYPE_FILE:
            i_type = ITEM_TYPE_FILE;
            break;
        case SMB2_TYPE_DIRECTORY:
            i_type = ITEM_TYPE_DIRECTORY;
            break;
        default:
            i_type = ITEM_TYPE_UNKNOWN;
            break;
        }
        ret = AddItem(access, &rdh, smb2dirent->name, i_type);
    }

    vlc_readdir_helper_finish(&rdh, ret == VLC_SUCCESS);

    return ret;
}

static int
ShareEnum(stream_t *access, input_item_node_t *p_node)
{
    struct access_sys *sys = access->p_sys;
    assert(sys->share_enum != NULL);

    int ret = VLC_SUCCESS;
    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init(&rdh, access, p_node);

    struct srvsvc_netsharectr *ctr = sys->share_enum->ctr;
    for (uint32_t iinfo = 0;
         iinfo < ctr->ctr1.count && ret == VLC_SUCCESS; ++iinfo)
    {
       struct srvsvc_netshareinfo1 *info = &ctr->ctr1.array[iinfo];
       if (info->type & SHARE_TYPE_HIDDEN)
           continue;
       switch (info->type & 0x3)
       {
           case SHARE_TYPE_DISKTREE:
               ret = AddItem(access, &rdh, info->name, ITEM_TYPE_DIRECTORY);
               break;
       }
    }

    vlc_readdir_helper_finish(&rdh, ret == VLC_SUCCESS);
    return 0;
}

static int
vlc_smb2_close_fh(stream_t *access)
{
    struct access_sys *sys = access->p_sys;

    assert(sys->smb2fh);

    if (smb2_close_async(sys->smb2, sys->smb2fh, smb2_generic_cb, access) < 0)
    {
        VLC_SMB2_SET_ERROR(access, "smb2_close_async", 1);
        return -1;
    }

    sys->smb2fh = NULL;

    return vlc_smb2_mainloop(access, true);
}

static int
vlc_smb2_disconnect_share(stream_t *access)
{
    struct access_sys *sys = access->p_sys;

    if (!sys->smb2_connected)
        return 0;

    if (smb2_disconnect_share_async(sys->smb2, smb2_generic_cb, access) < 0)
    {
        VLC_SMB2_SET_ERROR(access, "smb2_connect_share_async", 1);
        return -1;
    }

    int ret = vlc_smb2_mainloop(access, true);
    sys->smb2_connected = false;
    return ret;
}

static void
smb2_opendir_cb(struct smb2_context *smb2, int status, void *data,
                void *private_data)
{
    VLC_SMB2_GENERIC_CB();

    sys->smb2dir = data;
}

static void
smb2_open_cb(struct smb2_context *smb2, int status, void *data,
             void *private_data)
{
    VLC_SMB2_GENERIC_CB();

    sys->smb2fh = data;
}

static void
smb2_share_enum_cb(struct smb2_context *smb2, int status, void *data,
                   void *private_data)
{
    VLC_SMB2_GENERIC_CB();

    sys->share_enum = data;
}

static int
vlc_smb2_open_share(stream_t *access, const struct smb2_url *smb2_url,
                    const vlc_credential *credential)
{
    struct access_sys *sys = access->p_sys;

    const bool do_enum = smb2_url->share[0] == '\0';
    const char *username = credential->psz_username;
    const char *password = credential->psz_password;
    const char *domain = credential->psz_realm;
    const char *share = do_enum ? "IPC$" : smb2_url->share;

    if (!username)
    {
        username = "Guest";
        /* A NULL password enable ntlmssp anonymous login */
        password = NULL;
    }

    smb2_set_password(sys->smb2, password);
    smb2_set_domain(sys->smb2, domain ? domain : "");

    int err = smb2_connect_share_async(sys->smb2, smb2_url->server, share,
                                       username, smb2_generic_cb, access);
    if (err < 0)
    {
        VLC_SMB2_SET_ERROR(access, "smb2_connect_share_async", err);
        goto error;
    }
    if (vlc_smb2_mainloop(access, false) != 0)
        goto error;
    sys->smb2_connected = true;

    int ret;
    if (do_enum)
        ret = smb2_share_enum_async(sys->smb2, smb2_share_enum_cb, access);
    else
    {
        struct smb2_stat_64 smb2_stat;
        if (smb2_stat_async(sys->smb2, smb2_url->path, &smb2_stat,
                            smb2_generic_cb, access) < 0)
            VLC_SMB2_SET_ERROR(access, "smb2_stat_async", 1);

        if (vlc_smb2_mainloop(access, false) != 0)
            goto error;

        if (smb2_stat.smb2_type == SMB2_TYPE_FILE)
        {
            sys->smb2_size = smb2_stat.smb2_size;
            ret = smb2_open_async(sys->smb2, smb2_url->path, O_RDONLY,
                                  smb2_open_cb, access);
        }
        else if (smb2_stat.smb2_type == SMB2_TYPE_DIRECTORY)
            ret = smb2_opendir_async(sys->smb2, smb2_url->path,
                                     smb2_opendir_cb, access);
        else
        {
            msg_Err(access, "smb2_stat_cb: file type not handled");
            sys->error_status = 1;
            goto error;
        }
    }

    if (ret < 0)
    {
        VLC_SMB2_SET_ERROR(access, "smb2_open*_async", 1);
        goto error;
    }

    if (vlc_smb2_mainloop(access, false) != 0)
        goto error;
    return 0;

error:
    vlc_smb2_disconnect_share(access);
    return -1;
}

static char *
vlc_smb2_resolve(stream_t *access, const char *host, unsigned port)
{
    (void) access;
    if (!host)
        return NULL;

#ifdef HAVE_DSM
    /* Test if the host is an IP */
    struct in_addr addr;
    if (inet_pton(AF_INET, host, &addr) == 1)
        return NULL;

    /* Test if the host can be resolved */
    struct addrinfo *info = NULL;
    if (vlc_getaddrinfo_i11e(host, port, NULL, &info) == 0)
    {
        freeaddrinfo(info);
        /* Let smb2 resolve it */
        return NULL;
    }

    /* Test if the host is a netbios name */
    char *out_host = NULL;
    netbios_ns *ns = netbios_ns_new();
    if (!ns)
        return NULL;
    uint32_t ip4_addr;
    if (netbios_ns_resolve(ns, host, NETBIOS_FILESERVER, &ip4_addr) == 0)
    {
        char ip[] = "xxx.xxx.xxx.xxx";
        if (inet_ntop(AF_INET, &ip4_addr, ip, sizeof(ip)))
            out_host = strdup(ip);
    }
    netbios_ns_destroy(ns);
    return out_host;
#else
    (void) port;
    return NULL;
#endif
}

static int
Open(vlc_object_t *p_obj)
{
    stream_t *access = (stream_t *)p_obj;
    struct access_sys *sys = vlc_obj_calloc(p_obj, 1, sizeof (*sys));
    struct smb2_url *smb2_url = NULL;
    char *var_domain = NULL;

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    access->p_sys = sys;

    /* Parse the encoded URL */
    if (vlc_UrlParseFixup(&sys->encoded_url, access->psz_url) != 0)
        return VLC_ENOMEM;

    sys->smb2 = smb2_init_context();
    if (sys->smb2 == NULL)
    {
        msg_Err(access, "smb2_init_context failed");
        goto error;
    }

    smb2_set_security_mode(sys->smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

    if (sys->encoded_url.psz_path == NULL)
        sys->encoded_url.psz_path = (char *) "/";

    char *resolved_host = vlc_smb2_resolve(access, sys->encoded_url.psz_host,
                                           sys->encoded_url.i_port);

    /* smb2_* functions need a decoded url. Re compose the url from the
     * modified sys->encoded_url (with the resolved host). */
    char *url;
    if (resolved_host != NULL)
    {
        vlc_url_t resolved_url = sys->encoded_url;
        resolved_url.psz_host = resolved_host;
        url = vlc_uri_compose(&resolved_url);
        free(resolved_host);
    }
    else
        url = vlc_uri_compose(&sys->encoded_url);
    if (!vlc_uri_decode(url))
    {
        free(url);
        goto error;
    }
    smb2_url = smb2_parse_url(sys->smb2, url);
    free(url);

    if (!smb2_url || !smb2_url->share || !smb2_url->server)
    {
        msg_Err(access, "smb2_parse_url failed");
        goto error;
    }

    int ret = -1;
    vlc_credential credential;
    vlc_credential_init(&credential, &sys->encoded_url);
    var_domain = var_InheritString(access, "smb-domain");
    credential.psz_realm = var_domain;

    /* First, try Guest login or using "smb-" options (without
     * keystore/user interaction) */
    vlc_credential_get(&credential, access, "smb-user", "smb-pwd", NULL,
                       NULL);
    ret = vlc_smb2_open_share(access, smb2_url, &credential);

    while (ret == -1
        && (!sys->error_status || VLC_SMB2_STATUS_DENIED(sys->error_status))
        && vlc_credential_get(&credential, access, "smb-user", "smb-pwd",
                              SMB_LOGIN_DIALOG_TITLE, SMB_LOGIN_DIALOG_TEXT,
                              smb2_url->server))
    {
        sys->error_status = 0;
        ret = vlc_smb2_open_share(access, smb2_url, &credential);
    }
    if (ret == 0)
        vlc_credential_store(&credential, access);
    vlc_credential_clean(&credential);

    if (ret != 0)
    {
        const char *error = smb2_get_error(sys->smb2);
        if (error && *error)
            vlc_dialog_display_error(access,
                                     _("SMB2 operation failed"), "%s", error);
        if (credential.i_get_order == GET_FROM_DIALOG)
        {
            /* Tell other smb modules (likely dsm) that we already requested
             * credential to the users and that it it useless to try again.
             * This avoid to show 2 login dialogs for the same access. */
            var_Create(access, "smb-dialog-failed", VLC_VAR_VOID);
        }
        goto error;
    }

    if (sys->smb2fh != NULL)
    {
        access->pf_read = FileRead;
        access->pf_seek = FileSeek;
        access->pf_control = FileControl;
    }
    else if (sys->smb2dir != NULL)
    {
        access->pf_readdir = DirRead;
        access->pf_seek = NULL;
        access->pf_control = access_vaDirectoryControlHelper;
    }
    else if (sys->share_enum != NULL)
    {
        access->pf_readdir = ShareEnum;
        access->pf_seek = NULL;
        access->pf_control = access_vaDirectoryControlHelper;
    }
    else
        vlc_assert_unreachable();

    smb2_destroy_url(smb2_url);
    free(var_domain);
    return VLC_SUCCESS;

error:
    if (smb2_url != NULL)
        smb2_destroy_url(smb2_url);
    if (sys->smb2 != NULL)
    {
        vlc_smb2_disconnect_share(access);
        smb2_destroy_context(sys->smb2);
    }
    vlc_UrlClean(&sys->encoded_url);
    free(var_domain);

    /* Returning VLC_ETIMEOUT will stop the module probe and prevent to load
     * the next smb module. The smb2 module can return this specific error in
     * case of network error (EIO) or when the user asked to cancel it
     * (vlc_killed()). Indeed, in these cases, it is useless to try next smb
     * modules. */
    return vlc_killed() || sys->error_status == -EIO ? VLC_ETIMEOUT
         : VLC_EGENERIC;
}

static void
Close(vlc_object_t *p_obj)
{
    stream_t *access = (stream_t *)p_obj;
    struct access_sys *sys = access->p_sys;

    if (sys->smb2fh != NULL)
        vlc_smb2_close_fh(access);
    else if (sys->smb2dir != NULL)
        smb2_closedir(sys->smb2, sys->smb2dir);
    else if (sys->share_enum != NULL)
        smb2_free_data(sys->smb2, sys->share_enum);
    else
        vlc_assert_unreachable();

    vlc_smb2_disconnect_share(access);
    smb2_destroy_context(sys->smb2);

    vlc_UrlClean(&sys->encoded_url);
}
