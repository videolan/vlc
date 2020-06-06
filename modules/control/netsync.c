/*****************************************************************************
 * netsync.c: synchronization between several network clients.
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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
#include <assert.h>

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist_legacy.h>

#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_POLL_H
#   include <poll.h>
#endif

#include <vlc_network.h>

#define NETSYNC_PORT 9875

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define NETSYNC_TEXT N_("Network master clock")
#define NETSYNC_LONGTEXT N_("When set, " \
  "this VLC instance will act as the master clock for synchronization " \
  "for clients listening")

#define MIP_TEXT N_("Master server IP address")
#define MIP_LONGTEXT N_("The IP address of " \
  "the network master clock to use for clock synchronization.")

#define NETSYNC_TIMEOUT_TEXT N_("UDP timeout (in ms)")
#define NETSYNC_TIMEOUT_LONGTEXT N_("Length of time (in ms) " \
  "until aborting data reception.")

vlc_module_begin()
    set_shortname(N_("Network Sync"))
    set_description(N_("Network synchronization"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)

    add_bool("netsync-master", false,
              NETSYNC_TEXT, NETSYNC_LONGTEXT, true)
    add_string("netsync-master-ip", NULL, MIP_TEXT, MIP_LONGTEXT,
                true)
    add_integer("netsync-timeout", 500,
                 NETSYNC_TIMEOUT_TEXT, NETSYNC_TIMEOUT_LONGTEXT, true)

    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct intf_sys_t {
    int            fd;
    int            timeout;
    bool           is_master;
    playlist_t     *playlist;

    /* */
    input_thread_t *input;
    vlc_thread_t   thread;
};

static int PlaylistEvent(vlc_object_t *, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data);

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    intf_thread_t *intf = (intf_thread_t*)object;
    intf_sys_t    *sys;
    int fd;

    if (!var_InheritBool(intf, "netsync-master")) {
        char *psz_master = var_InheritString(intf, "netsync-master-ip");
        if (psz_master == NULL) {
            msg_Err(intf, "master address not specified");
            return VLC_EGENERIC;
        }
        fd = net_ConnectUDP(VLC_OBJECT(intf), psz_master, NETSYNC_PORT, -1);
        free(psz_master);
    } else {
        fd = net_ListenUDP1(VLC_OBJECT(intf), NULL, NETSYNC_PORT);
    }

    if (fd == -1) {
        msg_Err(intf, "Netsync socket failure");
        return VLC_EGENERIC;
    }

    intf->p_sys = sys = malloc(sizeof(*sys));
    if (!sys) {
        net_Close(fd);
        return VLC_ENOMEM;
    }

    sys->fd = fd;
    sys->is_master = var_InheritBool(intf, "netsync-master");
    sys->timeout = var_InheritInteger(intf, "netsync-timeout");
    if (sys->timeout < 500)
        sys->timeout = 500;
    sys->playlist = pl_Get(intf);
    sys->input = NULL;

    var_AddCallback(sys->playlist, "input-current", PlaylistEvent, intf);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close(vlc_object_t *object)
{
    intf_thread_t *intf = (intf_thread_t*)object;
    intf_sys_t *sys = intf->p_sys;

    var_DelCallback(sys->playlist, "input-current", PlaylistEvent, intf);

    if (sys->input != NULL) {
        vlc_cancel(sys->thread);
        vlc_join(sys->thread, NULL);
    }

    net_Close(sys->fd);
    free(sys);
}

static vlc_tick_t GetPcrSystem(input_thread_t *input)
{
    int canc = vlc_savecancel();
    /* TODO use the delay */
    vlc_tick_t system;
    if (input_GetPcrSystem(input, &system, NULL))
        system = -1;
    vlc_restorecancel(canc);

    return system;
}

