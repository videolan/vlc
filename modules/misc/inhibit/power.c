/*****************************************************************************
 * power.c : prevents the computer from suspending when VLC is playing
 *****************************************************************************
 * Copyright © 2009-2011 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>
#include <dbus/dbus.h>

enum vlc_inhibit_api
{
    FREEDESKTOP, /* KDE and GNOME <= 2.26 */
    GNOME, /**< GNOME 2.26..3.4 */
};

#define MAX_API (GNOME+1)

static const char dbus_service[][32] =
{
    [FREEDESKTOP] = "org.freedesktop.PowerManagement",
    [GNOME]       = "org.gnome.SessionManager",
};

static const char dbus_path[][33] =
{
    [FREEDESKTOP] = "/org/freedesktop/PowerManagement",
    [GNOME]       = "/org/gnome/SessionManager",
};

static const char dbus_interface[][40] =
{
    [FREEDESKTOP] = "org.freedesktop.PowerManagement.Inhibit",
    [GNOME]       = "org.gnome.SessionManager",
};

static const char dbus_method_uninhibit[][10] =
{
    [FREEDESKTOP] = "UnInhibit",
    [GNOME]       = "Uninhibit",
};

struct vlc_inhibit_sys
{
    DBusConnection *conn;
    dbus_uint32_t cookie;
    enum vlc_inhibit_api api;
};

static void Inhibit(vlc_inhibit_t *ih, unsigned flags)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;
    DBusConnection *conn = sys->conn;
    enum vlc_inhibit_api type = sys->api;

    const char *method = flags ? "Inhibit" : dbus_method_uninhibit[type];
    dbus_bool_t ret;

    DBusMessage *msg = dbus_message_new_method_call(dbus_service[type],
                                dbus_path[type], dbus_interface[type], method);
    if (unlikely(msg == NULL))
        return;

    if (flags) {
        const char *app = PACKAGE;
        const char *reason = _("Playing some media.");

        switch (type)
        {
            case FREEDESKTOP:
                ret = dbus_message_append_args(msg, DBUS_TYPE_STRING, &app,
                                                    DBUS_TYPE_STRING, &reason,
                                                    DBUS_TYPE_INVALID);
                break;
            case GNOME:
            {
                dbus_uint32_t xid = 0; // FIXME ?
                dbus_uint32_t gflags =
                   ((flags & VLC_INHIBIT_SUSPEND) ? 8 : 0) |
                   ((flags & VLC_INHIBIT_DISPLAY) ? 4 : 0);

                ret = dbus_message_append_args(msg, DBUS_TYPE_STRING, &app,
                                                    DBUS_TYPE_UINT32, &xid,
                                                    DBUS_TYPE_STRING, &reason,
                                                    DBUS_TYPE_UINT32, &gflags,
                                                    DBUS_TYPE_INVALID);
                break;
            }
        }
    } else {
        if (sys->cookie)
            ret = dbus_message_append_args(msg, DBUS_TYPE_UINT32, &sys->cookie,
                                                DBUS_TYPE_INVALID);
        else
            ret = false;
    }

    if (!ret)
        goto giveup;

    if (flags) { /* read reply */
        DBusMessage *reply = dbus_connection_send_with_reply_and_block(
                                                          conn, msg, 50, NULL);
        if (unlikely(reply == NULL))
            goto giveup; /* no reponse?! */

        if (!dbus_message_get_args(reply, NULL,
                                   DBUS_TYPE_UINT32, &sys->cookie,
                                   DBUS_TYPE_INVALID))
            sys->cookie = 0;

        dbus_message_unref(reply);
    } else { /* just send and flush */
        if (dbus_connection_send (conn, msg, NULL)) {
            sys->cookie = 0;
            dbus_connection_flush(conn);
        }
    }
giveup:
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

    sys->cookie = 0;

    for (unsigned i = 0; i < MAX_API; i++)
    {
        if (dbus_bus_name_has_owner(sys->conn, dbus_service[i], &err))
        {
            msg_Dbg(ih, "found service %s", dbus_service[i]);
            sys->api = i;
            ih->p_sys = sys;
            ih->inhibit = Inhibit;
            return VLC_SUCCESS;
        }

        msg_Dbg(ih, "cannot find service %s: %s", dbus_service[i],
                err.message);
        dbus_error_free(&err);
    }

    Close(obj);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys;

    dbus_connection_close (sys->conn);
    dbus_connection_unref (sys->conn);
    free (sys);
}

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Power"))
    set_description (N_("Inhibits power suspend and session idle timeout."))
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
    set_capability ("inhibit", 20)
    set_callbacks (Open, Close)
vlc_module_end ()
