/*****************************************************************************
 * htcpcp.c: HTCPCP module for VLC media player
 *****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_network.h>
#include <vlc_url.h>

#define IPPORT_HTCPCP 80

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname ("HTCPCP")
    set_description (N_("Coffee pot control"))
    set_capability ("access", 0)
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    add_shortcut ("koffie", "q%C3%A6hv%C3%A6", "%D9%82%D9%87%D9%88%D8%A9",
                  "akeita", "koffee", "kahva", "kafe", "caf%C3%E8",
                  "%E5%92%96%E5%95%A1", "kava", "k%C3%A1va", "kaffe", "coffee",
                  "kafo", "kohv", "kahvi", "%4Baffee"/*,
                  "%CE%BA%CE%B1%CF%86%CE%AD",
                  "%E0%A4%95%E0%A5%8C%E0%A4%AB%E0%A5%80",
                  "%E3%82%B3%E3%83%BC%E3%83%92%E3%83%BC",
                  "%EC%BB%A4%ED%94%BC", "%D0%BA%D0%BE%D1%84%D0%B5",
                  "%E0%B8%81%E0%B8%B2%E0%B9%81%E0%B8%9F"*/)
    set_callbacks (Open, Close)
vlc_module_end ()

static ssize_t Read( access_t *, uint8_t *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
    int fd;
};

static int Open (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;

    vlc_url_t url;
    vlc_UrlParse (&url, access->psz_location, 0);

    int fd = net_ConnectTCP (obj, url.psz_host,
                             url.i_port ? url.i_port : IPPORT_HTCPCP);
    if (fd == -1)
    {
        vlc_UrlClean (&url);
        return VLC_EGENERIC;
    }

    access_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    sys->fd = fd;
    net_Printf (obj, fd, NULL, "BREW %s HTTP/1.1\r\n",
                url.psz_path ? url.psz_path : "/");
    if (url.i_port)
        net_Printf (obj, fd, NULL, "Host: %s:%u\r\n",
                    url.psz_host, url.i_port);
    else
        net_Printf (obj, fd, NULL, "Host: %s\r\n", url.psz_host);
    net_Printf (obj, fd, NULL,
                "User-Agent: "PACKAGE_NAME"/"PACKAGE_VERSION"\r\n"
                "Accept-Additions: \r\n"
                "Content-Type: application/coffee-pot-command\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
    vlc_UrlClean (&url);

    access->p_sys = sys;
    access->pf_read = Read;
    access->pf_seek = Seek;
    access->pf_control = Control;
    return VLC_SUCCESS;

error:
    net_Close (fd);
    free (sys);
    vlc_UrlClean (&url);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    net_Close (sys->fd);
    free (sys);
}

static ssize_t Read (access_t *access, uint8_t *buf, size_t len)
{
    access_sys_t *sys = access->p_sys;

    char *resp = net_Gets (access, sys->fd, NULL);
    if (resp == NULL)
        goto error;

    unsigned code;
    if (sscanf (resp, "HTTP/%*u.%*u %u", &code) != 1)
    {
        msg_Err (access, "cannot parse response from coffee server");
        goto error;
    }
    if ((code / 100) != 2)
    {
        msg_Err (access, "server error %u", code);
        if (code == 418)
            dialog_FatalWait (access, N_("Teapot"), "%s",
                N_("The server is a teapot. You can't brew coffee with "
                   "a teapot."));
        else
            dialog_FatalWait (access, N_("Coffee pot"),
                N_("The pot failed to brew coffee (server error %u)."), code);
        goto error;
    }

    (void) buf; (void) len;
    dialog_FatalWait (access, N_("Coffee pot"), N_("Coffee is ready."));
error:
    access->info.b_eof = true;
    return 0;
}

static int Seek (access_t *access, uint64_t pos)
{
    (void) access; (void) pos;
    return VLC_EGENERIC;
}

static int Control (access_t *access, int query, va_list args)
{
    (void) access;

    switch (query)
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg (args, bool *) = false;
            break;

        case ACCESS_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = DEFAULT_PTS_DELAY * INT64_C(1000);
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
