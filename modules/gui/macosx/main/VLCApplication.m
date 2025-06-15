/*****************************************************************************
 * VLCApplication.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "VLCApplication.h"
#import "extensions/NSString+Helpers.h"

#import <vlc_configuration.h>
#import <vlc_interface.h>

/*****************************************************************************
 * VLCApplication implementation
 *****************************************************************************/

@interface VLCApplication ()
{
    NSURL *_appLocationURL;
    NSImage *_vlcAppIconImage;
    intf_thread_t *_intf;
}

@end

@implementation VLCApplication

- (instancetype)init
{
    self = [super init];
    if (self) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                                 selector:@selector(appBecameActive:)
                                                     name:NSApplicationDidBecomeActiveNotification
                                                   object:nil];
        /* we need to keep a file reference to the app's current location so we can find out where
         * it ends-up after being relocated or rename */
        _appLocationURL = [[[NSBundle mainBundle] bundleURL] fileReferenceURL];

        if (config_GetInt("macosx-icon-change")) {
            NSCalendar *const gregorian =
                [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
            const NSUInteger dayOfYear = [gregorian ordinalityOfUnit:NSCalendarUnitDay
                                                              inUnit:NSCalendarUnitYear
                                                             forDate:[NSDate date]];

            if (dayOfYear >= 354) {
                _winterHolidaysTheming = YES;
            }
        }

    }
    return self;
}

- (void)setIntf:(intf_thread_t*)intf
{
    _intf = intf;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (NSImage *)vlcAppIconImage
{
    NSAssert(NSThread.isMainThread, @"I must be called from the main thread only!");

    // If we already have an image, immediately return that
    if (_vlcAppIconImage != nil)
        return _vlcAppIconImage;

    if (self.winterHolidaysTheming) {
        /* After day 354 of the year, the usual VLC cone is replaced by another cone
         * wearing a Father Xmas hat.
         * Note: this icon doesn't represent an endorsement of The Coca-Cola Company.
         */
        _vlcAppIconImage = [NSImage imageNamed:@"VLC-Xmas"];
    }

    if (_vlcAppIconImage == nil)
        _vlcAppIconImage = [NSImage imageNamed:@"VLC"];

    return _vlcAppIconImage;
}

- (void)appBecameActive:(NSNotification *)aNotification
{
    if ([[[NSBundle mainBundle] bundleURL] checkResourceIsReachableAndReturnError:nil]) {
        return;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSAlertStyleCritical];
    [alert setMessageText:_NS("VLC has been moved or renamed")];
    [alert setInformativeText:_NS("To prevent errors, VLC must be relaunched.\n\nIf you cannot quit immediately, click Continue, then quit and relaunch as soon as possible to avoid problems.")];
    [alert addButtonWithTitle:_NS("Restart")];
    [alert addButtonWithTitle:_NS("Quit")];
    [alert addButtonWithTitle:_NS("Continue (Not Recommended)")];

    NSInteger alertButton = [alert runModal];

    if (alertButton == NSAlertThirdButtonReturn) {
        return;
    }

    if (alertButton == NSAlertFirstButtonReturn) {
        /* terminate and restart
         * NOTE you may not use [VLCMain relaunchApplication] here as it depends on the app not having moved and WILL crash */
        [NSWorkspace.sharedWorkspace launchApplicationAtURL:_appLocationURL.absoluteURL
                                                      options:NSWorkspaceLaunchNewInstance
                                                configuration:@{}
                                                        error:nil];
    }

    [self terminate:self];
}

// when user selects the quit menu from dock it sends a terminate:
// but we need to send a stop: to properly exits libvlc.
// However, we are not able to change the action-method sent by this standard menu item.
// thus we override terminate: to send a stop:
- (void)terminate:(id)sender
{
    [self activateIgnoringOtherApps:YES];
    libvlc_Quit(vlc_object_instance(_intf));
}

@end
