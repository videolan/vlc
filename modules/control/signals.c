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

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);
static void Run (intf_thread_t *);
static void *SigThread (void *);

vlc_module_begin ();
    set_shortname (N_("Signals"));
    set_category (CAT_INTERFACE);
    set_subcategory (SUBCAT_INTERFACE_CONTROL);
    set_description (N_("POSIX signals handling interface"));
    set_capability ("interface", 0);
    set_callbacks (Open, Close);
vlc_module_end ();

struct intf_sys_t
{
    pthread_t       thread;
    int             signum;
};

static int Open (vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *p_sys = malloc (sizeof (*p_sys));

    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->signum = 0;
    intf->p_sys = p_sys;

    if (pthread_create (&p_sys->thread, NULL, SigThread, obj))
    {
        free (p_sys);
        intf->p_sys = NULL;
        return VLC_ENOMEM;
    }

    intf->pf_run = Run;
    return 0;
}

static void Close (vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *p_sys = intf->p_sys;

    pthread_cancel (p_sys->thread);
#ifdef __APPLE__
   /* In Mac OS X up to 10.5 sigwait (among others) is not a pthread
    * cancellation point, so we throw a dummy quit signal to end
    * sigwait() in the sigth thread */
    pthread_kill (p_sys->thread, SIGQUIT);
# endif
    pthread_join (p_sys->thread, NULL);
    free (p_sys);
}

static void *SigThread (void *data)
{
    intf_thread_t *obj = data;
    intf_sys_t *p_sys = obj->p_sys;
    sigset_t set;

    sigemptyset (&set);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);

    sigaddset (&set, SIGCHLD);

    for (;;)
    {
        int signum;

        sigwait (&set, &signum);

#ifdef __APPLE__
        /* In Mac OS X up to 10.5 sigwait (among others) is not a pthread
         * cancellation point */
        pthread_testcancel();
#endif

        vlc_object_lock (obj);
        p_sys->signum = signum;
        vlc_object_signal_unlocked (obj);
        vlc_object_unlock (obj);
    }
}

static void Run (intf_thread_t *obj)
{
    intf_sys_t *p_sys = obj->p_sys;

    vlc_object_lock (obj);
    while (vlc_object_alive (obj))
    {
        switch (p_sys->signum)
        {
            case SIGINT:
            case SIGHUP:
            case SIGTERM:
            case SIGQUIT:
                msg_Err (obj, "Caught %s signal, exiting...",
                         strsignal (p_sys->signum));
                goto out;
        }
        vlc_object_wait (obj);
    }

out:
    vlc_object_unlock (obj);
    vlc_object_kill (obj->p_libvlc);
}
