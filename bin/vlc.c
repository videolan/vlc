/*****************************************************************************
 * vlc.c: the VLC player
 *****************************************************************************
 * Copyright (C) 1998-2013 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
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

#include <vlc/vlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif
#include <unistd.h>

#ifdef __OS2__
# include <iconv.h>

# define pthread_t      int
# define pthread_self() _gettid()

static char *FromSystem(const void *str)
{
    iconv_t handle = iconv_open ("UTF-8", "");
    if (handle == (iconv_t)(-1))
        return NULL;

    size_t str_len = strlen (str);
    char *out = NULL;
    for (unsigned mul = 4; mul < 8; mul++)
    {
        size_t in_size = str_len;
        const char *in = str;
        size_t out_max = mul * str_len;
        char *tmp = out = malloc (1 + out_max);
        if (!out)
            break;

        if (iconv (handle, &in, &in_size, &tmp, &out_max) != (size_t)(-1)) {
            *tmp = '\0';
            break;
        }
        free(out);
        out = NULL;

        if (errno != E2BIG)
            break;
    }
    iconv_close(handle);
    return out;
}
#endif

extern void vlc_enable_override (void);

static bool signal_ignored (int signum)
{
    struct sigaction sa;

    if (sigaction (signum, NULL, &sa))
        return false;
    return ((sa.sa_flags & SA_SIGINFO)
            ? (void *)sa.sa_sigaction : (void *)sa.sa_handler) == SIG_IGN;
}

static void vlc_kill (void *data)
{
#ifndef __OS2__
    pthread_t *ps = data;

    pthread_kill (*ps, SIGTERM);
#else
    // send a signal to the main thread
    kill (getpid(), SIGTERM);
#endif
}

static void exit_timeout (int signum)
{
    (void) signum;
    signal (SIGINT, SIG_DFL);
}

/*****************************************************************************
 * main: parse command line, start interface and spawn threads.
 *****************************************************************************/
