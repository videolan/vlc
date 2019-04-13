/*****************************************************************************
 * VLCClickerManager.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2019 Felix Paul Kühne
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "VLCClickerManager.h"

#include <vlc_common.h>
#include <vlc_actions.h>

#import "coreinteraction/VLCCoreInteraction.h"
#import "extensions/NSSound+VLCAdditions.h"
#import "imported/SPMediaKeyTap/SPMediaKeyTap.h"
#import "imported/AppleRemote/AppleRemote.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistModel.h"

@interface VLCClickerManager()
{
    /* media key support */
    BOOL b_mediaKeySupport;
    BOOL b_mediakeyJustJumped;
    SPMediaKeyTap *_mediaKeyController;
    BOOL b_mediaKeyTrapEnabled;

    AppleRemote *_remote;
    BOOL b_remote_button_hold; /* true as long as the user holds the left,right,plus or minus on the remote control */
}
@end

@implementation VLCClickerManager

- (instancetype)init
{
    self = [super init];
    if (self) {
        /* init media key support */
        b_mediaKeySupport = var_InheritBool(getIntf(), "macosx-mediakeys");
        if (b_mediaKeySupport) {
            _mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];
        }
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(coreChangedMediaKeySupportSetting:)
                                   name:VLCMediaKeySupportSettingChangedNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(coreChangedAppleRemoteSetting:)
                                   name:VLCAppleRemoteSettingChangedNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(startListeningWithAppleRemote)
                                   name:NSApplicationDidBecomeActiveNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(stopListeningWithAppleRemote)
                                   name:NSApplicationDidResignActiveNotification
                                 object:nil];

        /* init Apple Remote support */
        _remote = [[AppleRemote alloc] init];
        [_remote setClickCountEnabledButtons: kRemoteButtonPlay];
        [_remote setDelegate: self];
    }
    return self;
}

- (void)dealloc
{
    _mediaKeyController = nil;
    _remote = nil;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark -
#pragma mark Media Key support

- (void)resetMediaKeyJump
{
    b_mediakeyJustJumped = NO;
}

- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return;

    b_mediaKeySupport = var_InheritBool(p_intf, "macosx-mediakeys");
    if (b_mediaKeySupport && !_mediaKeyController)
        _mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];

    VLCMain *main = [VLCMain sharedInstance];
    if (b_mediaKeySupport && ([[[main playlistController] playlistModel] numberOfPlaylistItems] > 0)) {
        if (!b_mediaKeyTrapEnabled) {
            msg_Dbg(p_intf, "Enabling media key support");
            if ([_mediaKeyController startWatchingMediaKeys]) {
                b_mediaKeyTrapEnabled = YES;
            } else {
                msg_Warn(p_intf, "Failed to enable media key support, likely "
                         "app needs to be whitelisted in Security Settings.");
            }
        }
    } else {
        if (b_mediaKeyTrapEnabled) {
            b_mediaKeyTrapEnabled = NO;
            msg_Dbg(p_intf, "Disabling media key support");
            [_mediaKeyController stopWatchingMediaKeys];
        }
    }
}

