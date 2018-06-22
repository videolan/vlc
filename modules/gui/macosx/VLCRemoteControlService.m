/*****************************************************************************
 * VLCRemoteControlService.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2017-2021 VLC authors and VideoLAN
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
#import "VLCMain.h"
#import "CompatibilityFixes.h"
#import "VLCPlaylist.h"
#import "VLCCoreInteraction.h"
#import "VLCInputManager.h"

#define kVLCSettingPlaybackForwardSkipLength @(60)
#define kVLCSettingPlaybackBackwardSkipLength @(60)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

@interface VLCRemoteControlService()
{
    VLCCoreInteraction *_coreInteraction;
}
@end

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

- (instancetype)init
{
    self = [super init];
    if (self) {
        _coreInteraction = [VLCCoreInteraction sharedInstance];

        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(playbackRateChanged:)
                                   name:VLCPlayerRateChanged
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)playbackStateChangedTo:(int)state
{
    MPNowPlayingInfoCenter *nowPlayingInfoCenter = [MPNowPlayingInfoCenter defaultCenter];

    switch (state) {
        case PLAYING_S:
            nowPlayingInfoCenter.playbackState = MPNowPlayingPlaybackStatePlaying;
            break;

        case PAUSE_S:
            nowPlayingInfoCenter.playbackState = MPNowPlayingPlaybackStatePaused;
            break;

        case END_S:
        case -1:
            nowPlayingInfoCenter.playbackState = MPNowPlayingPlaybackStateStopped;
            break;

        default:
            nowPlayingInfoCenter.playbackState = MPNowPlayingPlaybackStateUnknown;
            break;
    }
}

- (void)playbackPositionUpdated
{
    MPNowPlayingInfoCenter *nowPlayingInfoCenter = [MPNowPlayingInfoCenter defaultCenter];

    NSMutableDictionary *currentlyPlayingTrackInfo = [nowPlayingInfoCenter.nowPlayingInfo mutableCopy];
    [self setTimeInformationForDictionary:currentlyPlayingTrackInfo];

    nowPlayingInfoCenter.nowPlayingInfo = currentlyPlayingTrackInfo;
}

- (void)playbackRateChanged:(NSNotification *)aNotification
{
    MPNowPlayingInfoCenter *nowPlayingInfoCenter = [MPNowPlayingInfoCenter defaultCenter];

    NSMutableDictionary *currentlyPlayingTrackInfo = [nowPlayingInfoCenter.nowPlayingInfo mutableCopy];
    [self setRateInformationForDictionary:currentlyPlayingTrackInfo];

    nowPlayingInfoCenter.nowPlayingInfo = currentlyPlayingTrackInfo;
}

- (void)metaDataChangedForCurrentMediaItem:(input_item_t *)p_input_item
{
    if (!p_input_item) {
        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
        return;
    }

    NSMutableDictionary *currentlyPlayingTrackInfo = [NSMutableDictionary dictionary];
    [self setTimeInformationForDictionary:currentlyPlayingTrackInfo];
    [self setRateInformationForDictionary:currentlyPlayingTrackInfo];

    /* fill title info */
    char *psz_title = input_item_GetTitle(p_input_item);
    if (!psz_title)
        psz_title = input_item_GetName(p_input_item);
    [currentlyPlayingTrackInfo setValue:toNSStr(psz_title) forKey:MPMediaItemPropertyTitle];
    free(psz_title);

    char *psz_artist = input_item_GetArtist(p_input_item);
    [currentlyPlayingTrackInfo setValue:toNSStr(psz_artist) forKey:MPMediaItemPropertyArtist];
    free(psz_artist);

    char *psz_albumName = input_item_GetAlbum(p_input_item);
    [currentlyPlayingTrackInfo setValue:toNSStr(psz_albumName) forKey:MPMediaItemPropertyAlbumTitle];
    free(psz_albumName);

    char *psz_trackNumber = input_item_GetTrackNumber(p_input_item);
    [currentlyPlayingTrackInfo setValue:[NSNumber numberWithInt:[toNSStr(psz_trackNumber) intValue]] forKey:MPMediaItemPropertyAlbumTrackNumber];
    free(psz_trackNumber);

    vlc_tick_t duration = input_item_GetDuration(p_input_item) / 1000000;
    [currentlyPlayingTrackInfo setValue:[NSNumber numberWithLongLong:duration] forKey:MPMediaItemPropertyPlaybackDuration];
    if (duration > 0) {
        [currentlyPlayingTrackInfo setValue:[NSNumber numberWithBool:NO] forKey:MPNowPlayingInfoPropertyIsLiveStream];
    } else {
        [currentlyPlayingTrackInfo setValue:[NSNumber numberWithBool:YES] forKey:MPNowPlayingInfoPropertyIsLiveStream];
    }

    char *psz_artworkURL = input_item_GetArtworkURL(p_input_item);
    if (psz_artworkURL) {
        NSString *artworkURL = toNSStr(psz_artworkURL);
        if (![artworkURL hasPrefix:@"attachment://"]) {
            NSImage *coverArtImage = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:artworkURL]];
            if (coverArtImage) {
                MPMediaItemArtwork *mpartwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:coverArtImage.size
                                                                                requestHandler:^NSImage* _Nonnull(CGSize size) {
                    return coverArtImage;
                }];
                [currentlyPlayingTrackInfo setValue:mpartwork forKey:MPMediaItemPropertyArtwork];
            }
        }
    }
    free(psz_artworkURL);

    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = currentlyPlayingTrackInfo;
}

