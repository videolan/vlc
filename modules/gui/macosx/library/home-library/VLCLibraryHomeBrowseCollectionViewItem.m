/*****************************************************************************
 * VLCLibraryHomeBrowseCollectionViewItem.m: MacOS X interface module
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

#import "VLCLibraryHomeBrowseCollectionViewItem.h"

#import "library/home-library/VLCLibraryHomeViewBrowseCarouselItemView.h"

#import "views/VLCUIUnits.h"

@implementation VLCLibraryHomeBrowseCollectionViewItem

- (void)loadView
{
    self.view = [[VLCLibraryHomeViewBrowseCarouselItemView alloc] initWithFrame:(NSRect){NSZeroPoint, VLCUIUnits.libraryCompactCardSize}];
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.representedObject = nil;
    VLCLibraryHomeViewBrowseCarouselItemView * const card = (VLCLibraryHomeViewBrowseCarouselItemView *)self.view;
    card.mrl = @"";
    card.target = nil;
    card.action = NULL;
    card.carouselView = nil;
}

@end
