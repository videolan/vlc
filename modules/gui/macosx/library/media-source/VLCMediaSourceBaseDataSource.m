/*****************************************************************************
 * VLCMediaSourceBaseDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCMediaSourceBaseDataSource.h"

#import "VLCMediaSourceProvider.h"
#import "VLCMediaSource.h"
#import "VLCMediaSourceDeviceCollectionViewItem.h"
#import "VLCMediaSourceCollectionViewItem.h"
#import "VLCMediaSourceDataSource.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCInputNodePathControlItem.h"
#import "library/VLCLibraryNavigationStack.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"


NSString *VLCMediaSourceTableViewCellIdentifier = @"VLCMediaSourceTableViewCellIdentifier";

@interface VLCMediaSourceBaseDataSource () <NSCollectionViewDataSource, NSCollectionViewDelegate, NSTableViewDelegate, NSTableViewDataSource>
{
    NSArray<VLCMediaSource *> *_mediaSources;
    NSArray *_discoveredLANdevices;
    BOOL _gridViewMode;
}
@end

@implementation VLCMediaSourceBaseDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _mediaSources = @[];
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenReset:)
                                   name:VLCMediaSourceChildrenReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenAdded:)
                                   name:VLCMediaSourceChildrenAdded
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenRemoved:)
                                   name:VLCMediaSourceChildrenRemoved
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourcePreparingEnded:)
                                   name:VLCMediaSourcePreparsingEnded
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - view and model state management

- (void)setupViews
{
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    [self.collectionView registerClass:[VLCMediaSourceDeviceCollectionViewItem class] forItemWithIdentifier:VLCMediaSourceDeviceCellIdentifier];
    [self.collectionView registerClass:[VLCMediaSourceCollectionViewItem class] forItemWithIdentifier:VLCMediaSourceCellIdentifier];
    [self.collectionView registerClass:[VLCLibraryCollectionViewSupplementaryElementView class]
               forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                           withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];

    self.homeButton.action = @selector(homeButtonAction:);
    self.homeButton.target = self;
    [self.pathControl clearInputNodePathControlItems];
    self.pathControl.action = @selector(pathControlAction:);
    self.pathControl.target = self;

    _homeButton.hidden = YES;
    _pathControl.hidden = YES;

    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    self.tableView.hidden = YES;
    
    [self reloadViews];
}

- (void)reloadViews
{
    VLCLibraryViewModeSegment viewModeSegment = VLCLibraryGridViewModeSegment;
    VLCLibraryWindowPersistentPreferences * const libraryWindowPrefs = VLCLibraryWindowPersistentPreferences.sharedInstance;

    switch (_mediaSourceMode) {
        case VLCMediaSourceModeLAN:
            viewModeSegment = libraryWindowPrefs.browseLibraryViewMode;
            break;
        case VLCMediaSourceModeInternet:
            viewModeSegment = libraryWindowPrefs.streamLibraryViewMode;
            break;
        default:
            break;
    }

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        self.collectionViewScrollView.hidden = NO;
        self.tableView.hidden = YES;
        [self.collectionView reloadData];
    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        self.collectionViewScrollView.hidden = YES;
        self.tableView.hidden = NO;
        [self.tableView reloadData];
    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)loadMediaSources
{
    [self.pathControl clearInputNodePathControlItems];
    NSArray *mediaSources;
    if (self.mediaSourceMode == VLCMediaSourceModeLAN) {
        mediaSources = [VLCMediaSourceProvider listOfLocalMediaSources];
    } else {
        mediaSources = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_INTERNET];
    }
    NSUInteger count = mediaSources.count;
    if (count > 0) {
        for (NSUInteger x = 0; x < count; x++) {
            VLCMediaSource *mediaSource = mediaSources[x];
            VLCInputNode *rootNode = [mediaSource rootNode];
            [mediaSource preparseInputNodeWithinTree:rootNode];
        }
    }
    _mediaSources = mediaSources;
    [self.collectionView reloadData];
}

- (void)setMediaSourceMode:(VLCMediaSourceMode)mediaSourceMode
{
    _mediaSourceMode = mediaSourceMode;
    [self loadMediaSources];
    [self homeButtonAction:self];
}

#pragma mark - collection view data source

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        return _mediaSources.count;
    }

    return 1;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCMediaSource *mediaSource = _mediaSources[section];
        VLCInputNode *rootNode = mediaSource.rootNode;
        return rootNode.numberOfChildren;
    }

    return _mediaSources.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCMediaSourceDeviceCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceDeviceCellIdentifier forIndexPath:indexPath];
    VLCMediaSource *mediaSource = _mediaSources[indexPath.section];
    
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCInputNode *rootNode = mediaSource.rootNode;
        NSArray *nodeChildren = rootNode.children;
        VLCInputNode *childNode = nodeChildren[indexPath.item];
        VLCInputItem *childRootInput = childNode.inputItem;
        viewItem.titleTextField.stringValue = childRootInput.name;

        const enum input_item_type_e inputType = childRootInput.inputType;
        const BOOL isStream = childRootInput.isStream;
        
        NSURL *artworkURL = childRootInput.artworkURL;
        NSImage *placeholder = nil;
        
        if (mediaSource.category == SD_CAT_LAN) {
            placeholder = [NSImage imageNamed:@"bw-Music"];
        } else {
            switch (inputType) {
                case ITEM_TYPE_DIRECTORY:
                    if ([childRootInput.name containsString:@"home"]) {
                        placeholder = [NSImage imageNamed:@"bw-home"];
                    } else {
                        placeholder = indexPath.item % 2 ? [NSImage imageNamed:@"bw-Server1"] : [NSImage imageNamed:@"bw-server2"];
                    }
                    break;
                case ITEM_TYPE_DISC:
                    if (isStream) {
                        placeholder = indexPath.item % 2 ? [NSImage imageNamed:@"bw-Server1"] : [NSImage imageNamed:@"bw-server2"];
                    } else {
                        placeholder = indexPath.item % 2 ? [NSImage imageNamed:@"bw-usb1"] : [NSImage imageNamed:@"bw-usb2"];;
                    }
                    break;
                default:
                    placeholder = [NSImage imageNamed:@"bw-Music"];
                    break;
            }
        }
        
        if (artworkURL) {
            [viewItem.mediaImageView setImageURL:artworkURL placeholderImage:placeholder];
        } else {
            viewItem.mediaImageView.image = placeholder;
        }
    } else {
        VLCMediaSource *mediaSource = _mediaSources[indexPath.item];
        viewItem.titleTextField.stringValue = mediaSource.mediaSourceDescription;
        viewItem.mediaImageView.image = [NSImage imageNamed:@"bw-Music"];
    }

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }

    VLCMediaSource *mediaSource;
    VLCInputNode *childNode;

    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        mediaSource = _mediaSources[indexPath.section];
        VLCInputNode *rootNode = mediaSource.rootNode;
        NSArray *nodeChildren = rootNode.children;
        childNode = nodeChildren[indexPath.item];
    } else {
        mediaSource = _mediaSources[indexPath.item];
        childNode = mediaSource.rootNode;
    }

    [self configureChildDataSourceWithNode:childNode andMediaSource:mediaSource];
    [self reloadData];
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    NSAssert([kind compare:NSCollectionElementKindSectionHeader] == NSOrderedSame, @"View request for non-existing footer.");

    VLCLibraryCollectionViewSupplementaryElementView *labelView = [collectionView makeSupplementaryViewOfKind:kind
                                                                                               withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                                                                                 forIndexPath:indexPath];
    
    labelView.stringValue = _mediaSources[indexPath.section].mediaSourceDescription;

    return labelView;
}

- (CGSize)collectionView:(NSCollectionView *)collectionView
                  layout:(NSCollectionViewLayout *)collectionViewLayout
referenceSizeForHeaderInSection:(NSInteger)section
{
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCMediaSource *mediaSource = _mediaSources[section];
        VLCInputNode *rootNode = mediaSource.rootNode;
        // Hide Section if no children under the root node are found.
        return rootNode.numberOfChildren == 0 ? CGSizeZero : [VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize];
    }
    
    return [VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize];
}

- (NSSize)collectionView:(NSCollectionView *)collectionView
                  layout:(NSCollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionViewLayout;
    NSAssert(collectionViewLayout, @"This should be a flow layout and thus a valid pointer");
    return [VLCLibraryUIUnits adjustedCollectionViewItemSizeForCollectionView:collectionView
                                                                   withLayout:collectionViewFlowLayout
                                                         withItemsAspectRatio:VLCLibraryCollectionViewItemAspectRatioDefaultItem];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        /* for LAN, we don't show the root items but the top items, which may change any time through a callback
         * so we don't run into conflicts, we compile a list of the currently known here and propose that
         * as the truth to the table view. For collection view, we use sections which can be reloaded individually,
         * so the problem is well hidden and does not need this work-around */
        _discoveredLANdevices = nil;
        NSMutableArray *currentDevices;
        @synchronized (_mediaSources) {
            NSInteger mediaSourceCount = _mediaSources.count;
            currentDevices = [[NSMutableArray alloc] initWithCapacity:mediaSourceCount];
            for (NSUInteger x = 0; x < mediaSourceCount; x++) {
                VLCMediaSource *mediaSource = _mediaSources[x];
                VLCInputNode *rootNode = mediaSource.rootNode;
                [currentDevices addObjectsFromArray:rootNode.children];
            }
        }
        _discoveredLANdevices = [currentDevices copy];
        return _discoveredLANdevices.count;
    }

    return _mediaSources.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:VLCMediaSourceTableViewCellIdentifier owner:self];

    if (cellView == nil) {
        cellView = [VLCLibraryTableCellView fromNibWithOwner:self];
        cellView.identifier = VLCMediaSourceTableViewCellIdentifier;
    }
    cellView.primaryTitleTextField.hidden = YES;
    cellView.secondaryTitleTextField.hidden = YES;
    cellView.singlePrimaryTitleTextField.hidden = NO;

    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCInputNode *currentNode = _discoveredLANdevices[row];
        VLCInputItem *currentNodeInput = currentNode.inputItem;

        NSURL *artworkURL = currentNodeInput.artworkURL;
        NSImage *placeholder = [NSImage imageNamed:@"NXdefaultappicon"];
        if (artworkURL) {
            [cellView.representedImageView setImageURL:artworkURL placeholderImage:placeholder];
        } else {
            cellView.representedImageView.image = placeholder;
        }

        cellView.singlePrimaryTitleTextField.stringValue = currentNodeInput.name;
    } else {
        VLCMediaSource *mediaSource = _mediaSources[row];
        cellView.singlePrimaryTitleTextField.stringValue = mediaSource.mediaSourceDescription;
        cellView.representedImageView.image = [NSImage imageNamed:@"NXFollow"];
    }

    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger selectedRow = self.tableView.selectedRow;
    if (selectedRow < 0) {
        return;
    }

    VLCMediaSource *mediaSource = _mediaSources[selectedRow];;
    VLCInputNode *childNode;
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        childNode = _discoveredLANdevices[selectedRow];
    } else {
        childNode = mediaSource.rootNode;
    }
    [self configureChildDataSourceWithNode:childNode andMediaSource:mediaSource];

    [self reloadData];
}