static void *Master(void *handle)
{
    intf_thread_t *intf = handle;
    intf_sys_t *sys = intf->p_sys;
    for (;;) {
        struct pollfd ufd = { .fd = sys->fd, .events = POLLIN, };
        uint64_t data[2];

        if (poll(&ufd, 1, -1) < 0)
            continue;

        /* We received something */
        struct sockaddr_storage from;
        socklen_t fromlen = sizeof (from);

        if (recvfrom(sys->fd, data, 8, 0,
                     (struct sockaddr *)&from, &fromlen) < 8)
            continue;

        vlc_tick_t master_system = GetPcrSystem(sys->input);
        if (master_system < 0)
            continue;

        data[0] = hton64(vlc_tick_now());
        data[1] = hton64(master_system);

        /* Reply to the sender */
        sendto(sys->fd, data, 16, 0,
               (struct sockaddr *)&from, fromlen);
#if 0
        /* not sure we need the client information to sync,
           since we are the master anyway */
        vlc_tick_t client_system = ntoh64(data[0]);
        msg_Dbg(intf, "Master clockref: %"PRId64" -> %"PRId64", from %s "
                 "(date: %"PRId64")", client_system, master_system,
                 (from.ss_family == AF_INET) ? inet_ntoa(((struct sockaddr_in *)&from)->sin_addr)
                 : "non-IPv4", /*date*/ 0);
#endif
    }
    return NULL;
}

static void *Slave(void *handle)
{
    intf_thread_t *intf = handle;
    intf_sys_t *sys = intf->p_sys;

    for (;;) {
        struct pollfd ufd = { .fd = sys->fd, .events = POLLIN, };
        uint64_t data[2];

        vlc_tick_t system = GetPcrSystem(sys->input);
        if (system < 0)
            goto wait;

        /* Send clock request to the master */
        const vlc_tick_t send_date = vlc_tick_now();

        data[0] = hton64(system);
        send(sys->fd, data, 8, 0);

        /* Don't block */
        if (poll(&ufd, 1, sys->timeout) <= 0)
            continue;

        const vlc_tick_t receive_date = vlc_tick_now();
        if (recv(sys->fd, data, 16, 0) < 16)
            goto wait;

        const vlc_tick_t master_date   = ntoh64(data[0]);
        const vlc_tick_t master_system = ntoh64(data[1]);
        const vlc_tick_t diff_date = receive_date -
                                  ((receive_date - send_date) / 2 + master_date);

        if (master_system > 0) {
            int canc = vlc_savecancel();

            vlc_tick_t client_system;
            if (!input_GetPcrSystem(sys->input, &client_system, NULL)) {
                const vlc_tick_t diff_system = client_system - master_system - diff_date;
                if (diff_system != 0) {
                    input_ModifyPcrSystem(sys->input, true, master_system - diff_date);
#if 0
                    msg_Dbg(intf, "Slave clockref: %"PRId64" -> %"PRId64" -> %"PRId64","
                             " clock diff: %"PRId64", diff: %"PRId64"",
                             system, master_system, client_system,
                             diff_system, diff_date);
#endif
                }
            }
            vlc_restorecancel(canc);
        }
    wait:
        vlc_tick_sleep(INTF_IDLE_SLEEP);
    }
    return NULL;
}

static int PlaylistEvent(vlc_object_t *object, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data)
{
    VLC_UNUSED(cmd); VLC_UNUSED(object);
    intf_thread_t  *intf = data;
    intf_sys_t     *sys = intf->p_sys;
    input_thread_t *input = newval.p_address;

    if (sys->input != NULL) {
        msg_Err(intf, "InputEvent DEAD");
        assert(oldval.p_address == sys->input);

        vlc_cancel(sys->thread);
        vlc_join(sys->thread, NULL);
    }

    sys->input = input;

    if (input != NULL) {
        if (vlc_clone(&sys->thread, sys->is_master ? Master : Slave, intf,
                      VLC_THREAD_PRIORITY_INPUT))
            sys->input = NULL;
    }
    return VLC_SUCCESS;
}

