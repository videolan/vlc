/*****************************************************************************
 * NSAnimationContext+VLCAdditions.m MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Joseane Silva
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

#import "NSAnimationContext+VLCAdditions.h"

@implementation NSAnimationContext (VLCAdditions)

+ (void)runAnimationRespectingPreferences:(NSNumber *)duration
                                  changes:(void (^)(NSAnimationContext *))changes
                        completionHandler:(nullable void (^)())completionHandler
{
    const BOOL reduceMotion = NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *const context){
        if (duration != nil) {
            context.duration = reduceMotion ? 0 : duration.doubleValue;
        }
        changes(context);
    } completionHandler:completionHandler];
}

+ (void)runAnimationRespectingPreferencesWithDuration:(NSTimeInterval)duration
                                              changes:(void (^)(NSAnimationContext *))changes
                                    completionHandler:(nullable void (^)())completionHandler
{
    [self runAnimationRespectingPreferences:@(duration)
                                    changes:changes
                          completionHandler:completionHandler];
}

+ (void)runAnimationRespectingPreferencesWithChanges:(void (^)(NSAnimationContext *))changes
                                   completionHandler:(nullable void (^)())completionHandler
{
    [self runAnimationRespectingPreferences:nil
                                    changes:changes
                          completionHandler:completionHandler];
}

@end