#ifndef VLC_DARWIN_RUNLOOP_H
#define VLC_DARWIN_RUNLOOP_H

#include <CoreFoundation/CFRunLoop.h>

void vlc_darwin_runloop_PerformBlock(CFRunLoopRef runloop, void (^block)());
void vlc_darwin_runloop_Stop(CFRunLoopRef runloop);
void vlc_darwin_runloop_RunUntilStopped(CFRunLoopRef runloop);
void vlc_darwin_DispatchSync(CFRunLoopRef runloop, void (^block_func)());

#endif
