/*****************************************************************************
 * darwinvlc.m: OS X specific main executable for VLC media player
 *****************************************************************************
 * Copyright (C) 2013-2015 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <dfuhrmann at videolan dot org>
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
#include <vlc_common.h>
#include <vlc_charset.h>

#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <string.h>

#import <CoreFoundation/CoreFoundation.h>
#import <Cocoa/Cocoa.h>

#ifdef HAVE_BREAKPAD
#import <Breakpad/Breakpad.h>
#endif


/**
 * Handler called when VLC asks to terminate the program.
 */
static void vlc_terminate(void *data)
{
    (void)data;

    dispatch_async(dispatch_get_main_queue(), ^{
        /*
         * Stop the main loop. When using the CoreFoundation mainloop, simply
         * CFRunLoopStop can be used.
         *
         * But this does not work when having an interface.
         * In this case, [NSApp stop:nil] needs to be used, but the used flag is only
         * evaluated at the end of main loop event processing. This is always true
         * in the case of code inside a action method. But here, this is
         * not true and thus we need to send an dummy event to make sure the stop
         * flag is actually processed by the main loop.
         */
        if (NSApp == nil) {
            CFRunLoopStop(CFRunLoopGetCurrent());
        } else {

            [NSApp stop:nil];
            NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                                location:NSMakePoint(0,0)
                                           modifierFlags:0
                                               timestamp:0.0
                                            windowNumber:0
                                                 context:nil
                                                 subtype:0
                                                   data1:0
                                                   data2:0];
            [NSApp postEvent:event atStart:YES];
        }

    });
}

#ifdef HAVE_BREAKPAD
BreakpadRef initBreakpad()
{
    BreakpadRef bp = nil;

    /* Create caches directory in case it does not exist */
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *cachePath = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject];
    NSString *bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleIdentifier"];
    NSString *cacheAppPath = [cachePath stringByAppendingPathComponent:bundleName];
    if (![fileManager fileExistsAtPath:cacheAppPath]) {
        [fileManager createDirectoryAtPath:cacheAppPath withIntermediateDirectories:NO attributes:nil error:nil];
    }

    /* Get Info.plist config */
    NSMutableDictionary *breakpad_config = [[[NSBundle mainBundle] infoDictionary] mutableCopy];

    /* Use in-process reporting */
    [breakpad_config setObject:[NSNumber numberWithBool:YES]
                        forKey:@BREAKPAD_IN_PROCESS];

    /* Set dump location */
    [breakpad_config setObject:cacheAppPath
                        forKey:@BREAKPAD_DUMP_DIRECTORY];

    bp = BreakpadCreate(breakpad_config);
    return bp;
}
#endif

/*****************************************************************************
 * main: parse command line, start interface and spawn threads.
 *****************************************************************************/
int main(int i_argc, const char *ppsz_argv[])
{
#ifdef HAVE_BREAKPAD
    BreakpadRef breakpad = NULL;

    if (!getenv("VLC_DISABLE_BREAKPAD"))
        breakpad = initBreakpad();
#endif

    /* The so-called POSIX-compliant MacOS X reportedly processes SIGPIPE even
     * if it is blocked in all thread.
     * Note: this is NOT an excuse for not protecting against SIGPIPE. If
     * LibVLC runs outside of VLC, we cannot rely on this code snippet. */
    signal(SIGPIPE, SIG_IGN);
    /* Restore SIGCHLD in case our parent process ignores it. */
    signal(SIGCHLD, SIG_DFL);

#ifndef NDEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    setenv("MALLOC_CHECK_", "2", 1);
#endif

#ifdef TOP_BUILDDIR
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
    setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 1);
#endif

#ifndef ALLOW_RUN_AS_ROOT
    if (geteuid() == 0)
    {
        fprintf(stderr, "VLC is not supposed to be run as root. Sorry.\n"
        "If you need to use real-time priorities and/or privileged TCP ports\n"
        "you can use %s-wrapper (make sure it is Set-UID root and\n"
        "cannot be run by non-trusted users first).\n", ppsz_argv[0]);
        return 1;
    }
