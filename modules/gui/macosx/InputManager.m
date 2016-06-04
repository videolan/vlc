/*****************************************************************************
 * InputManager.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 * $Id$
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

#import "InputManager.h"

#import "CoreInteraction.h"
#import "CompatibilityFixes.h"
#import "ExtensionsManager.h"
#import "intf.h"
#import "MainMenu.h"
#import "MainWindow.h"
#import "VLCPlaylist.h"
#import "VLCPlaylistInfo.h"
#import "TrackSynchronization.h"
#import "VideoView.h"

#import "iTunes.h"
#import "Spotify.h"

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
                [[[VLCMain sharedInstance] mainMenu] performSelectorOnMainThread:@selector(updatePlaybackRate) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_POSITION:
                [[[VLCMain sharedInstance] mainWindow] performSelectorOnMainThread:@selector(updateTimeSlider) withObject: nil waitUntilDone:NO];
                [[[VLCMain sharedInstance] statusBarIcon] performSelectorOnMainThread:@selector(updateProgress) withObject:nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_TITLE:
            case INPUT_EVENT_CHAPTER:
                [inputManager performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_CACHE:
                [inputManager performSelectorOnMainThread:@selector(updateMainWindow) withObject:nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_STATISTICS:
                dispatch_async(dispatch_get_main_queue(), ^{
                    [[[VLCMain sharedInstance] currentMediaInfoPanel] updateStatistics];
                });
                break;
            case INPUT_EVENT_ES:
                break;
            case INPUT_EVENT_TELETEXT:
                break;
            case INPUT_EVENT_AOUT:
                break;
            case INPUT_EVENT_VOUT:
                break;
            case INPUT_EVENT_ITEM_META:
            case INPUT_EVENT_ITEM_INFO:
                [inputManager performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
                [inputManager performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
                [inputManager performSelectorOnMainThread:@selector(updateMetaAndInfo) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_BOOKMARK:
                break;
            case INPUT_EVENT_RECORD:
                dispatch_async(dispatch_get_main_queue(), ^{
                    [[[VLCMain sharedInstance] mainMenu] updateRecordState: var_InheritBool(p_this, "record")];
                });
                break;
            case INPUT_EVENT_PROGRAM:
                [inputManager performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
                break;
            case INPUT_EVENT_ITEM_EPG:
                break;
            case INPUT_EVENT_SIGNAL:
                break;

            case INPUT_EVENT_ITEM_NAME:
                [inputManager performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
                break;

            case INPUT_EVENT_AUDIO_DELAY:
            case INPUT_EVENT_SUBTITLE_DELAY:
                [inputManager performSelectorOnMainThread:@selector(updateDelays) withObject:nil waitUntilDone:NO];
                break;

            case INPUT_EVENT_DEAD:
                [inputManager performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
                [[[VLCMain sharedInstance] mainWindow] performSelectorOnMainThread:@selector(updateTimeSlider) withObject:nil waitUntilDone:NO];
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

    /* sleep management */
    IOPMAssertionID systemSleepAssertionID;
    IOPMAssertionID userActivityAssertionID;

    /* iTunes/Spotify play/pause support */
    BOOL b_has_itunes_paused;
    BOOL b_has_spotify_paused;
    NSTimer *o_itunes_play_timer;
}
@end

@implementation VLCInputManager

- (id)initWithMain:(VLCMain *)o_mainObj
{
    self = [super init];
    if(self) {
        msg_Dbg(getIntf(), "Initializing input manager");

        o_main = o_mainObj;
        var_AddCallback(pl_Get(getIntf()), "input-current", InputThreadChanged, (__bridge void *)self);

        informInputChangedQueue = dispatch_queue_create("org.videolan.vlc.inputChangedQueue", DISPATCH_QUEUE_SERIAL);

    }
    return self;
}

- (void)dealloc
{
    msg_Dbg(getIntf(), "Deinitializing input manager");
    if (p_current_input) {
        /* continue playback where you left off */
        [[o_main playlist] storePlaybackPositionForItem:p_current_input];

        var_DelCallback(p_current_input, "intf-event", InputEvent, (__bridge void *)self);
        vlc_object_release(p_current_input);
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
        vlc_object_release(p_current_input);
        p_current_input = NULL;

        [[o_main mainMenu] setRateControlsEnabled: NO];

        [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputChangedNotification
                                                            object:nil];
    }

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

        p_input_changed = vlc_object_hold(p_current_input);

        [[o_main playlist] currentlyPlayingItemChanged];

        [[o_main playlist] continuePlaybackWhereYouLeftOff:p_current_input];

        [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputChangedNotification
                                                            object:nil];
    }

    [self updateMetaAndInfo];

    [self updateMainWindow];
    [self updateDelays];
    [self updateMainMenu];

    /*
     * Due to constraints within NSAttributedString's main loop runtime handling
     * and other issues, we need to inform the extension manager on a separate thread.
     * The serial queue ensures that changed inputs are propagated in the same order as they arrive.
     */
    dispatch_async(informInputChangedQueue, ^{
        [[o_main extensionsManager] inputChanged:p_input_changed];
        if (p_input_changed)
            vlc_object_release(p_input_changed);
    });
}

