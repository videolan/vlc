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

struct vlc_inhibit_sys
{
    DBusConnection *conn;
    dbus_uint32_t cookie[2];
};

static void Inhibit (vlc_inhibit_t *ih, unsigned flags)
{
    enum {
        FREEDESKTOP = 0, /* as used by KDE and gnome <= 2.26 */
        GNOME       = 1, /* as used by gnome > 2.26 */
    };

    static const char dbus_service[2][32] = {
        [FREEDESKTOP]   = "org.freedesktop.PowerManagement",
        [GNOME]         = "org.gnome.SessionManager",
    };

    static const char dbus_path[2][33] = {
        [FREEDESKTOP]   = "/org/freedesktop/PowerManagement",
        [GNOME]         = "/org/gnome/SessionManager",
    };

    static const char dbus_interface[2][40] = {
        [FREEDESKTOP]   = "org.freedesktop.PowerManagement.Inhibit",
        [GNOME]         = "org.gnome.SessionManager",
    };

    static const char dbus_method[2][2][10] = {
        {
            [FREEDESKTOP]   = "UnInhibit",
            [GNOME]         = "Uninhibit",
        },
        {
            [FREEDESKTOP]   = "Inhibit",
            [GNOME]         = "Inhibit",
        },
    };

    static const char *app = PACKAGE;
    static const char *reason = N_("Playing some media.");

    vlc_inhibit_sys_t *sys = ih->p_sys;
    DBusConnection *conn = sys->conn;

    const bool suspend = !!flags;
    const dbus_uint32_t xid = 0; // FIXME ?
    const dbus_uint32_t gnome_flags = ((flags & VLC_INHIBIT_SUSPEND) ? 8 : 0)
        | ((flags & VLC_INHIBIT_DISPLAY) ? 4 : 0);
    for (int type = 0; type < 2; type++) {
        dbus_bool_t ret;

        DBusMessage *msg = dbus_message_new_method_call(dbus_service[type],
                dbus_path[type], dbus_interface[type], dbus_method[suspend][type]);
        if (unlikely(msg == NULL))
            return;

        if (suspend) {
            if (type == FREEDESKTOP)
                ret = dbus_message_append_args (msg, DBUS_TYPE_STRING, &app,
                        DBUS_TYPE_STRING, &reason,
                        DBUS_TYPE_INVALID);
            else if (type == GNOME)
                ret = dbus_message_append_args (msg, DBUS_TYPE_STRING, &app,
                        DBUS_TYPE_UINT32, &xid,
                        DBUS_TYPE_STRING, &reason,
                        DBUS_TYPE_UINT32, &gnome_flags,
                        DBUS_TYPE_INVALID);
        } else {
            ret = false;
            if (sys->cookie[type])
                ret = dbus_message_append_args (msg, DBUS_TYPE_UINT32,
                    &sys->cookie[type], DBUS_TYPE_INVALID);
        }

        if (!ret)
            goto end;

        if (suspend) { /* read reply */
            /* blocks 50ms maximum */
            DBusMessage *reply = dbus_connection_send_with_reply_and_block(
                    conn, msg, 50, NULL );

            if (unlikely(reply == NULL))
                goto end; /* gpm is not active, or too slow. Better luck next time? */

            if (!dbus_message_get_args(reply, NULL,
                                       DBUS_TYPE_UINT32, &sys->cookie[type],
                                       DBUS_TYPE_INVALID))
                sys->cookie[type] = 0;

            dbus_message_unref( reply );
        } else { /* just send and flush */
            if (dbus_connection_send (conn, msg, NULL)) {
                sys->cookie[type] = 0;
                dbus_connection_flush (conn);
            }
        }
end:
        dbus_message_unref (msg);
    }
}

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    DBusError err;

    dbus_error_init (&err);
    sys->conn = dbus_bus_get_private (DBUS_BUS_SESSION, &err);
    if (sys->conn == NULL)
    {
        msg_Err (obj, "cannot connect to session bus: %s", err.message);
        dbus_error_free (&err);
        free (sys);
        return VLC_EGENERIC;
    }
    sys->cookie[0] = 0;
    sys->cookie[1] = 0;

    ih->p_sys = sys;
    ih->inhibit = Inhibit;
    return VLC_SUCCESS;
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