#pragma mark - glue code

- (void)configureChildDataSourceWithNode:(VLCInputNode *)node andMediaSource:(VLCMediaSource *)mediaSource
{
    if (!node || !mediaSource) {
        NSLog(@"Received bad node or media source, could not configure child data media source");
        return;
    }

    [mediaSource preparseInputNodeWithinTree:node];
    
    VLCMediaSourceDataSource *newChildDataSource = [[VLCMediaSourceDataSource alloc] init];
    
    newChildDataSource.displayedMediaSource = mediaSource;
    newChildDataSource.nodeToDisplay = node;
    newChildDataSource.collectionView = self.collectionView;
    newChildDataSource.pathControl = self.pathControl;
    newChildDataSource.tableView = self.tableView;
    
    [self setChildDataSource:newChildDataSource];
    [[VLCMain sharedInstance].libraryWindow.navigationStack appendCurrentLibraryState];

    _homeButton.hidden = NO;
    _pathControl.hidden = NO;
}

- (void)setChildDataSource:(VLCMediaSourceDataSource *)childDataSource
{
    if (!childDataSource) {
        NSLog(@"Received bad childDataSource, returning home");
        [self returnHome];
        return;
    } else if (childDataSource == _childDataSource) {
        NSLog(@"Received same childDataSource");
        return;
    }
    
    _childDataSource = childDataSource;

    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCInputNode *node = childDataSource.nodeToDisplay;
        VLCInputNodePathControlItem *nodePathItem = [[VLCInputNodePathControlItem alloc] initWithInputNode:node];

        [self.pathControl appendInputNodePathControlItem:nodePathItem];
    }
    self.pathControl.hidden = NO;
    
    [_childDataSource setupViews];

    self.collectionView.dataSource = _childDataSource;
    self.collectionView.delegate = _childDataSource;

    self.tableView.dataSource = _childDataSource;
    self.tableView.delegate = _childDataSource;
}

