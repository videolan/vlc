/*****************************************************************************
 * xdg-screensaver.c
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
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
#include <vlc_inhibit.h>
#include <assert.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("XDG-screensaver") )
    set_description (N_("XDG screen saver inhibition") )
    set_capability ("inhibit", 10 )
    set_callbacks (Open, Close)
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
vlc_module_end ()

struct vlc_inhibit_sys
{
    vlc_thread_t thread;
    vlc_cond_t update, inactive;
    vlc_mutex_t lock;
    posix_spawnattr_t attr;
    bool suspend, suspended;
};

static void Inhibit (vlc_inhibit_t *ih, bool suspend);
static void *Thread (void *);

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    ih->p_sys = p_sys;
    ih->inhibit = Inhibit;

    vlc_mutex_init (&p_sys->lock);
    vlc_cond_init (&p_sys->update);
    vlc_cond_init (&p_sys->inactive);
    posix_spawnattr_init (&p_sys->attr);
    /* Reset signal handlers to default and clear mask in the child process */
    {
        sigset_t set;

        sigemptyset (&set);
        posix_spawnattr_setsigmask (&p_sys->attr, &set);
        sigaddset (&set, SIGPIPE);
        posix_spawnattr_setsigdefault (&p_sys->attr, &set);
        posix_spawnattr_setflags (&p_sys->attr, POSIX_SPAWN_SETSIGDEF
                                              | POSIX_SPAWN_SETSIGMASK);
    }
    p_sys->suspend = false;
    p_sys->suspended = false;

    if (vlc_clone (&p_sys->thread, Thread, ih, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_cond_destroy (&p_sys->inactive);
        vlc_cond_destroy (&p_sys->update);
        vlc_mutex_destroy (&p_sys->lock);
        free (p_sys);
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *p_sys = ih->p_sys;

    /* Make sure xdg-screensaver is gone for good */
    vlc_mutex_lock (&p_sys->lock);
    while (p_sys->suspended)
        vlc_cond_wait (&p_sys->inactive, &p_sys->lock);
    vlc_mutex_unlock (&p_sys->lock);

    vlc_cancel (p_sys->thread);
    vlc_join (p_sys->thread, NULL);
    posix_spawnattr_destroy (&p_sys->attr);
    vlc_cond_destroy (&p_sys->inactive);
    vlc_cond_destroy (&p_sys->update);
    vlc_mutex_destroy (&p_sys->lock);
    free (p_sys);
}

static void Inhibit (vlc_inhibit_t *ih, bool suspend)
{
    vlc_inhibit_sys_t *p_sys = ih->p_sys;

    /* xdg-screensaver can take quite a while to start up (e.g. 1 second).
     * So we avoid _waiting_ for it unless we really need to (clean up). */
    vlc_mutex_lock (&p_sys->lock);
    p_sys->suspend = suspend;
    vlc_cond_signal (&p_sys->update);
    vlc_mutex_unlock (&p_sys->lock);
}

extern char **environ;

VLC_NORETURN
static void *Thread (void *data)
{
    vlc_inhibit_t *ih = data;
    vlc_inhibit_sys_t *p_sys = ih->p_sys;
    char id[11];

    snprintf (id, sizeof (id), "0x%08"PRIx32, ih->window_id);

    vlc_mutex_lock (&p_sys->lock);
    mutex_cleanup_push (&p_sys->lock);
    for (;;)
    {   /* TODO: detach the thread, so we don't need one at all time */
        while (p_sys->suspended == p_sys->suspend)
            vlc_cond_wait (&p_sys->update, &p_sys->lock);

        int canc = vlc_savecancel ();
        char *argv[4] = {
            (char *)"xdg-screensaver",
            (char *)(p_sys->suspend ? "suspend" : "resume"),
            id,
            NULL,
        };
        pid_t pid;

        vlc_mutex_unlock (&p_sys->lock);
        if (!posix_spawnp (&pid, "xdg-screensaver", NULL, &p_sys->attr,
                           argv, environ))
        {
            int status;

            msg_Dbg (ih, "started xdg-screensaver (PID = %d)", (int)pid);
            /* Wait for command to complete */
            while (waitpid (pid, &status, 0) == -1);
        }
        else/* We don't handle the error, but busy looping would be worse :( */
            msg_Warn (ih, "could not start xdg-screensaver");

        vlc_mutex_lock (&p_sys->lock);
        p_sys->suspended = p_sys->suspend;
        if (!p_sys->suspended)
            vlc_cond_signal (&p_sys->inactive);
        vlc_restorecancel (canc);
    }

    vlc_cleanup_pop ();
    assert (0);
}