- (void)setTimeInformationForDictionary:(NSMutableDictionary *)dictionary
{
    [dictionary setValue:[NSNumber numberWithLongLong:_coreInteraction.currentPlaybackTimeInSeconds] forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    [dictionary setValue:[NSNumber numberWithFloat:_coreInteraction.currentPlaybackPosition] forKey:MPNowPlayingInfoPropertyPlaybackProgress];
}

- (void)setRateInformationForDictionary:(NSMutableDictionary *)dictionary
{
    [dictionary setValue:[NSNumber numberWithFloat:_coreInteraction.internalPlaybackRate] forKey:MPNowPlayingInfoPropertyPlaybackRate];
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

- (MPRemoteCommandHandlerStatus)remoteCommandEvent:(MPRemoteCommandEvent *)event
{
    MPRemoteCommandCenter *cc = [MPRemoteCommandCenter sharedCommandCenter];

    if (event.command == cc.playCommand) {
        [_coreInteraction play];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.pauseCommand) {
        [_coreInteraction pause];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.stopCommand) {
        [_coreInteraction stop];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.togglePlayPauseCommand) {
        [_coreInteraction playOrPause];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.nextTrackCommand) {
        [_coreInteraction next];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.previousTrackCommand) {
        [_coreInteraction previous];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.skipForwardCommand) {
        [_coreInteraction forwardMedium];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.skipBackwardCommand) {
        [_coreInteraction backwardMedium];
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.changePlaybackPositionCommand) {
        MPChangePlaybackPositionCommandEvent *positionEvent = (MPChangePlaybackPositionCommandEvent *)event;
        return [_coreInteraction seekToTime:positionEvent.positionTime * 1000000] ? MPRemoteCommandHandlerStatusSuccess : MPRemoteCommandHandlerStatusCommandFailed;
    }
    if (event.command == cc.changeRepeatModeCommand) {
        MPChangeRepeatModeCommandEvent *repeatEvent = (MPChangeRepeatModeCommandEvent *)event;
        MPRepeatType repeatType = repeatEvent.repeatType;
        switch (repeatType) {
            case MPRepeatTypeAll:
                [_coreInteraction repeatAll];
                 break;

            case MPRepeatTypeOne:
                [_coreInteraction repeatOne];
                break;

            default:
                [_coreInteraction repeatOff];
                break;
        }
        return MPRemoteCommandHandlerStatusSuccess;
    }
    if (event.command == cc.changeShuffleModeCommand) {
        [_coreInteraction shuffle];
        return MPRemoteCommandHandlerStatusSuccess;
    }

    msg_Dbg(getIntf(), "%s Wasn't able to handle remote control event: %s",__PRETTY_FUNCTION__,[event.description UTF8String]);
    return MPRemoteCommandHandlerStatusCommandFailed;
}

@end

#pragma clang diagnostic pop