#endif

    setlocale(LC_ALL, "");

    if (isatty(STDERR_FILENO))
        /* This message clutters error logs. It is printed only on a TTY.
         * Fortunately, LibVLC prints version info with -vv anyway. */
        fprintf(stderr, "VLC media player %s (revision %s)\n",
                 libvlc_get_version(), libvlc_get_changeset());

    sigset_t set;

    sigemptyset(&set);
    /*
     * The darwin version of VLC used GCD to dequeue interesting signals.
     * For this to work, those signals must be blocked.
     *
     * There are two advantages over traditional signal handlers:
     *  - handling is done on a separate thread: no need to worry about async-safety,
     *  - EINTR is not generated: other threads need not handle that error.
     * That being said, some LibVLC programs do not use sigwait(). Therefore
     * EINTR must still be handled cleanly, notably from poll() calls.
     *
     * Signals that request a clean shutdown.
     * We have to handle SIGTERM cleanly because of daemon mode. */
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    /* SIGPIPE can happen and would crash the process. On modern systems,
     * the MSG_NOSIGNAL flag protects socket write operations against SIGPIPE.
     * But we still need to block SIGPIPE when:
     *  - writing to pipes,
     *  - using write() instead of send() for code not specific to sockets.
     * LibVLC code assumes that SIGPIPE is blocked. Other LibVLC applications
     * shall block it (or handle it somehow) too.
     */
    sigaddset(&set, SIGPIPE);

    /* SIGCHLD must be dequeued to clean up zombie child processes.
     * Furthermore the handler must not be set to SIG_IGN (see above).
     * We cannot pragmatically handle EINTR, short reads and short writes
     * in every code paths (including underlying libraries). So we just
     * block SIGCHLD in all threads, and dequeue it below. */
    sigaddset(&set, SIGCHLD);

    /* Block all these signals */
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    /* Handle signals with GCD */
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_source_t sigIntSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, queue);
    dispatch_source_t sigTermSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, queue);
    dispatch_source_t sigChldSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGCHLD, 0, queue);

    if (!sigIntSource || !sigTermSource || !sigChldSource)
        abort();

    dispatch_source_set_event_handler(sigIntSource, ^{
        vlc_terminate(nil);
    });
    dispatch_source_set_event_handler(sigTermSource, ^{
        vlc_terminate(nil);
    });

    dispatch_source_set_event_handler(sigChldSource, ^{
        int status;
        while(waitpid(-1, &status, WNOHANG) > 0)
            ;
    });

    dispatch_resume(sigIntSource);
    dispatch_resume(sigTermSource);
    dispatch_resume(sigChldSource);


    /* Handle parameters */
    const char *argv[i_argc + 2];
    int argc = 0;

    argv[argc++] = "--no-ignore-config";
    argv[argc++] = "--media-library";

    /* Overwrite system language */
    CFPropertyListRef lang_pref = CFPreferencesCopyAppValue(CFSTR("language"),
        kCFPreferencesCurrentApplication);

    if (lang_pref) {
        if (CFGetTypeID(lang_pref) == CFStringGetTypeID()) {
            char *lang = FromCFString(lang_pref, kCFStringEncodingUTF8);
            if (strncmp(lang, "auto", 4)) {
                char tmp[11];
                snprintf(tmp, 11, "LANG=%s", lang);
                putenv(tmp);
            }
            free(lang);
        }
        CFRelease(lang_pref);
    }

    ppsz_argv++; i_argc--; /* skip executable path */

    /* When VLC.app is run by double clicking in Mac OS X < 10.9, the 2nd arg
     * is the PSN - process serial number (a unique PID-ish thingie)
     * still ok for real Darwin & when run from command line
     * for example -psn_0_9306113 */
    if (i_argc >= 1 && !strncmp(*ppsz_argv, "-psn" , 4))
        ppsz_argv++, i_argc--;

    memcpy (argv + argc, ppsz_argv, i_argc * sizeof(*argv));
    argc += i_argc;
    argv[argc] = NULL;

    /* Initialize libvlc */
    libvlc_instance_t *vlc = libvlc_new(argc, argv);
    if (vlc == NULL)
        return 1;

    int ret = 1;
    libvlc_set_exit_handler(vlc, vlc_terminate, NULL);
    libvlc_set_app_id(vlc, "org.VideoLAN.VLC", PACKAGE_VERSION, PACKAGE_NAME);
    libvlc_set_user_agent(vlc, "VLC media player", "VLC/"PACKAGE_VERSION);

    libvlc_add_intf(vlc, "hotkeys,none");

    if (libvlc_add_intf(vlc, NULL)) {
        fprintf(stderr, "VLC cannot start any interface. Exiting.\n");
        goto out;
    }
    libvlc_playlist_play(vlc);

    /*
     * Run the main loop. If the mac interface is not initialized, only the CoreFoundation
     * runloop is used. Otherwise, [NSApp run] needs to be called, which setups more stuff
     * before actually starting the loop.
     */
    @autoreleasepool {
        if(NSApp == nil) {
            CFRunLoopRun();

        } else {
            [NSApp run];
        }
    }

    ret = 0;
    /* Cleanup */
out:
    dispatch_release(sigIntSource);
    dispatch_release(sigTermSource);
    dispatch_release(sigChldSource);

    libvlc_release(vlc);

#ifdef HAVE_BREAKPAD
    if (breakpad)
        BreakpadRelease(breakpad);
#endif

    return ret;
}
