/*****************************************************************************
 * VLCLibraryHomeBrowseCarouselView.m: MacOS X interface module
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

#import "VLCLibraryHomeBrowseCarouselView.h"

#import "library/home-library/VLCLibraryHomeViewBrowseCarouselItemView.h"

@implementation VLCLibraryHomeBrowseCarouselView

- (void)activateItemAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath == nil || indexPath.section != 0 || self.numberOfSections == 0 ||
        (NSUInteger)indexPath.item >= (NSUInteger)[self numberOfItemsInSection:0]) {
        return;
    }
    NSCollectionViewItem * const item = [self itemAtIndexPath:indexPath];
    [(VLCLibraryHomeViewBrowseCarouselItemView *)item.view performClick:self];
}

- (BOOL)requestContextMenuForItemAtIndexPath:(__unused NSIndexPath *)indexPath
                                     event:(__unused NSEvent *)event
{
    return NO;
}

@end
