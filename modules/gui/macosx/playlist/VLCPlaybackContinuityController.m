/*****************************************************************************
 * VLCPlaybackContinuityController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2015-2020 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan.org>
 *          David Fuhrmann <dfuhrmann # videolan.org>
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

#import "VLCPlaybackContinuityController.h"

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "panels/dialogs/VLCResumeDialogController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"

static const int64_t MinimumDuration = 3 * 60 * 1000;
static const float MinimumStorePercent = 0.05;
static const float MaximumStorePercent = 0.95;
static const int64_t MinimumStoreTime = 60 * 1000;
static const int64_t MinimumStoreRemainingTime = 60 * 1000;
static NSString *VLCRecentlyPlayedMediaKey = @"recentlyPlayedMedia";
static NSString *VLCRecentlyPlayedMediaListKey = @"recentlyPlayedMediaList";

@interface VLCPlaybackContinuityController()
{
    VLCInputItem *_currentInput;
    VLCResumeDialogController *_resumeDialogController;
}
@end

@implementation VLCPlaybackContinuityController

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray array], VLCRecentlyPlayedMediaListKey,
                                 [NSDictionary dictionary], VLCRecentlyPlayedMediaKey, nil];

    [defaults registerDefaults:appDefaults];
}

- (id)init
{
    self = [super init];
    if (self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(inputItemChanged:)
                                   name:VLCPlaylistCurrentItemChanged
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(playbackStatusUpdated:)
                                   name:VLCPlayerStateChanged
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    msg_Dbg(getIntf(), "Deinitializing input manager");

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    if (_currentInput) {
        /* continue playback where you left off */
        [self storePlaybackPositionForItem:_currentInput player:[VLCMain sharedInstance].playlistController.playerController];
    }
}

- (void)inputItemChanged:(NSNotification *)aNotification
{
    VLCMain *mainInstance = [VLCMain sharedInstance];
    // Cancel pending resume dialogs
    [_resumeDialogController cancel];

    // object is hold here and released then it is dead
    _currentInput = [[mainInstance playlistController] currentlyPlayingInputItem];
    if (_currentInput) {
        VLCPlaylistController *playlistController = aNotification.object;
        [self continuePlaybackWhereYouLeftOff:_currentInput player:playlistController.playerController];
    }
}

- (void)playbackStatusUpdated:(NSNotification *)aNotification
{
    // On shutdown, input might not be dead yet. Cleanup actions like itunes playback
    // and playback positon are done in different code paths (dealloc and appWillTerminate:).
    if ([[VLCMain sharedInstance] isTerminating]) {
        return;
    }

    VLCPlayerController *playerController = aNotification.object;
    enum vlc_player_state playerState = [playerController playerState];

    if (playerState == VLC_PLAYER_STATE_STOPPED || playerState == VLC_PLAYER_STATE_STOPPING) {
        /* continue playback where you left off */
        if (_currentInput)
            [self storePlaybackPositionForItem:_currentInput player:playerController];
    }
}

- (BOOL)isValidResumeItem:(VLCInputItem *)inputItem
{
    NSString *urlString = inputItem.MRL;

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

- (void)continuePlaybackWhereYouLeftOff:(VLCInputItem *)inputItem player:(VLCPlayerController *)playerController
{
    int settingValue = (int)config_GetInt("macosx-continue-playback");
    if (settingValue == 2) // never resume
        return;

    if (!inputItem)
        return;

    /* allow the user to over-write the start/stop/run-time */
    // FIXME: reimplement using new playlist
#if 0
    if (var_GetFloat(p_input_thread, "run-time") > 0 ||
        var_GetFloat(p_input_thread, "start-time") > 0 ||
        var_GetFloat(p_input_thread, "stop-time") != 0) {
        return;
    }
#endif

    /* check for file existance before resuming */
    if (![self isValidResumeItem:inputItem])
        return;

    NSURL *url = [NSURL URLWithString:inputItem.MRL];
    if (!url)
        return;

    VLCMediaLibraryMediaItem *libraryMediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:url];
    if (!libraryMediaItem) {
        // input item isn't part of the library
        [self resumeLegacyPlaybackPositionForInputItem:inputItem
                                                   ask:(settingValue == 0)
                                                player:playerController];
    }

    [self resumePlaybackPositionForLibraryMediaItem:libraryMediaItem
                                          inputItem:inputItem
                                                ask:(settingValue == 0)
                                             player:playerController];
}

