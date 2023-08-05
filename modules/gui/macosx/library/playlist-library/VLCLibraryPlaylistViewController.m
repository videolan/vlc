/*****************************************************************************
 * VLCLibraryPlaylistViewController.m: MacOS X interface module
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

#import "VLCLibraryPlaylistViewController.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

@interface VLCLibraryPlaylistViewController ()

@property (readonly) NSScrollView *collectionViewScrollView;
@property (readonly) VLCLibraryCollectionViewDelegate *collectionViewDelegate;

@end

@implementation VLCLibraryPlaylistViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];

    if(self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupPlaylistCollectionView];
        [self setupPlaylistPlaceholderView];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow*)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _libraryWindow = libraryWindow;
}

- (void)setupPlaylistCollectionView
{
    _collectionViewScrollView = [[NSScrollView alloc] initWithFrame:_libraryWindow.libraryTargetView.frame];
    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    _collectionView = [[NSCollectionView alloc] init];

    _collectionViewScrollView.documentView = _collectionView;
    _collectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    _collectionViewScrollView.scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionView.delegate = _collectionViewDelegate;
    _collectionView.collectionViewLayout = VLCLibraryCollectionViewFlowLayout.standardLayout;

    _collectionView.selectable = YES;
    _collectionView.allowsMultipleSelection = NO;
    _collectionView.allowsEmptySelection = YES;
}

- (void)setupPlaylistPlaceholderView
{
    _placeholderImageViewConstraints = @[
        [NSLayoutConstraint constraintWithItem:_libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:_libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];
}

@end