#pragma mark - user interaction with generic buttons

- (void)returnHome
{
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;

    _childDataSource = nil;
    [self.pathControl clearInputNodePathControlItems];

    [self reloadData];

    _homeButton.hidden = YES;
    _pathControl.hidden = YES;
    
}

- (void)homeButtonAction:(id)sender
{
    [self returnHome];
    VLCLibraryNavigationStack *mainNavStack = [VLCMain sharedInstance].libraryWindow.navigationStack;
    [mainNavStack clear];
}

- (void)pathControlAction:(id)sender
{
    if (_pathControl.clickedPathItem == nil || _childDataSource == nil) {
        return;
    }

    NSPathControlItem *selectedItem = _pathControl.clickedPathItem;
    NSString *itemNodeMrl = selectedItem.image.name;

    VLCInputNodePathControlItem *matchingItem = [_pathControl.inputNodePathControlItems objectForKey:itemNodeMrl];
    if (matchingItem != nil) {
        _childDataSource.nodeToDisplay = matchingItem.inputNode;
        [_pathControl clearPathControlItemsAheadOf:selectedItem];
    }
}

#pragma mark - VLCMediaSource Delegation

- (void)mediaSourceChildrenReset:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Reset nodes: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)mediaSourceChildrenAdded:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Received new nodes: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)mediaSourceChildrenRemoved:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Removed nodes: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)mediaSourcePreparingEnded:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Preparsing ended: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)reloadDataForNotification:(NSNotification *)aNotification
{
    if (_gridViewMode) {
        if (self.collectionView.dataSource == self) {
            NSInteger index = [_mediaSources indexOfObject:aNotification.object];
            if (self.collectionView.numberOfSections > index) {
                [self.collectionView reloadSections:[NSIndexSet indexSetWithIndex:index]];
            } else {
                [self.collectionView reloadData];
            }
        } else {
            [self.collectionView reloadData];
        }
    } else {
        [self.tableView reloadData];
    }
}

- (void)reloadData
{
    if (_gridViewMode) {
        [self.collectionView reloadData];
    } else {
        [self.tableView reloadData];
    }
}

@end
