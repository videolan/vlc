/*****************************************************************************
 * dbus.c: power management inhibition using D-Bus
 *****************************************************************************
 * Copyright © 2009-2012 Rémi Denis-Courmont
 * Copyright © 2007-2012 Rafaël Carré
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

/*
 * Based on freedesktop Power Management Specification version 0.2
 * http://people.freedesktop.org/~hughsient/temp/power-management-spec-0.2.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>

#include <dbus/dbus.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>

enum vlc_inhibit_api
{
    FDO_SS, /**< KDE >= 4 and GNOME >= 3.10 */
    FDO_PM, /**< KDE and GNOME <= 2.26 */
    MATE,  /**< >= 1.0 */
    GNOME, /**< GNOME 2.26..3.4 */
};

#define MAX_API (GNOME+1)

/* Currently, all services have identical service and interface names. */
static const char dbus_service[][40] =
{
    [FDO_SS] = "org.freedesktop.ScreenSaver",
    [FDO_PM] = "org.freedesktop.PowerManagement.Inhibit",
    [MATE]   = "org.mate.SessionManager",
    [GNOME]  = "org.gnome.SessionManager",
};

static const char dbus_path[][33] =
{
    [FDO_SS] = "/ScreenSaver",
    [FDO_PM] = "/org/freedesktop/PowerManagement",
    [MATE]   = "/org/mate/SessionManager",
    [GNOME]  = "/org/gnome/SessionManager",
};

static const char dbus_method_uninhibit[][10] =
{
    [FDO_SS] = "UnInhibit",
    [FDO_PM] = "UnInhibit",
    [MATE]   = "Uninhibit",
    [GNOME]  = "Uninhibit",
};

struct vlc_inhibit_sys
{
    DBusConnection *conn;
    DBusPendingCall *pending;
    dbus_uint32_t cookie;
    enum vlc_inhibit_api api;
};

static void Inhibit(vlc_inhibit_t *ih, unsigned flags)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;
    enum vlc_inhibit_api type = sys->api;

    /* Receive reply from previous request, possibly hours later ;-) */
    if (sys->pending != NULL)
    {
        DBusMessage *reply;

        /* NOTE: Unfortunately, the pending reply cannot simply be cancelled.
         * Otherwise, the cookie would be lost and inhibition would remain on
         * (until complete disconnection from the bus). */
        dbus_pending_call_block(sys->pending);
        reply = dbus_pending_call_steal_reply(sys->pending);
        dbus_pending_call_unref(sys->pending);
        sys->pending = NULL;

        if (reply != NULL)
        {
            if (!dbus_message_get_args(reply, NULL,
                                       DBUS_TYPE_UINT32, &sys->cookie,
                                       DBUS_TYPE_INVALID))
                sys->cookie = 0;
            dbus_message_unref(reply);
        }
        msg_Dbg(ih, "got cookie %"PRIu32, (uint32_t)sys->cookie);
    }

    /* FIXME: This check is incorrect if flags change from one non-zero value
     * to another one. But the D-Bus API cannot currently inhibit suspend
     * independently from the screensaver. */
    if (!sys->cookie == !flags)
        return; /* nothing to do */

    /* Send request */
    const char *method = flags ? "Inhibit" : dbus_method_uninhibit[type];
    dbus_bool_t ret;

    DBusMessage *msg = dbus_message_new_method_call(dbus_service[type],
                                  dbus_path[type], dbus_service[type], method);
    if (unlikely(msg == NULL))
        return;

    if (flags)
    {
        const char *app = PACKAGE;
        const char *reason = _("Playing some media.");

        assert(sys->cookie == 0);

        switch (type)
        {
            case MATE:
            case GNOME:
            {
                dbus_uint32_t xid = 0; // FIXME ?
                dbus_uint32_t gflags = 0xC;

                ret = dbus_message_append_args(msg, DBUS_TYPE_STRING, &app,
                                                    DBUS_TYPE_UINT32, &xid,
                                                    DBUS_TYPE_STRING, &reason,
                                                    DBUS_TYPE_UINT32, &gflags,
                                                    DBUS_TYPE_INVALID);
                break;
            }
            default:
                ret = dbus_message_append_args(msg, DBUS_TYPE_STRING, &app,
                                                    DBUS_TYPE_STRING, &reason,
                                                    DBUS_TYPE_INVALID);
                break;
        }

        if (!ret
        || !dbus_connection_send_with_reply(sys->conn, msg, &sys->pending, -1))
            sys->pending = NULL;
    }
    else
    {
        assert(sys->cookie != 0);
        if (dbus_message_append_args(msg, DBUS_TYPE_UINT32, &sys->cookie,
                                           DBUS_TYPE_INVALID)
         && dbus_connection_send (sys->conn, msg, NULL))
            sys->cookie = 0;
    }
    dbus_connection_flush(sys->conn);
    dbus_message_unref(msg);
}

static void Close(vlc_object_t *obj);

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    DBusError err;
    dbus_error_init(&err);

    sys->conn = dbus_bus_get_private (DBUS_BUS_SESSION, &err);
    if (sys->conn == NULL)
    {
        msg_Err(ih, "cannot connect to session bus: %s", err.message);
        dbus_error_free(&err);
        free(sys);
        return VLC_EGENERIC;
    }

    sys->pending = NULL;
    sys->cookie = 0;
    ih->p_sys = sys;

    for (unsigned i = 0; i < MAX_API; i++)
    {
        if (dbus_bus_name_has_owner(sys->conn, dbus_service[i], NULL))
        {
            msg_Dbg(ih, "found service %s", dbus_service[i]);
            sys->api = i;
            ih->inhibit = Inhibit;
            return VLC_SUCCESS;
        }

        msg_Dbg(ih, "cannot find service %s", dbus_service[i]);
    }

    Close(obj);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys;

    if (sys->pending != NULL)
    {
        dbus_pending_call_cancel(sys->pending);
        dbus_pending_call_unref(sys->pending);
    }
    dbus_connection_close (sys->conn);
    dbus_connection_unref (sys->conn);
    free (sys);
}

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("D-Bus screensaver"))
    set_description (N_("D-Bus screen saver inhibition"))
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
    set_capability ("inhibit", 20)
    set_callbacks (Open, Close)
vlc_module_end ()
