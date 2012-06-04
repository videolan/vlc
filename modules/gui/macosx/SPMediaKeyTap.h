/*
 Copyright (c) 2011, Joachim Bengtsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Neither the name of the organization nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Cocoa/Cocoa.h>
#import <IOKit/hidsystem/ev_keymap.h>
#import <Carbon/Carbon.h>

// http://overooped.com/post/2593597587/mediakeys

#define SPSystemDefinedEventMediaKeys 8

@interface SPMediaKeyTap : NSObject {
    EventHandlerRef _app_switching_ref;
    EventHandlerRef _app_terminating_ref;
    CFMachPortRef _eventPort;
    CFRunLoopSourceRef _eventPortSource;
    CFRunLoopRef _tapThreadRL;
    BOOL _shouldInterceptMediaKeyEvents;
    id _delegate;
    // The app that is frontmost in this list owns media keys
    NSMutableArray *_mediaKeyAppList;
}
+ (NSArray*)defaultMediaKeyUserBundleIdentifiers;

-(id)initWithDelegate:(id)delegate;

+(BOOL)usesGlobalMediaKeyTap;
-(void)startWatchingMediaKeys;
-(void)stopWatchingMediaKeys;
-(void)handleAndReleaseMediaKeyEvent:(NSEvent *)event;
@end

@interface NSObject (SPMediaKeyTapDelegate)
-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;
@end

#ifdef __cplusplus
extern "C" {
#endif

extern NSString *kMediaKeyUsingBundleIdentifiersDefaultsKey;
extern NSString *kIgnoreMediaKeysDefaultsKey;

#ifdef __cplusplus
}
#endif
