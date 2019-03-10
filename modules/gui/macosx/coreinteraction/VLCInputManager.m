/*****************************************************************************
 * VLCInputManager.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2015-2018 VLC authors and VideoLAN
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

#import "VLCInputManager.h"

#include <vlc_url.h>

#import "coreinteraction/VLCCoreInteraction.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"
#import "os-integration/VLCRemoteControlService.h"
#import "os-integration/iTunes.h"
#import "os-integration/Spotify.h"
#import "panels/VLCTrackSynchronizationWindowController.h"
#import "panels/dialogs/VLCResumeDialogController.h"
#import "windows/extensions/VLCExtensionsManager.h"
#import "windows/mainwindow/VLCMainWindow.h"
#import "windows/video/VLCVoutView.h"

@interface VLCInputManager()
- (void)updateMainMenu;
- (void)updateMainWindow;
- (void)updateDelays;
@end

#pragma mark Callbacks

static int InputThreadChanged(vlc_object_t *p_this, const char *psz_var,
                              vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        VLCInputManager *inputManager = (__bridge VLCInputManager *)param;
        [inputManager performSelectorOnMainThread:@selector(inputThreadChanged) withObject:nil waitUntilDone:NO];
    }

    return VLC_SUCCESS;
}

static NSDate *lastPositionUpdate = nil;

static int InputEvent(vlc_object_t *p_this, const char *psz_var,
                      vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        VLCInputManager *inputManager = (__bridge VLCInputManager *)param;

        switch (new_val.i_int) {
            case INPUT_EVENT_STATE:
                [inputManager performSelectorOnMainThread:@selector(playbackStatusUpdated) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_RATE:
                break;
            case INPUT_EVENT_POSITION:
                break;
            case INPUT_EVENT_TITLE:
            case INPUT_EVENT_CHAPTER:
                [inputManager performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_CACHE:
                [inputManager performSelectorOnMainThread:@selector(updateMainWindow) withObject:nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_STATISTICS:
                break;
            case INPUT_EVENT_ES:
                break;
            case INPUT_EVENT_VOUT:
                break;
            case INPUT_EVENT_ITEM_META:
            case INPUT_EVENT_ITEM_INFO:
                [inputManager performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
                [inputManager performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_BOOKMARK:
                break;
            case INPUT_EVENT_RECORD:
                break;
            case INPUT_EVENT_PROGRAM:
                [inputManager performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_ITEM_EPG:
                break;
            case INPUT_EVENT_SIGNAL:
                break;

            case INPUT_EVENT_AUDIO_DELAY:
            case INPUT_EVENT_SUBTITLE_DELAY:
                [inputManager performSelectorOnMainThread:@selector(updateDelays) withObject:nil waitUntilDone:NO];
                break;

            case INPUT_EVENT_DEAD:
                [inputManager performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
                break;

            default:
                break;
        }

        return VLC_SUCCESS;
    }
}

#pragma mark -
#pragma mark InputManager implementation

@interface VLCInputManager()
{
    __weak VLCMain *o_main;

    input_thread_t *p_current_input;
    dispatch_queue_t informInputChangedQueue;

    /* iTunes/Spotify play/pause support */
    BOOL b_has_itunes_paused;
    BOOL b_has_spotify_paused;

    /* remote control support */
    VLCRemoteControlService *_remoteControlService;

    NSTimer *hasEndedTimer;
}
@end

@implementation VLCInputManager

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray array], @"recentlyPlayedMediaList",
                                 [NSDictionary dictionary], @"recentlyPlayedMedia", nil];

    [defaults registerDefaults:appDefaults];
}

- (id)initWithMain:(VLCMain *)o_mainObj
{
    self = [super init];
    if(self) {
        msg_Dbg(getIntf(), "Initializing input manager");

        o_main = o_mainObj;
        var_AddCallback(pl_Get(getIntf()), "input-current", InputThreadChanged, (__bridge void *)self);

        informInputChangedQueue = dispatch_queue_create("org.videolan.vlc.inputChangedQueue", DISPATCH_QUEUE_SERIAL);

        if (@available(macOS 10.12.2, *)) {
            _remoteControlService = [[VLCRemoteControlService alloc] init];
            [_remoteControlService subscribeToRemoteCommands];
        }
    }
    return self;
}

