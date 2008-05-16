/*****************************************************************************
 * vlc.c: the vlc player
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
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
#include <locale.h>


/* Explicit HACK */
extern void LocaleFree (const char *);
extern char *FromLocale (const char *);


/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#ifdef WIN32
#include <windows.h>
extern void __wgetmainargs(int *argc, wchar_t ***wargv, wchar_t ***wenviron,
                           int expand_wildcards, int *startupinfo);
#else

# include <signal.h>
# include <time.h>
# include <pthread.h>

static void *SigHandler (void *set);
#endif

/*****************************************************************************
 * main: parse command line, start interface and spawn threads.
 *****************************************************************************/
int main( int i_argc, const char *ppsz_argv[] )
{
    int i_ret;

    setlocale (LC_ALL, "");

#ifndef __APPLE__
    /* This clutters OSX GUI error logs */
    fprintf( stderr, "VLC media player %s\n", libvlc_get_version() );
#endif

#ifdef HAVE_PUTENV
#   ifndef NDEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( (char*)"MALLOC_CHECK_=2" );
#       ifdef __APPLE__
    putenv( (char*)"MallocErrorAbort=crash_my_baby_crash" );
#       endif

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    putenv( (char *)"GNOME_DISABLE_CRASH_DIALOG=1" );
#   endif
#endif

#if defined (HAVE_GETEUID) && !defined (SYS_BEOS)
    /* FIXME: rootwrap (); */
#endif

#if !defined(WIN32) && !defined(UNDER_CE)
    /* Synchronously intercepted POSIX signals.
     *
     * In a threaded program such as VLC, the only sane way to handle signals
     * is to block them in all thread but one - this is the only way to
     * predict which thread will receive them. If any piece of code depends
     * on delivery of one of this signal it is intrinsically not thread-safe
     * and MUST NOT be used in VLC, whether we like it or not.
     * There is only one exception: if the signal is raised with
     * pthread_kill() - we do not use this in LibVLC but some pthread
     * implementations use them internally. You should really use conditions
     * for thread synchronization anyway.
     *
     * Signal that request a clean shutdown, and force an unclean shutdown
     * if they are triggered again 2+ seconds later.
     * We have to handle SIGTERM cleanly because of daemon mode.
     * Note that we set the signals after the vlc_create call. */
    static const int exitsigs[] = { SIGINT, SIGHUP, SIGQUIT, SIGTERM };
    /* Signals that cause a no-op:
     * - SIGALRM should not happen, but lets stay on the safe side.
     * - SIGPIPE might happen with sockets and would crash VLC. It MUST be
     *   blocked by any LibVLC-dependant application, in addition to VLC.
     * - SIGCHLD is comes after exec*() (such as httpd CGI support) and must
     *   be dequeued to cleanup zombie processes.
     */
    static const int dummysigs[] = { SIGALRM, SIGPIPE, SIGCHLD };

    sigset_t set;
    pthread_t sigth;

    sigemptyset (&set);
    for (unsigned i = 0; i < sizeof (exitsigs) / sizeof (exitsigs[0]); i++)
        sigaddset (&set, exitsigs[i]);
    for (unsigned i = 0; i < sizeof (dummysigs) / sizeof (dummysigs[0]); i++)
        sigaddset (&set, dummysigs[i]);

    /* Block all these signals */
    pthread_sigmask (SIG_BLOCK, &set, NULL);

    for (unsigned i = 0; i < sizeof (dummysigs) / sizeof (dummysigs[0]); i++)
        sigdelset (&set, dummysigs[i]);

    pthread_create (&sigth, NULL, SigHandler, &set);
#endif

#ifdef WIN32
    /* Replace argv[1..n] with unicode for Windows NT and above */
    if( GetVersion() < 0x80000000 )
    {
        wchar_t **wargv, **wenvp;
        int i,i_wargc;
        int si = { 0 };
        __wgetmainargs(&i_wargc, &wargv, &wenvp, 0, &si);

        for( i = 0; i < i_wargc; i++ )
        {
            int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
            if( len > 0 )
            {
                if( len > 1 ) {
                    char *utf8arg = (char *)malloc(len);
                    if( NULL != utf8arg )
                    {
                        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, utf8arg, len, NULL, NULL);
                        ppsz_argv[i] = utf8arg;
                    }
                    else
                    {
                        /* failed!, quit */
                        return 1;
                    }
                }
                else
                {
                    ppsz_argv[i] = strdup ("");
                }
            }
            else
            {
                /* failed!, quit */
                return 1;
            }
        }
    }
    else
