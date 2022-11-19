/*****************************************************************************
 * VLCLibraryVideoDataSource.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import <Cocoa/Cocoa.h>

typedef NS_ENUM(NSUInteger, VLCLibraryVideoGroup) {
    VLCLibraryVideoInvalidGroup = 0,
    VLCLibraryVideoRecentsGroup,
    VLCLibraryVideoLibraryGroup,
};

typedef NS_ENUM(NSUInteger, VLCLibraryVideoCollectionViewTableViewCellType) {
    VLCLibraryVideoCollectionViewTableViewCellNormalType = 0,
    VLCLibraryVideoCollectionViewTableViewCellHorizontalScrollType
};

@class VLCLibraryModel;

NS_ASSUME_NONNULL_BEGIN

/** Serves collection views for each of the video library sections **/
@interface VLCLibraryVideoCollectionViewsDataSource : NSObject <NSTableViewDataSource, NSTableViewDelegate>

@property (readwrite, assign) NSSize collectionViewItemSize;
@property (readwrite, assign) CGFloat collectionViewMinimumLineSpacing;
@property (readwrite, assign) CGFloat collectionViewMinimumInteritemSpacing;
@property (readwrite, assign) NSEdgeInsets collectionViewSectionInset;

@property (readwrite, assign, nonatomic) NSTableView *collectionsTableView;
@property (readwrite, assign) NSScrollView *collectionsTableViewScrollView;

- (void)reloadData;

@end

NS_ASSUME_NONNULL_END