int main(int argc, const char *argv[])
{
    /* The so-called POSIX-compliant MacOS X reportedly processes SIGPIPE even
     * if it is blocked in all thread.
     * Note: this is NOT an excuse for not protecting against SIGPIPE. If
     * LibVLC runs outside of VLC, we cannot rely on this code snippet. */
    signal (SIGPIPE, SIG_IGN);
    /* Restore SIGCHLD in case our parent process ignores it. */
    signal (SIGCHLD, SIG_DFL);

#ifndef NDEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    setenv ("MALLOC_CHECK_", "2", 1);

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    setenv ("GNOME_DISABLE_CRASH_DIALOG", "1", 1);
#endif

#ifdef TOP_BUILDDIR
    setenv ("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv ("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
    setenv ("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 1);
#endif

    /* Clear the X.Org startup notification ID. Otherwise the UI might try to
     * change the environment while the process is multi-threaded. That could
     * crash. Screw you X.Org. Next time write a thread-safe specification. */
    unsetenv ("DESKTOP_STARTUP_ID");

#ifndef ALLOW_RUN_AS_ROOT
    if (geteuid () == 0)
    {
        fprintf (stderr, "VLC is not supposed to be run as root. Sorry.\n"
        "If you need to use real-time priorities and/or privileged TCP ports\n"
        "you can use %s-wrapper (make sure it is Set-UID root and\n"
        "cannot be run by non-trusted users first).\n", argv[0]);
        return 1;
    }
#endif

    setlocale (LC_ALL, "");

    if (isatty (STDERR_FILENO))
        /* This message clutters error logs. It is printed only on a TTY.
         * Fortunately, LibVLC prints version info with -vv anyway. */
        fprintf (stderr, "VLC media player %s (revision %s)\n",
                 libvlc_get_version(), libvlc_get_changeset());

    sigset_t set;

    sigemptyset (&set);
    /* VLC uses sigwait() to dequeue interesting signals.
     * For this to work, those signals must be blocked in all threads,
     * including the thread calling sigwait() (see the man page for details).
     *
     * There are two advantages to sigwait() over traditional signal handlers:
     *  - delivery is synchronous: no need to worry about async-safety,
     *  - EINTR is not generated: other threads need not handle that error.
     * That being said, some LibVLC programs do not use sigwait(). Therefore
     * EINTR must still be handled cleanly, notably from poll() calls.
     *
     * Signals that request a clean shutdown, and force an unclean shutdown
     * if they are triggered again 2+ seconds later.
     * We have to handle SIGTERM cleanly because of daemon mode. */
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);

    /* SIGPIPE can happen and would crash the process. On modern systems,
     * the MSG_NOSIGNAL flag protects socket write operations against SIGPIPE.
     * But we still need to block SIGPIPE when:
     *  - writing to pipes,
     *  - using write() instead of send() for code not specific to sockets.
     * LibVLC code assumes that SIGPIPE is blocked. Other LibVLC applications
     * shall block it (or handle it somehow) too.
     */
    sigaddset (&set, SIGPIPE);

    /* SIGCHLD must be dequeued to clean up zombie child processes.
     * Furthermore the handler must not be set to SIG_IGN (see above).
     * We cannot pragmatically handle EINTR, short reads and short writes
     * in every code paths (including underlying libraries). So we just
     * block SIGCHLD in all threads, and dequeue it below. */
    sigaddset (&set, SIGCHLD);

    /* Block all these signals */
    pthread_t self = pthread_self ();
    pthread_sigmask (SIG_SETMASK, &set, NULL);

    const char *args[argc + 3];
    int count = 0;

    args[count++] = "--no-ignore-config";
    args[count++] = "--media-library";
#ifdef HAVE_DBUS
    args[count++] = "--dbus";
#endif

#ifdef __OS2__
    for (int i = 1; i < argc; i++)
        if ((args[count++] = FromSystem(argv[i])) == NULL)
        {
            fprintf (stderr, "Converting '%s' to UTF-8 failed.\n", argv[i]);
            return 1;
        }
#else
    memcpy(args + count, argv + 1, (argc - 1) * sizeof (*argv));
    count += (argc - 1);
#endif
    args[count] = NULL;

    vlc_enable_override ();

    /* Initialize libvlc */
    libvlc_instance_t *vlc = libvlc_new(count, args);
    if (vlc == NULL)
        return 1;

    int ret = 1;
    libvlc_set_exit_handler (vlc, vlc_kill, &self);
    libvlc_set_app_id (vlc, "org.VideoLAN.VLC", PACKAGE_VERSION, PACKAGE_NAME);
    libvlc_set_user_agent (vlc, "VLC media player", "VLC/"PACKAGE_VERSION);

    libvlc_add_intf (vlc, "hotkeys,none");
#if !defined (__OS2__)
    libvlc_add_intf (vlc, "globalhotkeys,none");
#endif
    if (libvlc_add_intf (vlc, NULL))
    {
        fprintf(stderr, "%s: cannot start any interface. Exiting.\n", argv[0]);
        goto out;
    }

    libvlc_playlist_play (vlc);

    /* Qt insists on catching SIGCHLD via signal handler. To work around that,
     * unblock it after all our child threads are created. */
    sigdelset (&set, SIGCHLD);
    pthread_sigmask (SIG_SETMASK, &set, NULL);

    /* Do not dequeue SIGHUP if it is ignored (nohup) */
    if (signal_ignored (SIGHUP))
        sigdelset (&set, SIGHUP);
    /* Ignore SIGPIPE */
    sigdelset (&set, SIGPIPE);

    int signum;
    sigwait (&set, &signum);

    /* Restore default signal behaviour after 3 seconds */
    sigemptyset (&set);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGALRM);
    signal (SIGINT, SIG_IGN);
    signal (SIGALRM, exit_timeout);
    pthread_sigmask (SIG_UNBLOCK, &set, NULL);
    alarm (3);

    ret = 0;
    /* Cleanup */
out:
    libvlc_release (vlc);
#ifdef __OS2__
    for (int i = count - argc + 1; i < count; i++)
        free(args[i]);
#endif
    return ret;
}