#endif
    {
        /* Note that FromLocale() can be used before libvlc is initialized */
        for (int i = 0; i < i_argc; i++)
            if ((ppsz_argv[i] = FromLocale (ppsz_argv[i])) == NULL)
                return 1; // BOOM!
    }

    libvlc_exception_t ex;
    libvlc_exception_init (&ex);

    /* Initialize libvlc */
    int i_argc_real = i_argc ? i_argc - 1 : 0;
    const char **ppsz_argv_real = i_argc ? &ppsz_argv[1] : ppsz_argv;
    libvlc_instance_t *vlc = libvlc_new (i_argc_real, ppsz_argv_real, &ex);

    if (vlc != NULL)
    {
        libvlc_run_interface (vlc, NULL, &ex);
        libvlc_release (vlc);
    }
    i_ret = libvlc_exception_raised (&ex);
    libvlc_exception_clear (&ex);

    for (int i = 0; i < i_argc; i++)
        LocaleFree (ppsz_argv[i]);

#if !defined(WIN32) && !defined(UNDER_CE)
    pthread_cancel (sigth);
# ifdef __APPLE__
    /* In Mac OS X up to 10.4.8 sigwait (among others) is not a pthread
     * cancellation point, so we throw a dummy quit signal to end
     * sigwait() in the sigth thread */
    pthread_kill (sigth, SIGQUIT);
# endif
    pthread_join (sigth, NULL);
#endif

    return i_ret;
}

#if !defined(WIN32) && !defined(UNDER_CE)
/*****************************************************************************
 * SigHandler: system signal handler
 *****************************************************************************
 * This thread receives all handled signals synchronously.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void *SigHandler (void *data)
{
    const sigset_t *exitset = (sigset_t *)data;
    sigset_t fullset;
    time_t abort_time = 0;

    pthread_sigmask (SIG_BLOCK, exitset, &fullset);

    for (;;)
    {
        int i_signal, state;
        if( sigwait (&fullset, &i_signal) != 0 )
            continue;

#ifdef __APPLE__
        /* In Mac OS X up to 10.4.8 sigwait (among others) is not a pthread
         * cancellation point */
        pthread_testcancel();
#endif

        if (!sigismember (exitset, i_signal))
            continue; /* Ignore "dummy" signals */

        /* Once a signal has been trapped, the termination sequence will be
         * armed and subsequent signals will be ignored to avoid sending
         * signals to a libvlc structure having been destroyed */

        pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);
        if (abort_time == 0 || time (NULL) > abort_time)
        {
            time (&abort_time);
            abort_time += 2;

            fprintf (stderr, "signal %d received, terminating vlc - do it "
                            "again quickly in case it gets stuck\n", i_signal);
            //VLC_Die( 0 );
        }
        else /* time (NULL) <= abort_time */
        {
            /* If user asks again more than 2 seconds later, die badly */
            pthread_sigmask (SIG_UNBLOCK, exitset, NULL);
            fprintf (stderr, "user insisted too much, dying badly\n");
#ifdef __APPLE__
            /* On Mac OS X, use exit(-1) as it doesn't trigger
             * backtrace generation, whereas abort() does.
             * The backtrace generation trigger a Crash Dialog
             * And takes way too much time, which is not what
             * we want. */
            exit (-1);
#else
            abort ();
#endif
        }
        pthread_setcancelstate (state, NULL);
    }
    /* Never reached */
}
#endif

#if defined(UNDER_CE)
#   if defined( _MSC_VER ) && defined( UNDER_CE )
#       include "vlc_common.h"
#   endif
/*****************************************************************************
 * WinMain: parse command line, start interface and spawn threads. (WinCE only)
 *****************************************************************************/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPTSTR lpCmdLine, int nCmdShow )
{
    char **argv, psz_cmdline[MAX_PATH];
    int argc, i_ret;

    WideCharToMultiByte( CP_ACP, 0, lpCmdLine, -1,
                         psz_cmdline, MAX_PATH, NULL, NULL );

    argv = vlc_parse_cmdline( psz_cmdline, &argc );
    argv = realloc( argv, (argc + 1) * sizeof(char *) );
    if( !argv ) return -1;

    if( argc ) memmove( argv + 1, argv, argc * sizeof(char *) );
    argv[0] = ""; /* Fake program path */

    i_ret = main( argc + 1, argv );

    /* No need to free the argv memory */
    return i_ret;
}
#endif
