/*****************************************************************************
 * VLCFileDragRecognisingView.m
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCFileDragRecognisingView.h"

@implementation VLCFileDragRecognisingView

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setupDragRecognition];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setupDragRecognition];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setupDragRecognition];
    }
    return self;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
}

- (void)awakeFromNib
{
    [self setupDragRecognition];
}

- (void)setupDragRecognition
{
    [self registerForDraggedTypes:@[NSFilenamesPboardType]];
}

@end
