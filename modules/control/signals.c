/*****************************************************************************
 * signals.c : signals handler module for vlc
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <signal.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);
static void *SigThread (void *);

vlc_module_begin ()
    set_shortname (N_("Signals"))
    set_category (CAT_INTERFACE)
    set_subcategory (SUBCAT_INTERFACE_CONTROL)
    set_description (N_("POSIX signals handling interface"))
    set_capability ("interface", 0)
    set_callbacks (Open, Close)
vlc_module_end ()

struct intf_sys_t
{
    vlc_thread_t    thread;
};

static int Open (vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *p_sys = malloc (sizeof (*p_sys));

    if (p_sys == NULL)
        return VLC_ENOMEM;

    intf->p_sys = p_sys;

    if (vlc_clone (&p_sys->thread, SigThread, obj, VLC_THREAD_PRIORITY_LOW))
    {
        free (p_sys);
        intf->p_sys = NULL;
        return VLC_ENOMEM;
    }

    intf->pf_run = NULL;
    return 0;
}

static void Close (vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *p_sys = intf->p_sys;

    vlc_cancel (p_sys->thread);
#ifdef __APPLE__
   /* In Mac OS X up to 10.5 sigwait (among others) is not a pthread
    * cancellation point, so we throw a dummy quit signal to end
    * sigwait() in the sigth thread */
    pthread_kill (p_sys->thread, SIGQUIT);
# endif
    vlc_join (p_sys->thread, NULL);
    free (p_sys);
}

static void *SigThread (void *data)
{
    intf_thread_t *obj = data;
    sigset_t set;
    int signum;

    sigemptyset (&set);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);

    sigaddset (&set, SIGCHLD);

    do
    {
        while (sigwait (&set, &signum));

#ifdef __APPLE__
        /* In Mac OS X up to 10.5 sigwait (among others) is not a pthread
         * cancellation point */
        vlc_testcancel();
#endif
    }
    while (signum == SIGCHLD);

    msg_Err (obj, "Caught %s signal, exiting...", strsignal (signum));
    libvlc_Quit (obj->p_libvlc);

    /* After 3 seconds, fallback to normal signal handling */
    msleep (3 * CLOCK_FREQ);
    pthread_sigmask (SIG_UNBLOCK, &set, NULL);
    for (;;)
        pause ();
}