/*
 * TODO: Investigate if this can be moved to dealloc again. Current problems:
 * - dealloc might be never called of this object, as strong references could be in the
 *   (already stopped) main loop, preventing the refcount to go 0.
 * - Calling var_DelCallback waits for all callbacks to finish. Thus, while dealloc is already
 *   called, callback might grab a reference to this object again, which could cause trouble.
 */
- (void)deinit
{
    msg_Dbg(getIntf(), "Deinitializing input manager");
    if (@available(macOS 10.12.2, *)) {
        [_remoteControlService unsubscribeFromRemoteCommands];
    }

    if (p_current_input) {
        /* continue playback where you left off */
        [self storePlaybackPositionForItem:p_current_input];

        var_DelCallback(p_current_input, "intf-event", InputEvent, (__bridge void *)self);
        input_Release(p_current_input);
        p_current_input = NULL;
    }

    var_DelCallback(pl_Get(getIntf()), "input-current", InputThreadChanged, (__bridge void *)self);

#if !OS_OBJECT_USE_OBJC
    dispatch_release(informInputChangedQueue);
#endif
}

- (void)inputThreadChanged
{
    if (p_current_input) {
        var_DelCallback(p_current_input, "intf-event", InputEvent, (__bridge void *)self);
        input_Release(p_current_input);
        p_current_input = NULL;

        [[o_main mainMenu] setRateControlsEnabled: NO];

        [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputChangedNotification
                                                            object:nil];
    }

    // Cancel pending resume dialogs
    [[[VLCMain sharedInstance] resumeDialog] cancel];

    input_thread_t *p_input_changed = NULL;

    // object is hold here and released then it is dead
    p_current_input = playlist_CurrentInput(pl_Get(getIntf()));
    if (p_current_input) {
        var_AddCallback(p_current_input, "intf-event", InputEvent, (__bridge void *)self);
        [self playbackStatusUpdated];
        [[o_main mainMenu] setRateControlsEnabled: YES];

        if ([o_main activeVideoPlayback] && [[[o_main mainWindow] videoView] isHidden]) {
            [[o_main mainWindow] changePlaylistState: psPlaylistItemChangedEvent];
        }

        p_input_changed = input_Hold(p_current_input);

//        [[o_main playlist] currentlyPlayingItemChanged];

        [self continuePlaybackWhereYouLeftOff:p_current_input];

        [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputChangedNotification
                                                            object:nil];
    }

    [self updateMainWindow];
    [self updateDelays];
    [self updateMainMenu];

    /*
     * Due to constraints within NSAttributedString's main loop runtime handling
     * and other issues, we need to inform the extension manager on a separate thread.
     * The serial queue ensures that changed inputs are propagated in the same order as they arrive.
     */
    dispatch_async(informInputChangedQueue, ^{
        [[self->o_main extensionsManager] inputChanged:p_input_changed];
        if (p_input_changed)
            input_Release(p_input_changed);
    });
}

- (void)playbackStatusUpdated
{
    // On shutdown, input might not be dead yet. Cleanup actions like itunes playback
    // and playback positon are done in different code paths (dealloc and appWillTerminate:).
    if ([[VLCMain sharedInstance] isTerminating]) {
        return;
    }

    int64_t state = -1;
    if (p_current_input) {
        state = var_GetInteger(p_current_input, "state");
    }

    // cancel itunes timer if next item starts playing
    if (state > -1 && state != END_S) {
        if (hasEndedTimer) {
            [hasEndedTimer invalidate];
            hasEndedTimer = nil;
        }
    }

    if (state == PLAYING_S) {
        [self stopItunesPlayback];
        [[o_main mainWindow] setPause];
    } else {
        [[o_main mainMenu] setSubmenusEnabled: FALSE];
        [[o_main mainWindow] setPlay];

        if (state == END_S || state == -1) {
            /* continue playback where you left off */
            if (p_current_input)
                [self storePlaybackPositionForItem:p_current_input];

            if (hasEndedTimer) {
                [hasEndedTimer invalidate];
            }
            hasEndedTimer = [NSTimer scheduledTimerWithTimeInterval: 0.5
                                                             target: self
                                                           selector: @selector(onPlaybackHasEnded:)
                                                           userInfo: nil
                                                            repeats: NO];
        }
    }

    [self updateMainWindow];
}

