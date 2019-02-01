/*****************************************************************************
 * VLCRemoteControlService.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2017, 2018 VLC authors and VideoLAN
 *
 * Authors: Carola Nitz <nitz.carola # gmail.com>
 *          Felix Paul Kühne <fkuehne # videolan.org>
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

#import <MediaPlayer/MediaPlayer.h>

#import "VLCRemoteControlService.h"
#import "coreinteraction/VLCCoreInteraction.h"
#import "main/VLCMain.h"
#import "main/CompatibilityFixes.h"

#define kVLCSettingPlaybackForwardSkipLength @(60)
#define kVLCSettingPlaybackBackwardSkipLength @(60)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

@implementation VLCRemoteControlService

static inline NSArray * RemoteCommandCenterCommandsToHandle()
{
    MPRemoteCommandCenter *cc = [MPRemoteCommandCenter sharedCommandCenter];
    NSMutableArray *commands = [NSMutableArray arrayWithObjects:
                                cc.playCommand,
                                cc.pauseCommand,
                                cc.stopCommand,
                                cc.togglePlayPauseCommand,
                                cc.nextTrackCommand,
                                cc.previousTrackCommand,
                                cc.skipForwardCommand,
                                cc.skipBackwardCommand,
                                cc.changePlaybackPositionCommand,
                                nil];
    return [commands copy];
}

- (void)subscribeToRemoteCommands
{
    MPRemoteCommandCenter *commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

    //Enable when you want to support these
    commandCenter.ratingCommand.enabled = NO;
    commandCenter.likeCommand.enabled = NO;
    commandCenter.dislikeCommand.enabled = NO;
    commandCenter.bookmarkCommand.enabled = NO;
    commandCenter.enableLanguageOptionCommand.enabled = NO;
    commandCenter.disableLanguageOptionCommand.enabled = NO;
    commandCenter.seekForwardCommand.enabled = NO;
    commandCenter.seekBackwardCommand.enabled = NO;

    commandCenter.skipForwardCommand.preferredIntervals = @[kVLCSettingPlaybackForwardSkipLength];
    commandCenter.skipBackwardCommand.preferredIntervals = @[kVLCSettingPlaybackBackwardSkipLength];

    for (MPRemoteCommand *command in RemoteCommandCenterCommandsToHandle()) {
        [command addTarget:self action:@selector(remoteCommandEvent:)];
    }

}

- (void)unsubscribeFromRemoteCommands
{
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;

    for (MPRemoteCommand *command in RemoteCommandCenterCommandsToHandle()) {
        [command removeTarget:self];
    }
}

- (MPRemoteCommandHandlerStatus )remoteCommandEvent:(MPRemoteCommandEvent *)event
{
    MPRemoteCommandCenter *cc = [MPRemoteCommandCenter sharedCommandCenter];
    VLCCoreInteraction *coreInteraction = [VLCCoreInteraction sharedInstance];

    if (event.command == cc.playCommand) {
        [coreInteraction play];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.pauseCommand) {
        [coreInteraction pause];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.stopCommand) {
        [coreInteraction stop];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.togglePlayPauseCommand) {
        [coreInteraction playOrPause];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.nextTrackCommand) {
        [coreInteraction next];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.previousTrackCommand) {
        [coreInteraction previous];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.skipForwardCommand) {
        [coreInteraction forwardMedium];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.skipBackwardCommand) {
        [coreInteraction backwardMedium];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.changePlaybackPositionCommand) {
        MPChangePlaybackPositionCommandEvent *positionEvent = (MPChangePlaybackPositionCommandEvent *)event;
        [coreInteraction jumpToTime: vlc_tick_from_sec( positionEvent.positionTime )];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.changeRepeatModeCommand) {
        MPChangeRepeatModeCommandEvent *repeatEvent = (MPChangeRepeatModeCommandEvent *)event;
        MPRepeatType repeatType = repeatEvent.repeatType;
        switch (repeatType) {
            case MPRepeatTypeAll:
                [coreInteraction repeatAll];
                 break;

            case MPRepeatTypeOne:
                [coreInteraction repeatOne];
                break;

            default:
                [coreInteraction repeatOff];
                break;
        }
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.changeShuffleModeCommand) {
        [coreInteraction shuffle];
        return MPRemoteCommandHandlerStatusSuccess;
    }

    msg_Dbg(getIntf(), "%s Wasn't able to handle remote control event: %s",__PRETTY_FUNCTION__,[event.description UTF8String]);
    return MPRemoteCommandHandlerStatusCommandFailed;
}

@end

#pragma clang diagnostic pop
