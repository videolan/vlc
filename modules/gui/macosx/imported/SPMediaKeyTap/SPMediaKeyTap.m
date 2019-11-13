/*
 Copyright (c) 2011, Joachim Bengtsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 * Neither the name of the organization nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Copyright (c) 2010 Spotify AB
#import "SPMediaKeyTap.h"

// Define to enable app list debug output
// #define DEBUG_SPMEDIAKEY_APPLIST 1

NSString *kIgnoreMediaKeysDefaultsKey = @"SPIgnoreMediaKeys";

@interface SPMediaKeyTap () {
    CFMachPortRef _eventPort;
    CFRunLoopSourceRef _eventPortSource;
    CFRunLoopRef _tapThreadRL;
    NSThread *_tapThread;
    BOOL _shouldInterceptMediaKeyEvents;
    id _delegate;
    // The app that is frontmost in this list owns media keys
    NSMutableArray<NSRunningApplication *> *_mediaKeyAppList;
}

- (BOOL)shouldInterceptMediaKeyEvents;
- (void)setShouldInterceptMediaKeyEvents:(BOOL)newSetting;
- (void)startWatchingAppSwitching;
- (void)stopWatchingAppSwitching;
- (void)eventTapThread;
@end

static CGEventRef tapEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);


// Inspired by http://gist.github.com/546311

@implementation SPMediaKeyTap

#pragma mark -
#pragma mark Setup and teardown

- (id)initWithDelegate:(id)delegate
{
    self = [super init];
    if (self) {
        _delegate = delegate;
        [self startWatchingAppSwitching];
        _mediaKeyAppList = [NSMutableArray new];
    }
    return self;
}

- (void)dealloc
{
    [self stopWatchingMediaKeys];
    [self stopWatchingAppSwitching];
}

- (void)startWatchingAppSwitching
{
    // Listen to "app switched" event, so that we don't intercept media keys if we
    // weren't the last "media key listening" app to be active

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                           selector:@selector(frontmostAppChanged:)
                                                               name:NSWorkspaceDidActivateApplicationNotification
                                                             object:nil];


    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                           selector:@selector(appTerminated:)
                                                               name:NSWorkspaceDidTerminateApplicationNotification
                                                             object:nil];
}

- (void)stopWatchingAppSwitching
{
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

- (BOOL)startWatchingMediaKeys
{
    // Prevent having multiple mediaKeys threads
    [self stopWatchingMediaKeys];

    [self setShouldInterceptMediaKeyEvents:YES];

    // Add an event tap to intercept the system defined media key events
    _eventPort = CGEventTapCreate(kCGSessionEventTap,
                                  kCGHeadInsertEventTap,
                                  kCGEventTapOptionDefault,
                                  CGEventMaskBit(NX_SYSDEFINED),
                                  tapEventCallback,
                                  (__bridge void * __nullable)(self));

    // Can be NULL if the app has no accessibility access permission
    if (_eventPort == NULL)
        return NO;

    _eventPortSource = CFMachPortCreateRunLoopSource(kCFAllocatorSystemDefault, _eventPort, 0);
    assert(_eventPortSource != NULL);

    if (_eventPortSource == NULL)
        return NO;

    // Let's do this in a separate thread so that a slow app doesn't lag the event tap
    _tapThread = [[NSThread alloc] initWithTarget:self
                                         selector:@selector(eventTapThread)
                                           object:nil];
    [_tapThread start];

    return YES;
}

- (void)stopWatchingMediaKeys
{
    // Shut down tap thread
    if(_tapThreadRL){
        CFRunLoopStop(_tapThreadRL);
        _tapThreadRL = nil;
    }

    // Remove tap port
    if(_eventPort){
        CFMachPortInvalidate(_eventPort);
        CFRelease(_eventPort);
        _eventPort = nil;
    }

    // Remove tap source
    if(_eventPortSource){
        CFRelease(_eventPortSource);
        _eventPortSource = nil;
    }
}

#pragma mark -
#pragma mark Accessors

+ (BOOL)usesGlobalMediaKeyTap
{
#ifdef _DEBUG
    // breaking in gdb with a key tap inserted sometimes locks up all mouse and keyboard input forever, forcing reboot
    return NO;
#else
    // XXX(nevyn): MediaKey event tap doesn't work on 10.4, feel free to figure out why if you have the energy.
    return
        ![[NSUserDefaults standardUserDefaults] boolForKey:kIgnoreMediaKeysDefaultsKey]
        && floor(NSAppKitVersionNumber) >= 949/*NSAppKitVersionNumber10_5*/;
#endif
}

+ (NSArray*)mediaKeyUserBundleIdentifiers
{
    static NSArray *bundleIdentifiers;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        bundleIdentifiers = @[
            [[NSBundle mainBundle] bundleIdentifier], // your app
            @"com.spotify.client",
            @"com.apple.iTunes",
            @"com.apple.Music",
            @"com.apple.QuickTimePlayerX",
            @"com.apple.quicktimeplayer",
            @"com.apple.iWork.Keynote",
            @"com.apple.iPhoto",
            @"org.videolan.vlc",
            @"com.apple.Aperture",
            @"com.plexsquared.Plex",
            @"com.soundcloud.desktop",
            @"org.niltsh.MPlayerX",
            @"com.ilabs.PandorasHelper",
            @"com.mahasoftware.pandabar",
            @"com.bitcartel.pandorajam",
            @"org.clementine-player.clementine",
            @"fm.last.Last.fm",
            @"fm.last.Scrobbler",
            @"com.beatport.BeatportPro",
            @"com.Timenut.SongKey",
            @"com.macromedia.fireworks", // the tap messes up their mouse input
            @"at.justp.Theremin",
            @"ru.ya.themblsha.YandexMusic",
            @"com.jriver.MediaCenter18",
            @"com.jriver.MediaCenter19",
            @"com.jriver.MediaCenter20",
            @"co.rackit.mate",
            @"com.ttitt.b-music",
            @"com.beardedspice.BeardedSpice",
            @"com.plug.Plug",
            @"com.netease.163music",
        ];
    });

    return bundleIdentifiers;
}