// Called when playback has ended and likely no subsequent media will start playing
- (void)onPlaybackHasEnded:(id)sender
{
    msg_Dbg(getIntf(), "Playback has been ended");

    [self resumeItunesPlayback];
    hasEndedTimer = nil;
}

- (void)stopItunesPlayback
{
    intf_thread_t *p_intf = getIntf();
    int64_t controlItunes = var_InheritInteger(p_intf, "macosx-control-itunes");
    if (controlItunes <= 0)
        return;

    // pause iTunes
    if (!b_has_itunes_paused) {
        iTunesApplication *iTunesApp = (iTunesApplication *) [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
        if (iTunesApp && [iTunesApp isRunning]) {
            if ([iTunesApp playerState] == iTunesEPlSPlaying) {
                msg_Dbg(p_intf, "pausing iTunes");
                [iTunesApp pause];
                b_has_itunes_paused = YES;
            }
        }
    }

    // pause Spotify
    if (!b_has_spotify_paused) {
        SpotifyApplication *spotifyApp = (SpotifyApplication *) [SBApplication applicationWithBundleIdentifier:@"com.spotify.client"];

        if (spotifyApp) {
            if ([spotifyApp respondsToSelector:@selector(isRunning)] && [spotifyApp respondsToSelector:@selector(playerState)]) {
                if ([spotifyApp isRunning] && [spotifyApp playerState] == kSpotifyPlayerStatePlaying) {
                    msg_Dbg(p_intf, "pausing Spotify");
                    [spotifyApp pause];
                    b_has_spotify_paused = YES;
                }
            }
        }
    }
}

- (void)resumeItunesPlayback
{
    intf_thread_t *p_intf = getIntf();
    if (var_InheritInteger(p_intf, "macosx-control-itunes") > 1) {
        if (b_has_itunes_paused) {
            iTunesApplication *iTunesApp = (iTunesApplication *) [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
            if (iTunesApp && [iTunesApp isRunning]) {
                if ([iTunesApp playerState] == iTunesEPlSPaused) {
                    msg_Dbg(p_intf, "unpausing iTunes");
                    [iTunesApp playpause];
                }
            }
        }

        if (b_has_spotify_paused) {
            SpotifyApplication *spotifyApp = (SpotifyApplication *) [SBApplication applicationWithBundleIdentifier:@"com.spotify.client"];
            if (spotifyApp) {
                if ([spotifyApp respondsToSelector:@selector(isRunning)] && [spotifyApp respondsToSelector:@selector(playerState)]) {
                    if ([spotifyApp isRunning] && [spotifyApp playerState] == kSpotifyPlayerStatePaused) {
                        msg_Dbg(p_intf, "unpausing Spotify");
                        [spotifyApp play];
                    }
                }
            }
        }
    }

    b_has_itunes_paused = NO;
    b_has_spotify_paused = NO;
}

- (void)updateMainWindow
{
    [[o_main mainWindow] updateWindow];
}

- (void)updateName
{
    [[o_main mainWindow] updateName];
}

- (void)updateDelays
{
    [[[VLCMain sharedInstance] trackSyncPanel] updateValues];
}

- (void)updateMainMenu
{
    [[o_main mainMenu] setupMenus];
    [[VLCCoreInteraction sharedInstance] resetAtoB];
}

- (BOOL)hasInput
{
    return p_current_input != NULL;
}

#pragma mark -
#pragma mark Resume logic


- (BOOL)isValidResumeItem:(input_item_t *)p_item
{
    char *psz_url = input_item_GetURI(p_item);
    NSString *urlString = toNSStr(psz_url);
    free(psz_url);

    if ([urlString isEqualToString:@""])
        return NO;

    NSURL *url = [NSURL URLWithString:urlString];

    if (![url isFileURL])
        return NO;

    BOOL isDir = false;
    if (![[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&isDir])
        return NO;

    if (isDir)
        return NO;

    return YES;
}

- (void)continuePlaybackWhereYouLeftOff:(input_thread_t *)p_input_thread
{
    NSDictionary *recentlyPlayedFiles = [[NSUserDefaults standardUserDefaults] objectForKey:@"recentlyPlayedMedia"];
    if (!recentlyPlayedFiles)
        return;

    input_item_t *p_item = input_GetItem(p_input_thread);
    if (!p_item)
        return;

    /* allow the user to over-write the start/stop/run-time */
    if (var_GetFloat(p_input_thread, "run-time") > 0 ||
        var_GetFloat(p_input_thread, "start-time") > 0 ||
        var_GetFloat(p_input_thread, "stop-time") != 0) {
        return;
    }

    /* check for file existance before resuming */
    if (![self isValidResumeItem:p_item])
        return;

    char *psz_url = vlc_uri_decode(input_item_GetURI(p_item));
    if (!psz_url)
        return;
    NSString *url = toNSStr(psz_url);
    free(psz_url);

    NSNumber *lastPosition = [recentlyPlayedFiles objectForKey:url];
    if (!lastPosition || lastPosition.intValue <= 0)
        return;

    int settingValue = (int)config_GetInt("macosx-continue-playback");
    if (settingValue == 2) // never resume
        return;

    CompletionBlock completionBlock = ^(enum ResumeResult result) {

        if (result == RESUME_RESTART)
            return;

        vlc_tick_t lastPos = vlc_tick_from_sec( lastPosition.intValue );
        msg_Dbg(getIntf(), "continuing playback at %lld", lastPos);
        var_SetInteger(p_input_thread, "time", lastPos);
    };

    if (settingValue == 1) { // always
        completionBlock(RESUME_NOW);
        return;
    }

    [[[VLCMain sharedInstance] resumeDialog] showWindowWithItem:p_item
                                               withLastPosition:lastPosition.intValue
                                                completionBlock:completionBlock];

}

- (void)storePlaybackPositionForItem:(input_thread_t *)p_input_thread
{
    if (!var_InheritBool(getIntf(), "macosx-recentitems"))
        return;

    input_item_t *p_item = input_GetItem(p_input_thread);
    if (!p_item)
        return;

    if (![self isValidResumeItem:p_item])
        return;

    char *psz_url = vlc_uri_decode(input_item_GetURI(p_item));
    if (!psz_url)
        return;
    NSString *url = toNSStr(psz_url);
    free(psz_url);

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableDictionary *mutDict = [[NSMutableDictionary alloc] initWithDictionary:[defaults objectForKey:@"recentlyPlayedMedia"]];

    float relativePos = var_GetFloat(p_input_thread, "position");
    long long pos = SEC_FROM_VLC_TICK(var_GetInteger(p_input_thread, "time"));
    long long dur = SEC_FROM_VLC_TICK(input_item_GetDuration(p_item));

    NSMutableArray *mediaList = [[defaults objectForKey:@"recentlyPlayedMediaList"] mutableCopy];

    if (relativePos > .05 && relativePos < .95 && dur > 180) {
        msg_Dbg(getIntf(), "Store current playback position of %f", relativePos);
        [mutDict setObject:[NSNumber numberWithInteger:pos] forKey:url];

        [mediaList removeObject:url];
        [mediaList addObject:url];
        NSUInteger mediaListCount = mediaList.count;
        if (mediaListCount > 30) {
            for (NSUInteger x = 0; x < mediaListCount - 30; x++) {
                [mutDict removeObjectForKey:[mediaList firstObject]];
                [mediaList removeObjectAtIndex:0];
            }
        }
    } else {
        [mutDict removeObjectForKey:url];
        [mediaList removeObject:url];
    }
    [defaults setObject:mutDict forKey:@"recentlyPlayedMedia"];
    [defaults setObject:mediaList forKey:@"recentlyPlayedMediaList"];
    [defaults synchronize];
}

@end
