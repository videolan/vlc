/*****************************************************************************
 * VLCLibraryHomeViewController.m: MacOS X interface module
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

#import "VLCLibraryHomeViewController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/home-library/VLCLibraryHomeViewStackViewController.h"
#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"

#import "library/video-library/VLCLibraryVideoDataSource.h"
#import "library/video-library/VLCLibraryVideoTableViewDelegate.h"
#import "library/video-library/VLCLibraryVideoViewController.h"

#import "main/VLCMain.h"

#import "windows/video/VLCMainVideoViewController.h"

@interface VLCLibraryHomeViewController ()
{
    id<VLCMediaLibraryItemProtocol> _awaitingPresentingLibraryItem;
    NSMutableSet<NSString *> *_ongoingLongLoadingNotifications;
    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
}
@end

@implementation VLCLibraryHomeViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];

    if(self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupGridViewController];
        [self setupHomePlaceholderView];
        [self setupHomeLibraryViews];

        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        NSString *notificationNames[] =
        {
            VLCLibraryModelVideoMediaListReset,
            VLCLibraryModelAudioMediaListReset,
            VLCLibraryModelVideoMediaItemDeleted,
            VLCLibraryModelAudioMediaItemDeleted,
        };

        for (size_t i = 0; i < ARRAY_SIZE(notificationNames); ++i) @autoreleasepool
        {
            [notificationCenter addObserver:self
                                   selector:@selector(libraryModelUpdated:)
                                       name:notificationNames[i]
                                     object:nil];

            NSString *startedNotification = [notificationNames[i] stringByAppendingString:VLCLongNotificationNameStartSuffix];
            [notificationCenter addObserver:self
                                   selector:@selector(libraryModelLongLoadStarted:)
                                       name:startedNotification
                                     object:nil];

            NSString *finishedNotification = [notificationNames[i] stringByAppendingString:VLCLongNotificationNameFinishSuffix];
            [notificationCenter addObserver:self
                                   selector:@selector(libraryModelLongLoadFinished:)
                                       name:finishedNotification
                                     object:nil];
        }
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _homeLibraryView = libraryWindow.homeLibraryView;
    _homeLibraryStackViewScrollView = libraryWindow.homeLibraryStackViewScrollView;
    _homeLibraryStackView = libraryWindow.homeLibraryStackView;
}

- (void)setupGridViewController
{
    _stackViewController = [[VLCLibraryHomeViewStackViewController alloc] init];
    self.stackViewController.collectionsStackViewScrollView = _homeLibraryStackViewScrollView;
    self.stackViewController.collectionsStackView = _homeLibraryStackView;
}

- (void)setupHomePlaceholderView
{
    _internalPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:182.f],
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:114.f],
    ];
}

- (void)setupHomeLibraryViews
{
    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    self.homeLibraryStackViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.homeLibraryStackViewScrollView.contentInsets = defaultInsets;
    self.homeLibraryStackViewScrollView.scrollerInsets = scrollerInsets;
}

#pragma mark - Show the video library view

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _internalPlaceholderImageViewSizeConstraints;
}

- (void)updatePresentedView
{
    if (VLCMain.sharedInstance.libraryController.libraryModel.numberOfVideoMedia == 0) { // empty library
        [self presentPlaceholderHomeLibraryView];
    } else {
        [self presentHomeLibraryView];
    }
}

- (void)presentHomeView
{
    [self updatePresentedView];
}

- (void)presentPlaceholderHomeLibraryView
{
    [self.libraryWindow displayLibraryPlaceholderViewWithImage:[NSImage imageNamed:@"placeholder-video"]
                                              usingConstraints:self.placeholderImageViewSizeConstraints
                                             displayingMessage:_NS("Your media will appear here.\nGo to the Browse section to add media you love.")];
}

- (void)presentHomeLibraryView
{
    [self.libraryWindow displayLibraryView:self.homeLibraryView];
    self.homeLibraryStackViewScrollView.hidden = NO;
    [self.stackViewController reloadData];
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    NSParameterAssert(aNotification);
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    const NSUInteger videoCount = model.numberOfVideoMedia;
    const NSUInteger audioCount = model.numberOfAudioMedia;

    NSArray<NSView *> * const targetViewSubViews = self.libraryTargetView.subviews;
    const BOOL emptyLibraryViewPresent = [targetViewSubViews containsObject:self.emptyLibraryView];
    const BOOL homeLibraryViewPresent = [targetViewSubViews containsObject:self.homeLibraryView];
    if (self.libraryWindow.librarySegmentType == VLCLibraryHomeSegmentType &&
        ((videoCount == 0 && !emptyLibraryViewPresent) ||
         (videoCount > 0 && !homeLibraryViewPresent) ||
         (audioCount == 0 && !emptyLibraryViewPresent) ||
         (audioCount > 0 && !homeLibraryViewPresent)) &&
        !self.libraryWindow.embeddedVideoPlaybackActive) {

        [self updatePresentedView];
    }
}

- (void)libraryModelLongLoadStarted:(NSNotification *)notification
{
    if ([_ongoingLongLoadingNotifications containsObject:notification.name]) {
        return;
    }

    [_ongoingLongLoadingNotifications addObject:notification.name];
    if (self.connected) {
        [self.stackViewController disconnectContainers];
    }
    [self.libraryWindow showLoadingOverlay];
}

- (void)libraryModelLongLoadFinished:(NSNotification *)notification
{
    [_ongoingLongLoadingNotifications removeObject:notification.name];
    if (_ongoingLongLoadingNotifications.count > 0) {
        return;
    }
    if (self.connected) {
        [self.stackViewController connectContainers];
    }
    [self.libraryWindow hideLoadingOverlay];
}

- (void)connect
{
    [self.stackViewController connectContainers];
}

- (void)disconnect
{
    [self.stackViewController disconnectContainers];
}

@end
