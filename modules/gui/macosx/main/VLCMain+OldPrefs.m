/*****************************************************************************
 * intf-prefs.m
 *****************************************************************************
 * Copyright (C) 2001-2019 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCMain+OldPrefs.h"

#include <unistd.h> /* execl() */

#import <vlc_interface.h>

#import "extensions/NSString+Helpers.h"
#import "preferences/VLCSimplePrefsController.h"

@implementation VLCMain(OldPrefs)

static NSString * kVLCPreferencesVersion = @"VLCPreferencesVersion";
static const int kCurrentPreferencesVersion = 4;

+ (void)initialize
{
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCurrentPreferencesVersion]
                                                            forKey:kVLCPreferencesVersion];

    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
}

- (void)resetAndReinitializeUserDefaults
{
    NSUserDefaults *standardUserDefaults = [NSUserDefaults standardUserDefaults];
    // note that [NSUserDefaults resetStandardUserDefaults] will NOT correctly reset to the defaults

    NSString *appDomain = [[NSBundle mainBundle] bundleIdentifier];
    [standardUserDefaults removePersistentDomainForName:appDomain];

    // set correct version to avoid question about outdated config
    [standardUserDefaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

- (void)migrateOldPreferences
{
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    NSInteger version = [defaults integerForKey:kVLCPreferencesVersion];

    // This was set in user defaults in VLC 2.0.x (preferences version 2), overriding any
    // value in the Info.plist. Make sure to delete it here, always,
    // as it could be set if an old version of VLC is launched again.
    [defaults removeObjectForKey:@"SUFeedURL"];

    /*
     * Store version explicitely in file, for ease of debugging.
     * Otherwise, the value will be just defined at app startup,
     * as initialized above.
     */
    [defaults setInteger:version forKey:kVLCPreferencesVersion];
    if (version >= kCurrentPreferencesVersion)
        return;

    if (version == 1) {
        [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);

        if (!fixIntfSettings())
            return;
        else
            config_SaveConfigFile(getIntf()); // we need to do manually, since we won't quit libvlc cleanly
    } else if (version == 2) {
        /* version 2 (used by VLC 2.0.x and early versions of 2.1) can lead to exceptions within 2.1 or later
         * so we reset the OS X specific prefs here - in practice, no user will notice */
        [self resetAndReinitializeUserDefaults];

    } else if (version == 3) {
        /* version 4 (introduced in 3.0.0) adds RTL settings depending on stored language */
        [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [VLCSimplePrefsController updateRightToLeftSettings];
        CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);

        // In VLC 2.2.x, config for filters was fully controlled by audio and video effects panel.
        // In VLC 3.0, this is no longer the case and VLCs config is not touched anymore. Therefore,
        // disable filter in VLCs config in this transition.

        config_PutPsz("audio-filter", "");
        config_PutPsz("video-filter", "");
        config_SaveConfigFile(getIntf());
    } else {
        NSArray *libraries = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
                                                                 NSUserDomainMask, YES);
        if (!libraries || [libraries count] == 0)
            return;
        NSString *preferences = [[libraries firstObject] stringByAppendingPathComponent:@"Preferences"];

        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSAlertStyleInformational];
        [alert setMessageText:_NS("Remove old preferences?")];
        [alert setInformativeText:_NS("We just found an older version of VLC's preferences files.")];
        [alert addButtonWithTitle:_NS("Move To Trash and Relaunch VLC")];
        [alert addButtonWithTitle:_NS("Ignore")];
        NSModalResponse res = [alert runModal];
        if (res != NSAlertFirstButtonReturn) {
            [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
            return;
        }

        // Do NOT add the current plist file here as this would conflict with caching.
        // Instead, just reset below.
        NSArray *ourPreferences = @[[[NSURL alloc] initFileURLWithPath:[preferences stringByAppendingPathComponent:@"org.videolan.vlc"]],
                                    [[NSURL alloc] initFileURLWithPath:[preferences stringByAppendingPathComponent:@"VLC"]]];

        [[NSWorkspace sharedWorkspace] recycleURLs:ourPreferences completionHandler:^(NSDictionary *newURLs, NSError *error){
            [self resetAndReinitializeUserDefaults];
            [VLCMain relaunchApplication];
        }];
        return;
    }

    [VLCMain relaunchApplication];
}

- (void)resetPreferences
{
    /* reset VLC's config */
    config_ResetAll();

    /* force config file creation, since libvlc won't exit normally */
    config_SaveConfigFile(getIntf());

    /* reset OS X defaults */
    [self resetAndReinitializeUserDefaults];

    [VLCMain relaunchApplication];
}

@end
