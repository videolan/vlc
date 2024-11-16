/*****************************************************************************
 * VLCStatusNotifierView.m: MacOS X interface module
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

#import "VLCStatusNotifierView.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryModel.h"

@interface VLCStatusNotifierView ()

@property NSUInteger loadingCount;
@property BOOL permanentDiscoveryMessageActive;
@property NSMutableSet<NSString *> *longNotifications;
@property NSMutableArray<NSString *> *messages;

@property (readonly) NSString *loadingLibraryItemsMessage;
@property (readonly) NSString *libraryItemsLoadedMessage;

@end

@implementation VLCStatusNotifierView

- (void)awakeFromNib
{
    self.loadingCount = 0;
    self.permanentDiscoveryMessageActive = NO;
    self.longNotifications = NSMutableSet.set;
    self.messages = NSMutableArray.array;

    _loadingLibraryItemsMessage = _NS("Loading library items");
    _libraryItemsLoadedMessage = _NS("Library items loaded");

    self.label.stringValue = _NS("Idle");

    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
    [defaultCenter addObserver:self selector:@selector(updateStatus:) name:nil object:nil];
}

- (void)displayStartLoad
{
    if (self.loadingCount == 0) {
        [self.progressIndicator startAnimation:self];
    }
    self.loadingCount++;
}

- (void)displayFinishLoad
{
    self.loadingCount--;
    if (self.loadingCount == 0) {
        [self.progressIndicator stopAnimation:self];
    }
}

- (void)addMessage:(NSString *)message
{
    [self.messages addObject:message];
    self.label.stringValue = [self.messages componentsJoinedByString:@"\n"];
}

- (void)removeMessage:(NSString *)message
{
    [self.messages removeObject:message];
    if (self.messages.count == 0) {
        self.label.stringValue = _NS("Idle");
        return;
    }

    self.label.stringValue = [self.messages componentsJoinedByString:@"\n"];
}

- (void)updateStatus:(NSNotification *)notification
{
    NSString * const notificationName = notification.name;
    if (![notificationName hasPrefix:@"VLC"]) {
        return;
    }

    if ([notificationName isEqualToString:VLCLibraryModelDiscoveryStarted]) {
        [self presentTransientMessage:_NS("Discovering media")];
        self.permanentDiscoveryMessageActive = YES;
        [self displayStartLoad];
    } else if ([notificationName isEqualToString:VLCLibraryModelDiscoveryProgress]) {
        [self addMessage:_NS("Discovering media")];
    } else if ([notificationName isEqualToString:VLCLibraryModelDiscoveryCompleted]) {
        self.permanentDiscoveryMessageActive = NO;
        [self displayFinishLoad];
        [self presentTransientMessage:_NS("Media discovery completed")];
    } else if ([notificationName isEqualToString:VLCLibraryModelDiscoveryFailed]) {
        self.permanentDiscoveryMessageActive = NO;
        [self displayFinishLoad];
        [self presentTransientMessage:_NS("Media discovery failed")];
    } else if ([notificationName containsString:VLCLongNotificationNameStartSuffix] && ![self.longNotifications containsObject:notificationName]) {
        if (self.longNotifications.count == 0) {
            [self removeMessage:self.libraryItemsLoadedMessage];
            [self addMessage:self.loadingLibraryItemsMessage];
            [self displayStartLoad];
        }
        [self.longNotifications addObject:notificationName];
    } else if ([notificationName containsString:VLCLongNotificationNameFinishSuffix]) {
        NSString * const loadingNotification =
            [notificationName stringByReplacingOccurrencesOfString:VLCLongNotificationNameFinishSuffix withString:VLCLongNotificationNameStartSuffix];
        [self.longNotifications removeObject:loadingNotification];
        if (self.longNotifications.count == 0) {
            [self displayFinishLoad];
            [self removeMessage:self.loadingLibraryItemsMessage];
            [self presentTransientMessage:self.libraryItemsLoadedMessage];
        }
    }
}

- (void)presentTransientMessage:(NSString *)message
{
    [self addMessage:message];
    [self performSelector:@selector(removeMessage:) withObject:message afterDelay:2.0];
}

@end
