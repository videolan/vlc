/*****************************************************************************
 * VLCLibraryHomeViewStackViewControllerTest.m: Recently added Home section tests
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCAppKitMockingTestCase.h"

#import "extensions/NSView+VLCAdditions.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItemCollectionViewItem.h"
#import "library/home-library/VLCLibraryHomeViewActionsView.h"
#import "library/home-library/VLCLibraryHomeViewAudioCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewRecentlyAddedCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewStackViewController.h"
#import "library/home-library/VLCLibraryHomeViewVideoCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"
#import "library/home-library/VLCLibraryHomeViewVideoGridContainerView.h"
#import "main/VLCMain.h"

// Unrelated sections are lightweight views. The stack, Recently Added row,
// shared carousel, and media source remain production implementations.
@implementation VLCFileDragRecognisingView
+ (BOOL)handlePasteboardFromDragSessionAsPlayQueueItems:(NSPasteboard *)pasteboard { return NO; }
- (void)setupDragRecognition {}
@end

@implementation VLCLibraryHomeViewActionsView
+ (instancetype)fromNibWithOwner:(id)owner { return [[self alloc] initWithFrame:NSZeroRect]; }
- (BOOL)handlePasteBoardFromDragSession:(NSPasteboard *)pasteboard { return NO; }
- (IBAction)openFileAction:(id)sender {}
- (IBAction)openDiscAction:(id)sender {}
- (IBAction)openUrlAction:(id)sender {}
- (IBAction)openCaptureDeviceAction:(id)sender {}
@end

@implementation VLCLibraryHeroView
+ (instancetype)fromNibWithOwner:(id)owner { return [[self alloc] initWithFrame:NSZeroRect]; }
- (void)setOptimalRepresentedItem {}
- (IBAction)playRepresentedItem:(id)sender {}
@end

@implementation VLCLibraryHomeViewVideoCarouselContainerView
@synthesize groupDescriptor = _groupDescriptor;
@synthesize videoGroup = _videoGroup;
@synthesize dataSource = _dataSource;
- (void)setup {}
- (void)connect {}
- (void)disconnect {}
@end

@implementation VLCLibraryHomeViewAudioCarouselContainerView
- (void)setup {}
- (void)connect {}
- (void)disconnect {}
@end

@implementation VLCLibraryHomeViewVideoGridContainerView
@synthesize groupDescriptor = _groupDescriptor;
@synthesize videoGroup = _videoGroup;
@synthesize constraintsWithSuperview = _constraintsWithSuperview;
@synthesize dataSource = _dataSource;
- (void)connect {}
- (void)disconnect {}
- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)item {}
@end

@interface VLCHomeRecentlyAddedTestMediaItem : NSObject
@property int64_t libraryID;
@property vlc_ml_media_type_t mediaType;
@end

@implementation VLCHomeRecentlyAddedTestMediaItem
@end

static VLCMediaLibraryMediaItem *VLCHomeRecentlyAddedMediaItem(int64_t libraryID, vlc_ml_media_type_t type)
{
    VLCHomeRecentlyAddedTestMediaItem * const item = [[VLCHomeRecentlyAddedTestMediaItem alloc] init];
    item.libraryID = libraryID;
    item.mediaType = type;
    return (id)item;
}

@interface VLCHomeRecentlyAddedTestCard : NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem>
@property (strong, nonatomic) VLCLibraryRepresentedItem *representedItem;
@end

@implementation VLCHomeRecentlyAddedTestCard
- (void)loadView
{
    self.view = [[NSView alloc] initWithFrame:NSZeroRect];
}
@end

@interface VLCLibraryHomeViewStackViewControllerTest : VLCAppKitMockingTestCase
{
    VLCLibraryHomeViewStackViewController *_controller;
    NSStackView *_stackView;
}
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfRecentlyAddedVideoMedia;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfRecentlyAddedAudioMedia;
@property NSUInteger recentVideoCount;
@property NSUInteger recentAudioCount;
@end

@implementation VLCLibraryHomeViewStackViewControllerTest

- (void)setUp
{
    [super setUp];
    self.listOfRecentlyAddedVideoMedia = @[];
    self.listOfRecentlyAddedAudioMedia = @[];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance) onClass:VLCMain.class withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    [self replaceInstanceMethod:@selector(setup)
                        onClass:VLCLibraryHomeViewMediaContainerViewDataSource.class
                      withBlock:^(VLCLibraryHomeViewMediaContainerViewDataSource *source) {
        // Keep source snapshots and notifications real without loading card or detail nibs.
        [source.collectionView registerClass:VLCHomeRecentlyAddedTestCard.class
                       forItemWithIdentifier:source.collectionViewItemIdentifier];
        source.collectionView.dataSource = source;
    }];
    _controller = [[VLCLibraryHomeViewStackViewController alloc] init];
    XCTestExpectation * const videosLoaded = [self expectRecentlyAddedSnapshot:@[]
                                                                        inView:_controller.recentlyAddedVideosView];
    XCTestExpectation * const audioLoaded = [self expectRecentlyAddedSnapshot:@[]
                                                                       inView:_controller.recentlyAddedAudioView];
    _stackView = [[NSStackView alloc] initWithFrame:NSZeroRect];
    _controller.collectionsStackView = _stackView;
    [self waitForExpectations:@[
        videosLoaded,
        audioLoaded
    ]
                      timeout:5.];
    [self addTeardownBlock:^{
        VLCLibraryHomeViewStackViewControllerTest * const test = weakSelf;
        if (test != nil) {
            [test->_controller disconnectContainers];
            test->_controller.recentlyAddedVideosView.collectionView.dataSource = nil;
            test->_controller.recentlyAddedAudioView.collectionView.dataSource = nil;
            test->_controller = nil;
            test->_stackView = nil;
            [test drainMainQueue];
        }
    }];
}

- (id)libraryController { return self; }
- (id)libraryModel { return self; }
- (NSUInteger)numberOfRecentMedia { return self.recentVideoCount; }
- (NSUInteger)numberOfRecentAudioMedia { return self.recentAudioCount; }

- (XCTestExpectation *)expectRecentlyAddedSnapshot:(NSArray<VLCMediaLibraryMediaItem *> *)items
                                            inView:(VLCLibraryHomeViewRecentlyAddedCarouselContainerView *)row
{
    __block BOOL observed = NO;
    return [self expectationForNotification:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                     object:row.dataSource
                                    handler:^BOOL(NSNotification *notification) {
        VLCLibraryHomeViewMediaContainerViewDataSource * const source = notification.object;
        if (observed || [source collectionView:source.collectionView numberOfItemsInSection:0] != (NSInteger)items.count) {
            return NO;
        }
        for (NSUInteger index = 0; index < items.count; ++index) {
            if ([source libraryItemAtIndexPath:[NSIndexPath indexPathForItem:index inSection:0]
                             forCollectionView:source.collectionView] != items[index]) {
                return NO;
            }
        }
        observed = YES;
        return YES;
    }];
}

- (void)setRecentlyAddedItems:(NSArray<VLCMediaLibraryMediaItem *> *)items
                       inView:(VLCLibraryHomeViewRecentlyAddedCarouselContainerView *)row
{
    const BOOL videos = row == _controller.recentlyAddedVideosView;
    if (videos)
        self.listOfRecentlyAddedVideoMedia = items;
    else
        self.listOfRecentlyAddedAudioMedia = items;
    XCTestExpectation * const loaded = [self expectRecentlyAddedSnapshot:items inView:row];
    NSNotificationName const name = videos
        ? VLCLibraryModelRecentlyAddedVideoMediaListReset
        : VLCLibraryModelRecentlyAddedAudioMediaListReset;
    [NSNotificationCenter.defaultCenter postNotificationName:name object:self];
    [self waitForExpectations:@[loaded] timeout:5.];
}

- (void)assertRecentlyAddedRowsPrecedeBrowse
{
    const NSUInteger videosIndex = [_stackView.arrangedSubviews indexOfObject:_controller.recentlyAddedVideosView];
    const NSUInteger audioIndex = [_stackView.arrangedSubviews indexOfObject:_controller.recentlyAddedAudioView];
    const NSUInteger expectedIndex = 2 + (_controller.recentsView != nil) + (_controller.audioRecentsView != nil);
    XCTAssertEqual(videosIndex, expectedIndex);
    XCTAssertEqual(audioIndex, videosIndex + 1);
    XCTAssertEqual([_stackView.arrangedSubviews indexOfObject:_controller.browseView], audioIndex + 1);
    if (_controller.recentsView != nil) {
        XCTAssertEqual([_stackView.arrangedSubviews indexOfObject:_controller.recentsView], (NSUInteger)2);
    }
    if (_controller.audioRecentsView != nil) {
        XCTAssertEqual([_stackView.arrangedSubviews indexOfObject:_controller.audioRecentsView],
                       (NSUInteger)2 + (_controller.recentsView != nil));
    }
}

- (void)testTypedRecentlyAddedRowsKeepTheirOrderAsPlaybackRecentsAppearAndDisappear
{
    [self assertRecentlyAddedRowsPrecedeBrowse];
    self.recentAudioCount = 1;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentAudioMediaListReset object:self];
    [self assertRecentlyAddedRowsPrecedeBrowse];
    self.recentVideoCount = 1;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaListReset object:self];
    [self assertRecentlyAddedRowsPrecedeBrowse];
    self.recentAudioCount = 0;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentAudioMediaListReset object:self];
    [self assertRecentlyAddedRowsPrecedeBrowse];
    self.recentVideoCount = 0;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaListReset object:self];
    [self assertRecentlyAddedRowsPrecedeBrowse];
}

- (void)testTypedRecentlyAddedRowsUpdateAndHideIndependently
{
    VLCLibraryHomeViewRecentlyAddedCarouselContainerView * const videos = _controller.recentlyAddedVideosView;
    VLCLibraryHomeViewRecentlyAddedCarouselContainerView * const audio = _controller.recentlyAddedAudioView;
    XCTAssertTrue(videos.hidden);
    XCTAssertTrue(audio.hidden);
    for (VLCLibraryHomeViewRecentlyAddedCarouselContainerView *row in @[
        videos,
        audio
    ]) {
        XCTAssertFalse(row.headerView.navigable);
        XCTAssertTrue(row.headerView.libraryButton.hidden);
        XCTAssertFalse(row.headerView.libraryButton.enabled);
    }
    XCTAssertEqualObjects(videos.headerView.title, @"Recently added videos");
    XCTAssertEqualObjects(audio.headerView.title, @"Recently added music");

    VLCMediaLibraryMediaItem * const firstVideo = VLCHomeRecentlyAddedMediaItem(300, VLC_ML_MEDIA_TYPE_VIDEO);
    VLCMediaLibraryMediaItem * const secondVideo = VLCHomeRecentlyAddedMediaItem(400, VLC_ML_MEDIA_TYPE_VIDEO);
    [self setRecentlyAddedItems:@[
        firstVideo,
        secondVideo
    ]
                         inView:videos];
    XCTAssertFalse(videos.hidden);
    XCTAssertTrue(audio.hidden);
    XCTAssertEqual(videos.headerView.itemCount, (NSUInteger)2);

    VLCMediaLibraryMediaItem * const song = VLCHomeRecentlyAddedMediaItem(200, VLC_ML_MEDIA_TYPE_AUDIO);
    [self setRecentlyAddedItems:@[song] inView:audio];
    XCTAssertFalse(audio.hidden);
    XCTAssertEqual(audio.headerView.itemCount, (NSUInteger)1);
    XCTAssertEqual(videos.headerView.itemCount, (NSUInteger)2);
    XCTAssertEqual([audio.dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                          forCollectionView:audio.collectionView], song);

    VLCMediaLibraryMediaItem * const updatedVideo = VLCHomeRecentlyAddedMediaItem(300, VLC_ML_MEDIA_TYPE_VIDEO);
    [self setRecentlyAddedItems:@[updatedVideo] inView:videos];
    XCTAssertEqual(videos.headerView.itemCount, (NSUInteger)1);
    XCTAssertEqual([videos.dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                           forCollectionView:videos.collectionView], updatedVideo);
    XCTAssertEqual(audio.headerView.itemCount, (NSUInteger)1);

    [self setRecentlyAddedItems:@[] inView:videos];
    XCTAssertTrue(videos.hidden);
    XCTAssertFalse(audio.hidden);
    XCTAssertEqual(videos.headerView.itemCount, (NSUInteger)0);
    XCTAssertEqual(audio.headerView.itemCount, (NSUInteger)1);
    [self setRecentlyAddedItems:@[] inView:audio];
    XCTAssertTrue(audio.hidden);
    XCTAssertEqual(audio.headerView.itemCount, (NSUInteger)0);
    [self assertRecentlyAddedRowsPrecedeBrowse];
}

- (void)testReconnectRefreshesBothTypedRowsAfterHomeWasDisconnected
{
    VLCLibraryHomeViewRecentlyAddedCarouselContainerView * const videos = _controller.recentlyAddedVideosView;
    VLCLibraryHomeViewRecentlyAddedCarouselContainerView * const audio = _controller.recentlyAddedAudioView;
    VLCMediaLibraryMediaItem * const firstVideo = VLCHomeRecentlyAddedMediaItem(300, VLC_ML_MEDIA_TYPE_VIDEO);
    VLCMediaLibraryMediaItem * const firstSong = VLCHomeRecentlyAddedMediaItem(200, VLC_ML_MEDIA_TYPE_AUDIO);
    [self setRecentlyAddedItems:@[firstVideo] inView:videos];
    [self setRecentlyAddedItems:@[firstSong] inView:audio];
    [_controller disconnectContainers];
    VLCMediaLibraryMediaItem * const latestVideo = VLCHomeRecentlyAddedMediaItem(400, VLC_ML_MEDIA_TYPE_VIDEO);
    VLCMediaLibraryMediaItem * const latestSong = VLCHomeRecentlyAddedMediaItem(500, VLC_ML_MEDIA_TYPE_AUDIO);
    self.listOfRecentlyAddedVideoMedia = @[latestVideo];
    self.listOfRecentlyAddedAudioMedia = @[latestSong];
    XCTestExpectation * const videosDisconnected = [self expectRecentlyAddedSnapshot:@[latestVideo] inView:videos];
    XCTestExpectation * const audioDisconnected = [self expectRecentlyAddedSnapshot:@[latestSong] inView:audio];
    videosDisconnected.inverted = YES;
    audioDisconnected.inverted = YES;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedVideoMediaListReset object:self];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedAudioMediaListReset object:self];
    [self waitForExpectations:@[
        videosDisconnected,
        audioDisconnected
    ]
                      timeout:.2];
    XCTAssertEqual([videos.dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                           forCollectionView:videos.collectionView], firstVideo);
    XCTAssertEqual([audio.dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                          forCollectionView:audio.collectionView], firstSong);

    XCTestExpectation * const videosReconnected = [self expectRecentlyAddedSnapshot:@[latestVideo] inView:videos];
    XCTestExpectation * const audioReconnected = [self expectRecentlyAddedSnapshot:@[latestSong] inView:audio];
    [_controller connectContainers];
    [self waitForExpectations:@[
        videosReconnected,
        audioReconnected
    ]
                      timeout:5.];
    XCTAssertFalse(videos.hidden);
    XCTAssertFalse(audio.hidden);
    XCTAssertEqual(videos.headerView.itemCount, (NSUInteger)1);
    XCTAssertEqual(audio.headerView.itemCount, (NSUInteger)1);
    XCTAssertEqual([videos.dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                           forCollectionView:videos.collectionView], latestVideo);
    XCTAssertEqual([audio.dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                          forCollectionView:audio.collectionView], latestSong);
}

@end
