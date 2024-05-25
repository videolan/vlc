/*****************************************************************************
 * VLCInputNodePathControlItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCInputNodePathControlItem.h"

#import "VLCInputItem.h"
#import "VLCLibraryImageCache.h"

#import "extensions/NSString+Helpers.h"

@implementation VLCInputNodePathControlItem

+ (NSString *)accessibilityDescriptionPrefix
{
    return _NS("Thumbnail for media location");
}

- (instancetype)initWithInputNode:(VLCInputNode *)inputNode
{
    self = [super init];
    if (self && inputNode != nil && inputNode.inputItem != nil) {
        _inputNode = inputNode;

        VLCInputItem * const inputItem = inputNode.inputItem;
        self.title = inputItem.name;

        NSImage * const folderImage = [NSImage imageNamed:NSImageNameFolder];
        self.image = folderImage.copy;
        // HACK: We have no way when we get the clicked item from the path control
        // of knowing specifically which input node this path item corresponds to,
        // as the path control returns a copy for clickedPathItem that is not of
        // this class. As a very awkward workaround, lets set the accessibility
        // description of the image and we will use this as an identifier.
        self.image.accessibilityDescription = [NSString stringWithFormat:@"%@: %@", 
                                               VLCInputNodePathControlItem.accessibilityDescriptionPrefix, 
                                               inputItem.path];
    } else if (inputNode == nil) {
        NSLog(@"WARNING: Received nil input node, cannot create VLCInputNodePathControlItem");
    } else if (inputNode.inputItem == nil) {
        NSLog(@"WARNING: Received nil input node's input item, cannot create VLCInputNodePathControlItem");
    }
    return self;
}

@end