- (BOOL)shouldInterceptMediaKeyEvents
{
    BOOL shouldIntercept = NO;
    @synchronized(self) {
        shouldIntercept = _shouldInterceptMediaKeyEvents;
    }
    return shouldIntercept;
}

- (void)pauseTapOnTapThread:(NSNumber *)yeahno
{
    CGEventTapEnable(self->_eventPort, [yeahno boolValue]);
}

- (void)setShouldInterceptMediaKeyEvents:(BOOL)newSetting
{
    BOOL oldSetting;
    @synchronized(self) {
        oldSetting = _shouldInterceptMediaKeyEvents;
        _shouldInterceptMediaKeyEvents = newSetting;
    }
    if(_tapThreadRL && oldSetting != newSetting) {
        [self performSelector:@selector(pauseTapOnTapThread:)
                     onThread:_tapThread
                   withObject:@(newSetting)
                waitUntilDone:NO];

    }
}


#pragma mark -
#pragma mark Event tap callbacks

// Note: method called on background thread

static CGEventRef tapEventCallback2(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
    SPMediaKeyTap *self = (__bridge SPMediaKeyTap *)refcon;

    if(type == kCGEventTapDisabledByTimeout) {
        NSLog(@"Media key event tap was disabled by timeout");
        CGEventTapEnable(self->_eventPort, TRUE);
        return event;
    } else if(type == kCGEventTapDisabledByUserInput) {
        // Was disabled manually by -[pauseTapOnTapThread]
        return event;
    }
    NSEvent *nsEvent = nil;
    @try {
        nsEvent = [NSEvent eventWithCGEvent:event];
    }
    @catch (NSException * e) {
        NSLog(@"Strange CGEventType: %d: %@", type, e);
        assert(0);
        return event;
    }

    if (type != NX_SYSDEFINED || [nsEvent subtype] != SPSystemDefinedEventMediaKeys)
        return event;

    int keyCode = (([nsEvent data1] & 0xFFFF0000) >> 16);
    if (keyCode != NX_KEYTYPE_PLAY && keyCode != NX_KEYTYPE_FAST && keyCode != NX_KEYTYPE_REWIND && keyCode != NX_KEYTYPE_PREVIOUS && keyCode != NX_KEYTYPE_NEXT)
        return event;

    if (![self shouldInterceptMediaKeyEvents])
        return event;

    [self performSelectorOnMainThread:@selector(handleAndReleaseMediaKeyEvent:) withObject:nsEvent waitUntilDone:NO];

    return NULL;
}

static CGEventRef tapEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
    @autoreleasepool {
        CGEventRef ret = tapEventCallback2(proxy, type, event, refcon);
        return ret;
    }
}

- (void)handleAndReleaseMediaKeyEvent:(NSEvent *)event
{
    [_delegate mediaKeyTap:self receivedMediaKeyEvent:event];
}

- (void)eventTapThread
{
    _tapThreadRL = CFRunLoopGetCurrent();
    CFRunLoopAddSource(_tapThreadRL, _eventPortSource, kCFRunLoopCommonModes);
    CFRunLoopRun();
}


#pragma mark -
#pragma mark Task switching callbacks

- (void)mediaKeyAppListChanged
{
    #ifdef DEBUG_SPMEDIAKEY_APPLIST
    [self debugPrintAppList];
    #endif

    if([_mediaKeyAppList count] == 0)
        return;

    NSRunningApplication *thisApp = [NSRunningApplication currentApplication];
    NSRunningApplication *otherApp = [_mediaKeyAppList firstObject];

    BOOL isCurrent = [thisApp isEqual:otherApp];

    [self setShouldInterceptMediaKeyEvents:isCurrent];
}

- (void)frontmostAppChanged:(NSNotification *)notification
{
    NSRunningApplication *app = [notification.userInfo objectForKey:NSWorkspaceApplicationKey]; 
    if (app.bundleIdentifier == nil)
        return;

    if (![[SPMediaKeyTap mediaKeyUserBundleIdentifiers] containsObject:app.bundleIdentifier])
        return;

    [_mediaKeyAppList removeObject:app];
    [_mediaKeyAppList insertObject:app atIndex:0];
    [self mediaKeyAppListChanged];
}

- (void)appTerminated:(NSNotification *)notification
{
    NSRunningApplication *app = [notification.userInfo objectForKey:NSWorkspaceApplicationKey];
    [_mediaKeyAppList removeObject:app];

    [self mediaKeyAppListChanged];
}

#ifdef DEBUG_SPMEDIAKEY_APPLIST
- (void)debugPrintAppList
{
    NSMutableString *list = [NSMutableString stringWithCapacity:255];
    for (NSRunningApplication *app in _mediaKeyAppList) {
        [list appendFormat:@"     - %@\n", app.bundleIdentifier];
    }
    NSLog(@"List: \n%@", list);
}
#endif

@end
