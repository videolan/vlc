/*****************************************************************************
 * netsync.c: synchronisation between several network clients.
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id$
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_playlist.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_POLL
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
#define NETSYNC_LONGTEXT N_("When set then " \
  "this vlc instance shall dictate its clock for synchronisation" \
  "over clients listening on the masters network ip address")

#define MIP_TEXT N_("Master server ip address")
#define MIP_LONGTEXT N_("The IP address of " \
  "the network master clock to use for clock synchronisation.")

#define NETSYNC_TIMEOUT_TEXT N_("UDP timeout (in ms)")
#define NETSYNC_TIMEOUT_LONGTEXT N_("Amount of time (in ms) " \
  "to wait before aborting network reception of data.")

vlc_module_begin()
    set_shortname(N_("Network Sync"))
    set_description(N_("Network synchronisation"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)

    add_bool("netsync-master", false, NULL,
              NETSYNC_TEXT, NETSYNC_LONGTEXT, true)
    add_string("netsync-master-ip", NULL, NULL, MIP_TEXT, MIP_LONGTEXT,
                true)
    add_integer("netsync-timeout", 500, NULL,
                 NETSYNC_TIMEOUT_TEXT, NETSYNC_TIMEOUT_LONGTEXT, true)

    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run(intf_thread_t *intf);

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    intf_thread_t *intf = (intf_thread_t*)object;
    int fd;

    if (!var_InheritBool(intf, "netsync-master")) {
        char *psz_master = var_InheritString(intf, "netsync-master-ip");
        if (psz_master == NULL) {
            msg_Err(intf, "master address not specified");
            return VLC_EGENERIC;
        }
        fd = net_ConnectUDP(VLC_OBJECT(intf), psz_master, NETSYNC_PORT, -1);
        free(psz_master);
    }
    else
        fd = net_ListenUDP1(VLC_OBJECT(intf), NULL, NETSYNC_PORT);

    if (fd == -1) {
        msg_Err(intf, "Netsync socket failure");
        return VLC_EGENERIC;
    }

    intf->p_sys = (void *)(intptr_t)fd;
    intf->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close(vlc_object_t *object)
{
    intf_thread_t *intf = (intf_thread_t*)object;

    net_Close((intptr_t)intf->p_sys);
}

/*****************************************************************************
 * Run: interface thread
 *****************************************************************************/
static void Run(intf_thread_t *intf)
{
#define MAX_MSG_LENGTH (2 * sizeof(int64_t))
    int canc = vlc_savecancel();
    input_thread_t *input = NULL;
    char data[MAX_MSG_LENGTH];
    int fd = (intptr_t)intf->p_sys;

    playlist_t *playlist = pl_Hold(intf);
    int timeout = var_InheritInteger(intf, "netsync-timeout");
    if (timeout < 500)
        timeout = 500;
    bool is_master = var_InheritBool(intf, "netsync-master");

    /* High priority thread */
    vlc_thread_set_priority(intf, VLC_THREAD_PRIORITY_INPUT);

    while (vlc_object_alive(intf)) {
        /* Update the input */
        if (input == NULL) {
            input = playlist_CurrentInput(playlist);
        } else if (input->b_dead || !vlc_object_alive(input)) {
            vlc_object_release(input);
            input = NULL;
        }

        if (input == NULL) {
            /* Wait a bit */
            msleep(INTF_IDLE_SLEEP);
            continue;
        }

        /*
         * We now have an input
         */

        /* Initialize file descriptor set and timeout (0.5s) */
        /* FIXME: arbitrary tick */
        struct pollfd ufd = { .fd = fd, .events = POLLIN, };

        if (is_master) {
            struct sockaddr_storage from;
            mtime_t master_system;
            mtime_t client_system;
            mtime_t date;
            int struct_size, read_size, ret;

            /* Don't block */
            ret = poll(&ufd, 1, timeout);
            if (ret <= 0)
                continue;

            /* We received something */
            struct_size = sizeof(from);
            read_size = recvfrom(fd, data, MAX_MSG_LENGTH, 0,
                                 (struct sockaddr*)&from,
                                 (unsigned int *)&struct_size);

            /* not sure we need the client information to sync,
               since we are the master anyway */
            client_system = ntoh64(*(int64_t *)data);

            date = mdate();

            if (input_GetPcrSystem(input, &master_system))
                continue;

            *((int64_t *)data) = hton64(date);
            *(((int64_t *)data)+1) = hton64(master_system);

            /* Reply to the sender */
            sendto(fd, data, 2 * sizeof(int64_t), 0,
                    (struct sockaddr *)&from, struct_size);

#if 0
            msg_Dbg(intf, "Master clockref: %"PRId64" -> %"PRId64", from %s "
                     "(date: %"PRId64")", client_system, master_system,
                     (from.ss_family == AF_INET) ? inet_ntoa(((struct sockaddr_in *)&from)->sin_addr)
                     : "non-IPv4", date);
#endif
        }
        else
        {
            mtime_t master_system;
            mtime_t client_system;
            mtime_t system = 0;
            mtime_t send_date, receive_date;
            mtime_t diff_date, master_date;
            int sent, read_size, ret;

            if (input_GetPcrSystem(input, &system)) {
                msleep(INTF_IDLE_SLEEP);
                continue;
            }

            /* Send clock request to the master */
            send_date = mdate();
            *((int64_t *)data) = hton64(system);

            sent = send(fd, data, sizeof(int64_t), 0);
            if (sent <= 0) {
                msleep(INTF_IDLE_SLEEP);
                continue;
            }

            /* Don't block */
            ret = poll(&ufd, 1, timeout);
            if (ret == 0)
                continue;
            if (ret < 0) {
                msleep(INTF_IDLE_SLEEP);
                continue;
            }

            receive_date = mdate();
            read_size = recv(fd, data, MAX_MSG_LENGTH, 0);
            if (read_size <= 0) {
                msleep(INTF_IDLE_SLEEP);
                continue;
            }

            master_date = ntoh64(*(int64_t *)data);
            master_system = ntoh64(*(((int64_t *)data)+1)); /* system date */

            diff_date = receive_date -
                          ((receive_date - send_date) / 2 + master_date);

            if (input && master_system > 0) {
                mtime_t diff_system;

                if (input_GetPcrSystem(input, &client_system)) {
                    msleep(INTF_IDLE_SLEEP);
                    continue;
                }

                diff_system = client_system - master_system - diff_date;
                if (diff_system != 0) {
                    input_ModifyPcrSystem(input, true, master_system - diff_date);
#if 0
                    msg_Dbg(intf, "Slave clockref: %"PRId64" -> %"PRId64" -> %"PRId64","
                             " clock diff: %"PRId64", diff: %"PRId64"",
                             system, master_system, client_system,
                             diff_system, diff_date);
#endif
                }
            }
            msleep(INTF_IDLE_SLEEP);
        }
    }

    if (input)
        vlc_object_release(input);
    pl_Release(intf);
    vlc_restorecancel(canc);
}

