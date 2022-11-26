/*****************************************************************************
 * VLCLibraryAudioViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibraryAudioViewController.h"

#import "main/VLCMain.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryNavigationStack.h"
#import "library/VLCLibraryWindow.h"
#import "library/audio-library/VLCLibraryAudioDataSource.h"

@interface VLCLibraryAudioViewController()
{
    NSArray<NSString *> *_placeholderImageNames;
    NSArray<NSString *> *_placeholderLabelStrings;
}
@end

@implementation VLCLibraryAudioViewController

#pragma mark - Set up the view controller

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];

    if(self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupAudioDataSource];
        [self setupAudioCollectionView];
        [self setupAudioTableViews];
        [self setupAudioSegmentedControl];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow*)libraryWindow
{
    NSAssert(libraryWindow, @"Cannot setup audio view controller with invalid library window");

    _libraryTargetView = libraryWindow.libraryTargetView;
    _audioLibraryView = libraryWindow.audioLibraryView;
    _audioLibrarySplitView = libraryWindow.audioLibrarySplitView;
    _audioCollectionSelectionTableViewScrollView = libraryWindow.audioCollectionSelectionTableViewScrollView;
    _audioCollectionSelectionTableView = libraryWindow.audioCollectionSelectionTableView;
    _audioGroupSelectionTableViewScrollView = libraryWindow.audioGroupSelectionTableViewScrollView;
    _audioGroupSelectionTableView = libraryWindow.audioGroupSelectionTableView;
    _audioCollectionViewScrollView = libraryWindow.audioCollectionViewScrollView;
    _audioLibraryCollectionView = libraryWindow.audioLibraryCollectionView;
    _audioSegmentedControl = libraryWindow.audioSegmentedControl;
    _gridVsListSegmentedControl = libraryWindow.gridVsListSegmentedControl;
    _optionBarView = libraryWindow.optionBarView;
    _librarySortButton = libraryWindow.librarySortButton;
    _librarySearchField = libraryWindow.librarySearchField;
    _placeholderImageView = libraryWindow.placeholderImageView;
    _placeholderLabel = libraryWindow.placeholderLabel;
    _emptyLibraryView = libraryWindow.emptyLibraryView;
}

- (void)setupAudioDataSource
{
    _audioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    _audioDataSource.libraryModel = [VLCMain sharedInstance].libraryController.libraryModel;
    _audioDataSource.collectionSelectionTableView = _audioCollectionSelectionTableView;
    _audioDataSource.groupSelectionTableView = _audioGroupSelectionTableView;
    _audioDataSource.collectionView = _audioLibraryCollectionView;
    [_audioDataSource setup];

    _audioGroupDataSource = [[VLCLibraryGroupDataSource alloc] init];
    _audioDataSource.groupDataSource = _audioGroupDataSource;
}

- (void)setupAudioCollectionView
{
    _audioLibraryCollectionView.selectable = YES;
    _audioLibraryCollectionView.allowsMultipleSelection = NO;
    _audioLibraryCollectionView.allowsEmptySelection = YES;
}

- (void)setupAudioTableViews
{
    _audioCollectionSelectionTableView.dataSource = _audioDataSource;
    _audioCollectionSelectionTableView.delegate = _audioDataSource;

    _audioGroupSelectionTableView.dataSource = _audioGroupDataSource;
    _audioGroupSelectionTableView.delegate = _audioGroupDataSource;

    if(@available(macOS 11.0, *)) {
        _audioGroupSelectionTableView.style = NSTableViewStyleFullWidth;
    }
}

- (void)setupAudioPlaceholderView
{
    _audioPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];

    _placeholderImageNames = @[@"placeholder-group2", @"placeholder-music", @"placeholder-music", @"placeholder-music"];
    _placeholderLabelStrings = @[
        _NS("Your favorite artists will appear here.\nGo to the Browse section to add artists you love."),
        _NS("Your favorite albums will appear here.\nGo to the Browse section to add albums you love."),
        _NS("Your favorite tracks will appear here.\nGo to the Browse section to add tracks you love."),
        _NS("Your favorite genres will appear here.\nGo to the Browse section to add genres you love."),
    ];
}

- (void)setupAudioSegmentedControl
{
    _audioSegmentedControl.segmentCount = 4;
    [_audioSegmentedControl setLabel:_NS("Artists") forSegment:VLCAudioLibraryArtistsSegment];
    [_audioSegmentedControl setLabel:_NS("Albums") forSegment:VLCAudioLibraryAlbumsSegment];
    [_audioSegmentedControl setLabel:_NS("Songs") forSegment:VLCAudioLibrarySongsSegment];
    [_audioSegmentedControl setLabel:_NS("Genres") forSegment:VLCAudioLibraryGenresSegment];
    _audioSegmentedControl.selectedSegment = 0;
}

- (void)configureAudioSegmentedControl
{
    [_audioSegmentedControl setTarget:self];
    [_audioSegmentedControl setAction:@selector(segmentedControlAction:)];
}

#pragma mark - Show the audio view

- (void)presentAudioView
{
    for (NSView *subview in _libraryTargetView.subviews) {
        [subview removeFromSuperview];
    }

    if (_audioDataSource.libraryModel.numberOfAudioMedia == 0) { // empty library
        [self presentPlaceholderAudioView];
    } else {
        [self presentAudioLibraryView];
    }

    _librarySortButton.hidden = NO;
    _librarySearchField.enabled = YES;
    _optionBarView.hidden = NO;
    _audioSegmentedControl.hidden = NO;
}

- (void)presentPlaceholderAudioView
{
    for (NSLayoutConstraint *constraint in [VLCMain sharedInstance].libraryWindow.videoPlaceholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint *constraint in _audioPlaceholderImageViewSizeConstraints) {
        constraint.active = YES;
    }

    NSInteger selectedLibrarySegment = _audioSegmentedControl.selectedSegment;

    if(selectedLibrarySegment < _placeholderImageNames.count && selectedLibrarySegment >= 0) {
        _placeholderImageView.image = [NSImage imageNamed:_placeholderImageNames[selectedLibrarySegment]];
    }

    if(selectedLibrarySegment < _placeholderLabelStrings.count && selectedLibrarySegment >= 0) {
        _placeholderLabel.stringValue = _placeholderLabelStrings[selectedLibrarySegment];
    }

    _emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    [_libraryTargetView addSubview:_emptyLibraryView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_emptyLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
}

- (void)presentAudioLibraryView
{
    _audioLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    [_libraryTargetView addSubview:_audioLibraryView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_audioLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_audioLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_audioLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    if (self.gridVsListSegmentedControl.selectedSegment == VLCGridViewModeSegment) {
        _audioLibrarySplitView.hidden = YES;
        _audioCollectionViewScrollView.hidden = NO;
    } else {
        [self presentAudioTableView];
        _audioCollectionViewScrollView.hidden = YES;
    }

    [self configureAudioSegmentedControl];
    [self segmentedControlAction:VLCMain.sharedInstance.libraryWindow.navigationStack];
}

- (void)presentAudioTableView
{
    _audioLibrarySplitView.hidden = NO;
}

- (IBAction)segmentedControlAction:(id)sender
{
    if (_audioDataSource.libraryModel.listOfAudioMedia.count == 0) {
        return;
    }

    _audioDataSource.audioLibrarySegment = _audioSegmentedControl.selectedSegment;

    VLCLibraryNavigationStack *globalNavStack = VLCMain.sharedInstance.libraryWindow.navigationStack;
    if(sender != globalNavStack) {
        [globalNavStack appendCurrentLibraryState];
    }
}

- (void)reloadData
{
    [_audioDataSource reloadData];
}

@end