-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event
{
    if (b_mediaKeySupport) {
        assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

        int keyCode = (([event data1] & 0xFFFF0000) >> 16);
        int keyFlags = ([event data1] & 0x0000FFFF);
        int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
        int keyRepeat = (keyFlags & 0x1);

        if (keyCode == NX_KEYTYPE_PLAY && keyState == 0)
            [[VLCCoreInteraction sharedInstance] playOrPause];

        if ((keyCode == NX_KEYTYPE_FAST || keyCode == NX_KEYTYPE_NEXT) && !b_mediakeyJustJumped) {
            if (keyState == 0 && keyRepeat == 0)
                [[VLCCoreInteraction sharedInstance] next];
            else if (keyRepeat == 1) {
                [[VLCCoreInteraction sharedInstance] forwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }

        if ((keyCode == NX_KEYTYPE_REWIND || keyCode == NX_KEYTYPE_PREVIOUS) && !b_mediakeyJustJumped) {
            if (keyState == 0 && keyRepeat == 0)
                [[VLCCoreInteraction sharedInstance] previous];
            else if (keyRepeat == 1) {
                [[VLCCoreInteraction sharedInstance] backwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }
    }
}

#pragma mark -
#pragma mark Apple Remote Control

- (void)coreChangedAppleRemoteSetting: (NSNotification *)notification
{
    if (var_InheritBool(getIntf(), "macosx-appleremote") == YES) {
        [_remote startListening: self];
    } else {
        [_remote stopListening:self];
    }
}

- (void)startListeningWithAppleRemote
{
    if (var_InheritBool(getIntf(), "macosx-appleremote") == YES)
        [_remote startListening: self];
}

- (void)stopListeningWithAppleRemote
{
    [_remote stopListening:self];
}

/* Helper method for the remote control interface in order to trigger forward/backward and volume
 increase/decrease as long as the user holds the left/right, plus/minus button */
- (void) executeHoldActionForRemoteButton: (NSNumber*) buttonIdentifierNumber
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return;

    if (b_remote_button_hold) {
        switch([buttonIdentifierNumber intValue]) {
            case kRemoteButtonRight_Hold:
                [[VLCCoreInteraction sharedInstance] forwardShort];
                break;
            case kRemoteButtonLeft_Hold:
                [[VLCCoreInteraction sharedInstance] backwardShort];
                break;
            case kRemoteButtonVolume_Plus_Hold:
                if (p_intf)
                    var_SetInteger(vlc_object_instance(p_intf), "key-action", ACTIONID_VOL_UP);
                break;
            case kRemoteButtonVolume_Minus_Hold:
                if (p_intf)
                    var_SetInteger(vlc_object_instance(p_intf), "key-action", ACTIONID_VOL_DOWN);
                break;
        }
        if (b_remote_button_hold) {
            /* trigger event */
            [self performSelector:@selector(executeHoldActionForRemoteButton:)
                       withObject:buttonIdentifierNumber
                       afterDelay:0.25];
        }
    }
}

/* Apple Remote callback */
- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier
               pressedDown: (BOOL) pressedDown
                clickCount: (unsigned int) count
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return;

    switch(buttonIdentifier) {
        case k2009RemoteButtonFullscreen:
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            break;
        case k2009RemoteButtonPlay:
            [[VLCCoreInteraction sharedInstance] playOrPause];
            break;
        case kRemoteButtonPlay:
            if (count >= 2)
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            else
                [[VLCCoreInteraction sharedInstance] playOrPause];
            break;
        case kRemoteButtonVolume_Plus:
            if (config_GetInt("macosx-appleremote-sysvol"))
                [NSSound increaseSystemVolume];
            else
                if (p_intf)
                    var_SetInteger(vlc_object_instance(p_intf), "key-action", ACTIONID_VOL_UP);
            break;
        case kRemoteButtonVolume_Minus:
            if (config_GetInt("macosx-appleremote-sysvol"))
                [NSSound decreaseSystemVolume];
            else
                if (p_intf)
                    var_SetInteger(vlc_object_instance(p_intf), "key-action", ACTIONID_VOL_DOWN);
            break;
        case kRemoteButtonRight:
            if (config_GetInt("macosx-appleremote-prevnext"))
                [[VLCCoreInteraction sharedInstance] forwardShort];
            else
                [[VLCCoreInteraction sharedInstance] next];
            break;
        case kRemoteButtonLeft:
            if (config_GetInt("macosx-appleremote-prevnext"))
                [[VLCCoreInteraction sharedInstance] backwardShort];
            else
                [[VLCCoreInteraction sharedInstance] previous];
            break;
        case kRemoteButtonRight_Hold:
        case kRemoteButtonLeft_Hold:
        case kRemoteButtonVolume_Plus_Hold:
        case kRemoteButtonVolume_Minus_Hold:
            /* simulate an event as long as the user holds the button */
            b_remote_button_hold = pressedDown;
            if (pressedDown) {
                NSNumber* buttonIdentifierNumber = [NSNumber numberWithInt:buttonIdentifier];
                [self performSelector:@selector(executeHoldActionForRemoteButton:)
                           withObject:buttonIdentifierNumber];
            }
            break;
        case kRemoteButtonMenu:
            [[VLCCoreInteraction sharedInstance] showPosition];
            break;
        case kRemoteButtonPlay_Sleep:
        {
            NSAppleScript * script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Events\" to sleep"];
            [script executeAndReturnError:nil];
            break;
        }
        default:
            /* Add here whatever you want other buttons to do */
            break;
    }
}

@end
