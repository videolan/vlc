/*****************************************************************************
 * VLCLibraryModelChangeDelegate.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryModelChangeDelegate.h"

NSString * const VLCTimerNotificationNameUserInfoKey = @"notificationName";
NSString * const VLCTimerNotificationObjectUserInfoKey = @"notificationObject";
NSString * const VLCLongNotificationNameStartSuffix = @"Loading";
NSString * const VLCLongNotificationNameFinishSuffix = @"Finished";

@interface VLCLibraryModelChangeDelegate ()

@property (readonly) NSNotificationCenter *notificationCenter;
@property (readonly) NSMutableDictionary<NSString *, NSTimer*> *recentNotifications;
@property (readonly) NSMutableSet<NSString *> *longNotifications;

@end

@implementation VLCLibraryModelChangeDelegate

- (instancetype)initWithLibraryModel:(VLCLibraryModel *)model
{
    self = [super init];
    if (self) {
        _model = model;
        _notificationCenter = NSNotificationCenter.defaultCenter;
        _throttleInterval = 0.5;
        _recentNotifications = NSMutableDictionary.dictionary;
        _longNotifications = NSMutableSet.set;
    }
    return self;
}

- (void)notifyChange:(NSString *)notificationName withObject:(nonnull id)object
{
    // If the object received is not the model then we are dealing with a notification carrying
    // a specific object (i.e. a media item) that is updated or deleted, do not throttle
    if (object != self.model) {
        [self.notificationCenter postNotificationName:notificationName object:object];
        return;
    }

    NSTimer * const existingTimer = [self.recentNotifications objectForKey:notificationName];
    if (existingTimer != nil) {
        [existingTimer invalidate];
        NSString * const loadingNotification = [notificationName stringByAppendingString:VLCLongNotificationNameStartSuffix];
        [self.notificationCenter postNotificationName:loadingNotification object:object];
        [self.longNotifications addObject:notificationName];
    }

    NSDictionary<NSString *, NSString *> * const userInfo = @{
        VLCTimerNotificationNameUserInfoKey: notificationName,
        VLCTimerNotificationObjectUserInfoKey: object,
    };
    NSTimer * const throttleTimer = [NSTimer scheduledTimerWithTimeInterval:self.throttleInterval
                                                                     target:self
                                                                   selector:@selector(throttleTimerFired:)
                                                                   userInfo:userInfo
                                                                    repeats:NO];
    [self.recentNotifications setObject:throttleTimer forKey:notificationName];
}

- (void)throttleTimerFired:(NSTimer *)timer
{
    if (timer == nil) {
        return;
    }

    NSDictionary * const userInfo = timer.userInfo;
    NSString * const notificationName = [userInfo objectForKey:VLCTimerNotificationNameUserInfoKey];
    const id notificationObject = [userInfo objectForKey:VLCTimerNotificationObjectUserInfoKey];
    [self.recentNotifications removeObjectForKey:notificationName];
    [self.notificationCenter postNotificationName:notificationName object:notificationObject];

    if ([self.longNotifications containsObject:notificationName]) {
        NSString * const finishedName = [notificationName stringByAppendingString:VLCLongNotificationNameFinishSuffix];
        [self.notificationCenter postNotificationName:finishedName object:notificationObject];
    }
}

@end
