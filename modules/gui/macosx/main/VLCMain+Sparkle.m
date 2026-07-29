/*****************************************************************************
 * VLCMain+Sparkle.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Claudio Cambra <developer at claudiocambra dot com>
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

#import "VLCMain+Sparkle.h"

#ifdef HAVE_SPARKLE

#import <Sparkle/Sparkle.h>

#import "extensions/NSString+Helpers.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayerController.h"

NSString *const kIntel64UpdateURLString = @"https://update.videolan.org/vlc/sparkle/vlc-intel64.xml";
NSString *const kARM64UpdateURLString = @"https://update.videolan.org/vlc/sparkle/vlc-arm64.xml";

@interface VLCMain () <SPUUpdaterDelegate>
@property (readwrite) SPUStandardUpdaterController *sparkleUpdaterController;
@end

@implementation VLCMain (Sparkle)

- (void)setupSparkle
{
    self.sparkleUpdaterController = [[SPUStandardUpdaterController alloc]
        initWithStartingUpdater:YES updaterDelegate:self userDriverDelegate:nil];
}

/* received directly before the update gets installed, so let's shut down a bit */
- (void)updater:(SPUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    [NSApp activateIgnoringOtherApps:YES];
    [self.playQueueController stopPlayback];
}

/* don't be enthusiastic about an update if we currently play a video */
- (BOOL)updater:(SPUUpdater *)updater mayPerformUpdateCheck:(SPUUpdateCheck)updateCheck error:(NSError * __autoreleasing *)error
{
    if ([self.playQueueController.playerController activeVideoPlayback]) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"org.videolan.vlc.Sparkle"
                                          code:1
                                      userInfo:@{NSLocalizedDescriptionKey: _NS("VLC is currently playing video.")}];
        }
        return NO;
    }

    return YES;
}

/* use the correct feed depending on the hardware architecture */
- (nullable NSString *)feedURLStringForUpdater:(SPUUpdater *)updater
{
#ifdef __x86_64__
    if (OSX_BIGSUR_AND_HIGHER) {
        if ([self processIsTranslated] > 0) {
            msg_Dbg(getIntf(), "Process is translated. On update, VLC will install the native ARM-64 binary.");
            return kARM64UpdateURLString;
        }
    }
    return kIntel64UpdateURLString;
#elif __arm64__
    return kARM64UpdateURLString;
#else
    #error unsupported architecture
#endif
}

- (void)updaterDidNotFindUpdate:(SPUUpdater *)updater error:(NSError *)error
{
    msg_Dbg(getIntf(), "No update found");
}

- (void)updater:(SPUUpdater *)updater failedToDownloadUpdate:(SUAppcastItem *)item error:(NSError *)error
{
    msg_Warn(getIntf(), "Failed to download update with error %li", error.code);
}

- (void)updater:(SPUUpdater *)updater didAbortWithError:(NSError *)error
{
    msg_Err(getIntf(), "Updater aborted with error %li", error.code);
}

@end

#endif
