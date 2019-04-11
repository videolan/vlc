/*****************************************************************************
 * VLCPlaybackContinuityController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2015-2019 VLC authors and VideoLAN
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

#import "main/VLCMain.h"
#import "panels/dialogs/VLCResumeDialogController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import <vlc_url.h>

@interface VLCPlaybackContinuityController()
{
    __weak VLCMain *_mainInstance;
    input_item_t *p_current_input;
}
@end

@implementation VLCPlaybackContinuityController

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray array], @"recentlyPlayedMediaList",
                                 [NSDictionary dictionary], @"recentlyPlayedMedia", nil];

    [defaults registerDefaults:appDefaults];
}

- (id)init
{
    self = [super init];
    if (self) {
        _mainInstance = [VLCMain sharedInstance];
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

    if (p_current_input) {
        /* continue playback where you left off */
        [self storePlaybackPositionForItem:p_current_input player:_mainInstance.playlistController.playerController];
    }
}

- (void)inputItemChanged:(NSNotification *)aNotification
{
    // Cancel pending resume dialogs
    [[_mainInstance resumeDialog] cancel];

    // object is hold here and released then it is dead
    p_current_input = [[_mainInstance playlistController] currentlyPlayingInputItem];
    if (p_current_input) {
        VLCPlaylistController *playlistController = aNotification.object;
        [self continuePlaybackWhereYouLeftOff:p_current_input player:playlistController.playerController];
    }
}

- (void)playbackStatusUpdated:(NSNotification *)aNotification
{
    // On shutdown, input might not be dead yet. Cleanup actions like itunes playback
    // and playback positon are done in different code paths (dealloc and appWillTerminate:).
    if ([_mainInstance isTerminating]) {
        return;
    }

    VLCPlayerController *playerController = aNotification.object;
    enum vlc_player_state playerState = [playerController playerState];

    if (playerState == VLC_PLAYER_STATE_STOPPED || playerState == VLC_PLAYER_STATE_STOPPING) {
        /* continue playback where you left off */
        if (p_current_input)
            [self storePlaybackPositionForItem:p_current_input player:playerController];
    }
}

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

- (void)continuePlaybackWhereYouLeftOff:(input_item_t *)p_input_item player:(VLCPlayerController *)playerController
{
    NSDictionary *recentlyPlayedFiles = [[NSUserDefaults standardUserDefaults] objectForKey:@"recentlyPlayedMedia"];
    if (!recentlyPlayedFiles)
        return;

    if (!p_input_item)
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
    if (![self isValidResumeItem:p_input_item])
        return;

    char *psz_url = vlc_uri_decode(input_item_GetURI(p_input_item));
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

        [playerController setTimePrecise: lastPos];
    };

    if (settingValue == 1) { // always
        completionBlock(RESUME_NOW);
        return;
    }

    [[_mainInstance resumeDialog] showWindowWithItem:p_input_item
                                    withLastPosition:lastPosition.intValue
                                     completionBlock:completionBlock];

}

- (void)storePlaybackPositionForItem:(input_item_t *)p_input_item player:(VLCPlayerController *)playerController
{
    if (!var_InheritBool(getIntf(), "macosx-recentitems"))
        return;

    if (!p_input_item)
        return;

    if (![self isValidResumeItem:p_input_item])
        return;

    char *psz_url = vlc_uri_decode(input_item_GetURI(p_input_item));
    if (!psz_url)
        return;
    NSString *url = toNSStr(psz_url);
    free(psz_url);

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableDictionary *mutDict = [[NSMutableDictionary alloc] initWithDictionary:[defaults objectForKey:@"recentlyPlayedMedia"]];

    float relativePos = playerController.position;
    long long pos = SEC_FROM_VLC_TICK(playerController.time);
    long long dur = SEC_FROM_VLC_TICK(input_item_GetDuration(p_input_item));

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
