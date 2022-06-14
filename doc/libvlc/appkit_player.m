/* appkit_player.m: AppKit libvlc integration sample
 *
 * Copyright (C) 2022 Videolabs
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
    To compile the sample:

    clang doc/libvlc/appkit_player.m -fobjc-arc -o appkit_player -I include/ -Wl,-framework,AppKit -I include -L build-macosx/lib/.libs -lvlc
    install_name_tool -add_rpath build-macosx/lib/.libs appkit_player
    install_name_tool -add_rpath build-macosx/src/.libs appkit_player

    VLC_PLUGIN_PATH=$(pwd)/build-macosx/modules/ ./appkit_player $MRL
*/

#import <Cocoa/Cocoa.h>
#import <vlc/vlc.h>

#define ARRAY_SIZE(A) (sizeof (A) / sizeof (A[0]))

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    libvlc_instance_t *instance;
    libvlc_media_player_t *player;
    libvlc_media_t *media;
}
@property NSWindow *window;
@property NSView *view;
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
     // Insert code here to initialize your application

     NSWindowStyleMask windowMask =
         NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable |
         NSWindowStyleMaskResizable | NSWindowStyleMaskClosable;
     _window = [[NSWindow alloc]
         initWithContentRect: NSMakeRect(300, 300, 200, 100)
                   styleMask: windowMask
                     backing: NSBackingStoreBuffered
                       defer: NO];
    [_window setTitle: @"LibVLC AppKit sample"];
    [_window makeKeyAndOrderFront:nil];

    _view = [[NSView alloc] initWithFrame:NSMakeRect(300, 300, 200, 100)];
    [_window setContentView: _view];

    const char *const vlc_args[] = { "-vv", "--aout=dummy" };
    instance = libvlc_new(ARRAY_SIZE(vlc_args), vlc_args);
    NSAssert(instance != NULL, @"Failed to allocate libvlc instance");

    player = libvlc_media_player_new(instance);
    NSAssert(player != NULL, @"Failed to allocate player instance");

    NSArray *args = [[NSProcessInfo processInfo] arguments];
    NSString *location = [args objectAtIndex:1];
    media = libvlc_media_new_location([location UTF8String]);
    libvlc_media_player_set_media(player, media);

    libvlc_media_player_set_nsobject(player, (__bridge void*)_view);
    libvlc_media_player_play(player);
}
@end

int main(int argc, char *argv[])
{
    if (argc < 2) return -1;
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp setDelegate: delegate];
    [NSApp activateIgnoringOtherApps: YES];
    return NSApplicationMain(argc, (const char **)argv);
}
