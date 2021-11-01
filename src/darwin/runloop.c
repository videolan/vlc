/*****************************************************************************
 * runloop.c: CFRunLoop handling code for core and modules
 *****************************************************************************
 * Copyright (C) 2021 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#include <vlc_common.h>
#include <vlc_threads.h>

#include <CoreFoundation/CoreFoundation.h>
#include "runloop.h"

static CFStringRef runloop_mode = CFSTR("org.videolan.vlccore.window");

static void SourcePerform(void *info) { (void)info; }
static CFRunLoopSourceContext source_context =
    { .perform = SourcePerform };

void vlc_darwin_runloop_PerformBlock(CFRunLoopRef runloop, void (^block)())
{
    CFRunLoopPerformBlock(runloop, runloop_mode, ^{
        (block)();
    });
    CFRunLoopWakeUp(runloop);
}

void vlc_darwin_DispatchSync(CFRunLoopRef runloop, void (^block_func)())
{
    __block vlc_sem_t performed;
    vlc_sem_init(&performed, 0);

    CFStringRef modes_cfstrings[] = {
        kCFRunLoopDefaultMode,
        runloop_mode,
    };

    CFArrayRef modes = CFArrayCreate(NULL, (const void **)modes_cfstrings,
            ARRAY_SIZE(modes_cfstrings),
            &kCFTypeArrayCallBacks);

    /* NOTE: we're using CFRunLoopPerformBlock with a custom mode tag
     * to avoid deadlocks between the window module (main thread) and the
     * display module, which would happen when using dispatch_sync here. */
    CFRunLoopPerformBlock(runloop, modes, ^{
        (block_func)();
        vlc_sem_post(&performed);
    });
    CFRunLoopWakeUp(runloop);

    vlc_sem_wait(&performed);
    CFRelease(modes);
}

void vlc_darwin_runloop_Stop(CFRunLoopRef runloop)
{
    /* Callback hell right below, we need to execute the call
     * to CFRunLoopStop inside the CFRunLoopRunInMode context
     * since the CFRunLoopRunInMode might have already returned
     * otherwise, which means more callback wrapping. */
    CFRunLoopPerformBlock(runloop, runloop_mode, ^{
        /* Signal that we can end the ReportEvent call */
        CFRunLoopStop(runloop);
    });
    CFRunLoopWakeUp(runloop);
}

void vlc_darwin_runloop_RunUntilStopped(CFRunLoopRef runloop)
{
    /* We need to inject a source within the CFRunLoop to avoi the call to
     * CFRunLoopRunInMode to think the runloop is empty and return the value
     * kCFRunLoopRunFinished instead of waiting for an event. */
    CFRunLoopSourceRef source =
        CFRunLoopSourceCreate(NULL, 0, &source_context);

    CFRunLoopAddSource(runloop, source, runloop_mode);
    for (;;)
    {
        /* We need a timeout here, otherwise the CFRunLoopInMode
         * call will check the events (if woken up), and since
         * we might have no event, it would return a timeout
         * result code, and loop again, creating a busy loop.
         * INFINITY is more than enough, and we'll interrupt
         * anyway. */
        CFRunLoopRunResult ret = CFRunLoopRunInMode(runloop_mode, INFINITY, true);

        /* Usual CFRunLoop are typically checking result code
         * like kCFRunLoopRunFinished too, but we really want
         * to receive the Stop signal from above to leave the
         * loop in the correct state. */
        if (ret == kCFRunLoopRunStopped)
            break;
    }
    CFRunLoopRemoveSource(runloop, source, runloop_mode);
    CFRelease(source);
}
