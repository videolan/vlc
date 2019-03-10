/*****************************************************************************
 * applescript.m: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "applescript.h"

#import <vlc_common.h>
#import <vlc_url.h>

#import "main/VLCMain.h"
#import "coreinteraction/VLCCoreInteraction.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "windows/VLCOpenInputMetadata.h"

/*****************************************************************************
 * VLGetURLScriptCommand implementation
 *****************************************************************************/
@implementation VLGetURLScriptCommand

- (id)performDefaultImplementation
{
    NSString *commandString = [[self commandDescription] commandName];
    NSString *parameterString = [self directParameter];

    if ([commandString isEqualToString:@"GetURL"] || [commandString isEqualToString:@"OpenURL"]) {
        if (parameterString) {
            VLCOpenInputMetadata *inputMetadata = [[VLCOpenInputMetadata alloc] init];
            inputMetadata.MRLString = parameterString;

            [[[VLCMain sharedInstance] playlistController] addPlaylistItems:@[inputMetadata]];
        }
    }
    return nil;
}

@end


/*****************************************************************************
 * VLControlScriptCommand implementation
 *****************************************************************************/
/*
 * This entire control command needs a better design. more object oriented.
 * Applescript developers would be very welcome (hartman)
 */
@implementation VLControlScriptCommand

- (id)performDefaultImplementation
{
    NSString *commandString = [[self commandDescription] commandName];
    NSString *parameterString = [self directParameter];
    VLCCoreInteraction *coreInteractionInstance = [VLCCoreInteraction sharedInstance];

    if ([commandString isEqualToString:@"play"])
        [coreInteractionInstance playOrPause];
    else if ([commandString isEqualToString:@"stop"])
        [coreInteractionInstance stop];
    else if ([commandString isEqualToString:@"previous"])
        [coreInteractionInstance previous];
    else if ([commandString isEqualToString:@"next"])
        [coreInteractionInstance next];
    else if ([commandString isEqualToString:@"fullscreen"])
        [coreInteractionInstance toggleFullscreen];
    else if ([commandString isEqualToString:@"mute"])
        [coreInteractionInstance toggleMute];
    else if ([commandString isEqualToString:@"volumeUp"])
        [coreInteractionInstance volumeUp];
    else if ([commandString isEqualToString:@"volumeDown"])
        [coreInteractionInstance volumeDown];
    else if ([commandString isEqualToString:@"moveMenuFocusUp"])
        [coreInteractionInstance moveMenuFocusUp];
    else if ([commandString isEqualToString:@"moveMenuFocusDown"])
        [coreInteractionInstance moveMenuFocusDown];
    else if ([commandString isEqualToString:@"moveMenuFocusLeft"])
        [coreInteractionInstance moveMenuFocusLeft];
    else if ([commandString isEqualToString:@"moveMenuFocusRight"])
        [coreInteractionInstance moveMenuFocusRight];
    else if ([commandString isEqualToString:@"menuFocusActivate"])
        [coreInteractionInstance menuFocusActivate];
    else if ([commandString isEqualToString:@"stepForward"]) {
        //default: forwardShort
        if (parameterString) {
            int parameterInt = [parameterString intValue];
            switch (parameterInt) {
                case 1:
                    [coreInteractionInstance forwardExtraShort];
                    break;
                case 2:
                    [coreInteractionInstance forwardShort];
                    break;
                case 3:
                    [coreInteractionInstance forwardMedium];
                    break;
                case 4:
                    [coreInteractionInstance forwardLong];
                    break;
                default:
                    [coreInteractionInstance forwardShort];
                    break;
            }
        } else
            [coreInteractionInstance forwardShort];
    } else if ([commandString isEqualToString:@"stepBackward"]) {
        //default: backwardShort
        if (parameterString) {
            int parameterInt = [parameterString intValue];
            switch (parameterInt) {
                case 1:
                    [coreInteractionInstance backwardExtraShort];
                    break;
                case 2:
                    [coreInteractionInstance backwardShort];
                    break;
                case 3:
                    [coreInteractionInstance backwardMedium];
                    break;
                case 4:
                    [coreInteractionInstance backwardLong];
                    break;
                default:
                    [coreInteractionInstance backwardShort];
                    break;
            }
        } else
            [coreInteractionInstance backwardShort];
    }
   return nil;
}

@end

/*****************************************************************************
 * Category that adds AppleScript support to NSApplication
 *****************************************************************************/
@implementation NSApplication(ScriptSupport)

- (BOOL)scriptFullscreenMode
{
    return [[[[VLCMain sharedInstance] playlistController] playerController] fullscreen];
}

- (void)setScriptFullscreenMode:(BOOL)mode
{
    [[[[VLCMain sharedInstance] playlistController] playerController] setFullscreen:mode];
}

- (BOOL)muted
{
    return [[VLCCoreInteraction sharedInstance] mute];
}

- (BOOL)playing
{
    enum vlc_player_state playerState = [[[[VLCMain sharedInstance] playlistController] playerController] playerState];

    if (playerState == VLC_PLAYER_STATE_STARTED || playerState == VLC_PLAYER_STATE_PLAYING) {
        return YES;
    }

    return NO;
}

- (int)audioVolume
{
    return [[VLCCoreInteraction sharedInstance] volume];
}

- (void)setAudioVolume:(int)volume
{
    [[VLCCoreInteraction sharedInstance] setVolume:volume];
}

- (long long)audioDesync
{
    return MS_FROM_VLC_TICK([[[[VLCMain sharedInstance] playlistController] playerController] audioDelay]);
}

- (void)setAudioDesync:(long long)audioDelay
{
    [[[[VLCMain sharedInstance] playlistController] playerController] setAudioDelay: VLC_TICK_FROM_MS(audioDelay)];
}

- (int)currentTime
{
    return (int)SEC_FROM_VLC_TICK([[[[VLCMain sharedInstance] playlistController] playerController] time]);
}

- (void)setCurrentTime:(int)currentTime
{
    [[[[VLCMain sharedInstance] playlistController] playerController] setTimeFast: VLC_TICK_FROM_SEC(currentTime)];
}

- (NSInteger)durationOfCurrentItem
{
    return [[VLCCoreInteraction sharedInstance] durationOfCurrentPlaylistItem];
}

- (NSString *)pathOfCurrentItem
{
    return [[[VLCCoreInteraction sharedInstance] URLOfCurrentPlaylistItem] path];
}

- (NSString *)nameOfCurrentItem
{
    return [[VLCCoreInteraction sharedInstance] nameOfCurrentPlaylistItem];
}

- (BOOL)playbackShowsMenu
{
    const struct vlc_player_title *currentTitle = [[[[VLCMain sharedInstance] playlistController] playerController] selectedTitle];
    if (currentTitle == NULL) {
        return NO;
    }

    if (currentTitle->flags & VLC_PLAYER_TITLE_MENU) {
        return YES;
    }

    return NO;
}

@end
