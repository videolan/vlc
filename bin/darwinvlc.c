/*****************************************************************************
 * darwinvlc.c: the darwin-specific VLC player
 *****************************************************************************
 * Copyright (C) 1998-2013 the VideoLAN team
 * $Id$
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
#include <TargetConditionals.h>
#import <CoreFoundation/CoreFoundation.h>

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
    pthread_t *ps = data;
    pthread_kill (*ps, SIGTERM);
}

static void exit_timeout (int signum)
{
    (void) signum;
    signal (SIGINT, SIG_DFL);
}

/*****************************************************************************
 * main: parse command line, start interface and spawn threads.
 *****************************************************************************/
int main( int i_argc, const char *ppsz_argv[] )
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
#endif

#ifdef TOP_BUILDDIR
    setenv ("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv ("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
#endif

#ifndef ALLOW_RUN_AS_ROOT
    if (geteuid () == 0)
    {
        fprintf (stderr, "VLC is not supposed to be run as root. Sorry.\n"
        "If you need to use real-time priorities and/or privileged TCP ports\n"
        "you can use %s-wrapper (make sure it is Set-UID root and\n"
        "cannot be run by non-trusted users first).\n", ppsz_argv[0]);
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

    const char *argv[i_argc + 2];
    int argc = 0;

    argv[argc++] = "--no-ignore-config";
    argv[argc++] = "--media-library";

    /* overwrite system language on Mac */
#if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR // TARGET_OS_MAC is unspecific
    char *lang = NULL;

    for (int i = 0; i < i_argc; i++) {
        if (!strncmp(ppsz_argv[i], "--language", 10)) {
            lang = strstr(ppsz_argv[i], "=");
            ppsz_argv++, i_argc--;
            continue;
        }
    }
    if (lang && strncmp( lang, "auto", 4 )) {
        char tmp[11];
        snprintf(tmp, 11, "LANG%s", lang);
        putenv(tmp);
    }

    if (!lang) {
        CFStringRef language;
        language = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("language"),
                                                          kCFPreferencesCurrentApplication);
        if (language) {
            CFIndex length = CFStringGetLength(language) + 1;
            if (length > 0) {
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                lang = (char *)malloc(maxSize);
                CFStringGetCString(language, lang, maxSize - 1, kCFStringEncodingUTF8);
            }
            if (strncmp( lang, "auto", 4 )) {
                char tmp[11];
                snprintf(tmp, 11, "LANG=%s", lang);
                putenv(tmp);
            }
            CFRelease(language);
        }
    }
#endif

    ppsz_argv++; i_argc--; /* skip executable path */

    /* When VLC.app is run by double clicking in Mac OS X, the 2nd arg
     * is the PSN - process serial number (a unique PID-ish thingie)
     * still ok for real Darwin & when run from command line
     * for example -psn_0_9306113 */
    if (i_argc >= 1 && !strncmp (*ppsz_argv, "-psn" , 4))
        ppsz_argv++, i_argc--;

    memcpy (argv + argc, ppsz_argv, i_argc * sizeof (*argv));
    argc += i_argc;
    argv[argc] = NULL;

    vlc_enable_override ();

    /* Initialize libvlc */
    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    if (vlc == NULL)
        return 1;

    int ret = 1;
    libvlc_set_exit_handler (vlc, vlc_kill, &self);
    libvlc_set_app_id (vlc, "org.VideoLAN.VLC", PACKAGE_VERSION, PACKAGE_NAME);
    libvlc_set_user_agent (vlc, "VLC media player", "VLC/"PACKAGE_VERSION);

    libvlc_add_intf (vlc, "hotkeys,none");

    if (libvlc_add_intf (vlc, NULL))
        goto out;

    libvlc_playlist_play (vlc, -1, 0, NULL);

    /* Qt4 insists on catching SIGCHLD via signal handler. To work around that,
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

    return ret;
}