- (void)playbackStatusUpdated
{
    intf_thread_t *p_intf = getIntf();
    int state = -1;
    if (p_current_input) {
        state = var_GetInteger(p_current_input, "state");
    }

    int i_control_itunes = var_InheritInteger(p_intf, "macosx-control-itunes");
    // cancel itunes timer if next item starts playing
    if (state > -1 && state != END_S && i_control_itunes > 0) {
        if (o_itunes_play_timer) {
            [o_itunes_play_timer invalidate];
            o_itunes_play_timer = nil;
        }
    }

    if (state == PLAYING_S) {
        if (i_control_itunes > 0) {
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

        BOOL shouldDisableScreensaver = var_InheritBool(p_intf, "disable-screensaver");

        /* Declare user activity.
         This wakes the display if it is off, and postpones display sleep according to the users system preferences
         Available from 10.7.3 */

#ifdef MAC_OS_X_VERSION_10_7
        if ([o_main activeVideoPlayback] && &IOPMAssertionDeclareUserActivity && shouldDisableScreensaver)
        {
            CFStringRef reasonForActivity = CFStringCreateWithCString(kCFAllocatorDefault, _("VLC media playback"), kCFStringEncodingUTF8);
            IOReturn success = IOPMAssertionDeclareUserActivity(reasonForActivity,
                                             kIOPMUserActiveLocal,
                                             &userActivityAssertionID);
            CFRelease(reasonForActivity);

            if (success != kIOReturnSuccess)
                msg_Warn(getIntf(), "failed to declare user activity");

        }
#endif

        /* prevent the system from sleeping */
        if (systemSleepAssertionID > 0) {
            msg_Dbg(getIntf(), "releasing old sleep blocker (%i)" , systemSleepAssertionID);
            IOPMAssertionRelease(systemSleepAssertionID);
        }

        IOReturn success;
        /* work-around a bug in 10.7.4 and 10.7.5, so check for 10.7.x < 10.7.4 and 10.8 */
        if ((NSAppKitVersionNumber >= 1115.2 && NSAppKitVersionNumber < 1138.45) || OSX_MOUNTAIN_LION || OSX_MAVERICKS || OSX_YOSEMITE || OSX_EL_CAPITAN) {
            CFStringRef reasonForActivity = CFStringCreateWithCString(kCFAllocatorDefault, _("VLC media playback"), kCFStringEncodingUTF8);
            if ([o_main activeVideoPlayback] && shouldDisableScreensaver)
                success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
            else
                success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
            CFRelease(reasonForActivity);
        } else {
            /* fall-back on the 10.5 mode, which also works on 10.7.4 and 10.7.5 */
            if ([o_main activeVideoPlayback] && shouldDisableScreensaver)
                success = IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
            else
                success = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
        }

        if (success == kIOReturnSuccess)
            msg_Dbg(getIntf(), "prevented sleep through IOKit (%i)", systemSleepAssertionID);
        else
            msg_Warn(getIntf(), "failed to prevent system sleep through IOKit");

        [[o_main mainMenu] setPause];
        [[o_main mainWindow] setPause];
    } else {
        [[o_main mainMenu] setSubmenusEnabled: FALSE];
        [[o_main mainMenu] setPlay];
        [[o_main mainWindow] setPlay];

        /* allow the system to sleep again */
        if (systemSleepAssertionID > 0) {
            msg_Dbg(getIntf(), "releasing sleep blocker (%i)" , systemSleepAssertionID);
            IOPMAssertionRelease(systemSleepAssertionID);
        }

        if (state == END_S || state == -1) {
            /* continue playback where you left off */
            if (p_current_input)
                [[o_main playlist] storePlaybackPositionForItem:p_current_input];

            if (i_control_itunes > 0) {
                if (o_itunes_play_timer) {
                    [o_itunes_play_timer invalidate];
                }
                o_itunes_play_timer = [NSTimer scheduledTimerWithTimeInterval: 0.5
                                                                       target: self
                                                                     selector: @selector(resumeItunesPlayback:)
                                                                     userInfo: nil
                                                                      repeats: NO];
            }
        }
    }

    [self updateMainWindow];
    [self sendDistributedNotificationWithUpdatedPlaybackStatus];
}


- (void)resumeItunesPlayback:(id)sender
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
    o_itunes_play_timer = nil;
}

- (void)updateMetaAndInfo
{
    if (!p_current_input) {
        [[[VLCMain sharedInstance] currentMediaInfoPanel] updatePanelWithItem:nil];
        return;
    }

    input_item_t *p_input_item = input_GetItem(p_current_input);

    [[[o_main playlist] model] updateItem:p_input_item];
    [[[VLCMain sharedInstance] currentMediaInfoPanel] updatePanelWithItem:p_input_item];
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
    [[o_main mainMenu] updatePlaybackRate];
    [[VLCCoreInteraction sharedInstance] resetAtoB];
}

- (void)sendDistributedNotificationWithUpdatedPlaybackStatus
{
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"VLCPlayerStateDidChange"
                                                                   object:nil
                                                                 userInfo:nil
                                                       deliverImmediately:YES];
}

- (BOOL)hasInput
{
    return p_current_input != NULL;
}

@end
