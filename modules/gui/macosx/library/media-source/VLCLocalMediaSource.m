/*****************************************************************************
 * VLCLocalMediaSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLocalMediaSource.h"

#import "library/VLCInputItem.h"

#import "VLCLocalMediaSourceNodeObservation.h"

@implementation VLCLocalMediaSource

- (nullable NSNumber *)childCountForInputNode:(VLCInputNode *)inputNode
                                        error:(NSError * _Nullable * _Nullable)error
{
    NSURL * const nodeURL = [NSURL URLWithString:inputNode.inputItem.MRL];
    if (!nodeURL.isFileURL) {
        return [super childCountForInputNode:inputNode error:error];
    }

    NSArray<NSURL *> * const children =
        [NSFileManager.defaultManager contentsOfDirectoryAtURL:nodeURL
                                    includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                                                       options:NSDirectoryEnumerationSkipsHiddenFiles |
                                                               NSDirectoryEnumerationSkipsSubdirectoryDescendants
                                                         error:error];
    if (children == nil) {
        return nil;
    }

    NSUInteger childCount = 0;
    for (NSURL * const childURL in children) {
        NSNumber *isDirectory = nil;
        [childURL getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        if (isDirectory.boolValue || input_item_Playable(childURL.absoluteString.UTF8String)) {
            ++childCount;
        }
    }
    return @(childCount);
}

- (nullable id<VLCMediaSourceNodeObservation>)observeInputNode:(VLCInputNode *)inputNode
                                                      onChange:(void (^)(VLCMediaSourceNodeChange change))changeHandler
{
    NSURL * const nodeURL = [NSURL URLWithString:inputNode.inputItem.MRL];
    if (!nodeURL.isFileURL) {
        return nil;
    }

    const __weak typeof(self) weakSelf = self;
    return [[VLCLocalMediaSourceNodeObservation alloc] initWithURL:nodeURL
                                                      eventHandler:^(dispatch_source_vnode_flags_t eventFlags) {
        if ((eventFlags & DISPATCH_VNODE_DELETE) != 0 ||
            (eventFlags & DISPATCH_VNODE_RENAME) != 0) {
            changeHandler(VLCMediaSourceNodeChangeInvalidated);
            return;
        }

        const typeof(self) strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        [strongSelf generateChildNodesForDirectoryNode:inputNode withUrl:nodeURL];
        changeHandler(VLCMediaSourceNodeChangeChildrenUpdated);
    }];
}

@end