- (void)resumePlaybackPositionForLibraryMediaItem:(VLCMediaLibraryMediaItem *)libraryMediaItem
                                        inputItem:(VLCInputItem *)inputItem
                                              ask:(BOOL)ask
                                           player:(VLCPlayerController *)playerController
{
    float lastPlaybackPosition = libraryMediaItem.lastPlaybackPosition;
    int64_t duration = libraryMediaItem.duration;

    if (lastPlaybackPosition < MinimumStorePercent || duration < MinimumDuration) {
        return;
    }

    CompletionBlock completionBlock = ^(enum ResumeResult result) {
        if (result == RESUME_RESTART)
            return;

        msg_Dbg(getIntf(), "continuing playback at %0.2f", lastPlaybackPosition);
        [playerController setPositionPrecise:lastPlaybackPosition];
    };

    if (!ask) { // always
        completionBlock(RESUME_NOW);
        return;
    }

    if (!_resumeDialogController) {
        _resumeDialogController = [[VLCResumeDialogController alloc] init];
    }

    [_resumeDialogController showWindowWithItem:inputItem
                               withLastPosition:(lastPlaybackPosition * duration) / 1000
                                completionBlock:completionBlock];
}

- (void)resumeLegacyPlaybackPositionForInputItem:(VLCInputItem *)inputItem
                                             ask:(BOOL)ask
                                          player:(VLCPlayerController *)playerController
{
    NSDictionary *recentlyPlayedFiles = [[NSUserDefaults standardUserDefaults] objectForKey:VLCRecentlyPlayedMediaKey];
    if (!recentlyPlayedFiles)
        return;

    NSNumber *lastPosition = [recentlyPlayedFiles objectForKey:inputItem.MRL];
    if (!lastPosition || lastPosition.intValue <= 0)
        return;

    CompletionBlock completionBlock = ^(enum ResumeResult result) {
        if (result == RESUME_RESTART)
            return;

        vlc_tick_t lastPos = vlc_tick_from_sec( lastPosition.intValue );
        msg_Dbg(getIntf(), "continuing playback at %lld", lastPos);

        [playerController setTimePrecise: lastPos];
    };

    if (!ask) { // always
        completionBlock(RESUME_NOW);
        return;
    }

    if (!_resumeDialogController) {
        _resumeDialogController = [[VLCResumeDialogController alloc] init];
    }

    [_resumeDialogController showWindowWithItem:inputItem
                               withLastPosition:lastPosition.intValue
                                completionBlock:completionBlock];
}

BOOL ShouldStorePlaybackPosition(float position, int64_t duration)
{
    int64_t positionTime = position * duration;
    int64_t remainingTime = duration - positionTime;

    if (duration < MinimumDuration) {
        return NO;
    }

    if (position < MinimumStorePercent && positionTime < MinimumStoreTime) {
        return NO;
    }

    if (position > MaximumStorePercent && remainingTime < MinimumStoreRemainingTime) {
        return NO;
    }

    return YES;
}

- (void)storePlaybackPositionForItem:(VLCInputItem *)inputItem
                              player:(VLCPlayerController *)playerController
{
    if (!var_InheritBool(getIntf(), "macosx-recentitems"))
        return;

    if (!inputItem)
        return;

    if (![self isValidResumeItem:inputItem])
        return;

    NSURL *url = [NSURL URLWithString:inputItem.MRL];
    if (!url)
        return;

    VLCMediaLibraryMediaItem *libraryMediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:url];
    if (!libraryMediaItem) {
        // input item isn't part of the library
        [self storeLegacyPlaybackPositionForInputItem:inputItem withPlayer:playerController];
    }

    if (ShouldStorePlaybackPosition(playerController.position, libraryMediaItem.duration)) {
        libraryMediaItem.lastPlaybackPosition = playerController.position;
    }
}

- (void)storeLegacyPlaybackPositionForInputItem:(VLCInputItem *)inputItem
                                     withPlayer:(VLCPlayerController *)playerController
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableDictionary *mutDict = [[NSMutableDictionary alloc] initWithDictionary:[defaults objectForKey:VLCRecentlyPlayedMediaKey]];

    float relativePos = playerController.position;
    long long pos = SEC_FROM_VLC_TICK(playerController.time);
    long long dur = SEC_FROM_VLC_TICK(inputItem.duration);

    NSMutableArray *mediaList = [[defaults objectForKey:VLCRecentlyPlayedMediaListKey] mutableCopy];
    NSString *mrl = inputItem.MRL;

    if (ShouldStorePlaybackPosition(relativePos, dur*1000)) {
        msg_Dbg(getIntf(), "Store current playback position of %f", relativePos);
        [mutDict setObject:[NSNumber numberWithInteger:pos] forKey:inputItem.MRL];

        [mediaList removeObject:mrl];
        [mediaList addObject:mrl];
        NSUInteger mediaListCount = mediaList.count;
        if (mediaListCount > 30) {
            for (NSUInteger x = 0; x < mediaListCount - 30; x++) {
                [mutDict removeObjectForKey:[mediaList firstObject]];
                [mediaList removeObjectAtIndex:0];
            }
        }
    } else {
        [mutDict removeObjectForKey:mrl];
        [mediaList removeObject:mrl];
    }
    [defaults setObject:mutDict forKey:VLCRecentlyPlayedMediaKey];
    [defaults setObject:mediaList forKey:VLCRecentlyPlayedMediaListKey];
}

@end
