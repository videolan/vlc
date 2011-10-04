/**
 * @file mce.c
 * @brief Nokia MCE screen unblanking for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009-2011 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>
#include <dbus/dbus.h>

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("MCE"))
    set_description (N_("Nokia MCE screen unblanking"))
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
    set_capability ("inhibit", 20)
    set_callbacks (Open, Close)
vlc_module_end ()

static void Inhibit (vlc_inhibit_t *, bool);
static void Timer (void *data);

struct vlc_inhibit_sys
{
    DBusConnection *conn;
    vlc_timer_t timer;
};

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    DBusError err;

    dbus_error_init (&err);
    sys->conn = dbus_bus_get_private (DBUS_BUS_SYSTEM, &err);
    if (sys->conn == NULL)
    {
        msg_Err (obj, "cannot connect to system bus: %s", err.message);
        dbus_error_free (&err);
        goto error;
    }

    if (vlc_timer_create (&sys->timer, Timer, sys->conn))
    {
        dbus_connection_unref (sys->conn);
        goto error;
    }

    ih->p_sys = sys;
    ih->inhibit = Inhibit;
    return VLC_SUCCESS;

error:
    free (sys);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys;

    vlc_timer_destroy (sys->timer);
    dbus_connection_close (sys->conn);
    dbus_connection_unref (sys->conn);
    free (sys);
}

static void Inhibit (vlc_inhibit_t *ih, bool unblank)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;

    /* The shortest blanking interval is 10s on N900, 15s on N9 */
    const mtime_t interval = 9 * CLOCK_FREQ;
    vlc_timer_schedule (sys->timer, false, unblank, interval);
}

/* NOTE: This plug-in could be compiled without MCE development files easily.
 * But then it would get included on all platforms with D-Bus. */
#include <mce/dbus-names.h>

static void Timer (void *data)
{
    DBusConnection *conn = data;
    DBusMessage *msg = dbus_message_new_method_call (MCE_SERVICE,
                                                     MCE_REQUEST_PATH,
                                                     MCE_REQUEST_IF,
                                                     MCE_DISPLAY_ON_REQ);
    if (unlikely(msg == NULL))
        return;

    if (dbus_connection_send (conn, msg, NULL))
        dbus_connection_flush (conn);
    dbus_message_unref (msg);
}
