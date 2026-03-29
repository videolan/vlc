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
#include <AppKit/AppKit.h>
#import "extensions/NSImage+VLCAdditions.h"

#import "VLCLibraryMediaSourceViewNavigationStack.h"
#import "VLCMediaSourceProvider.h"
#import "VLCMediaSource.h"
#import "VLCMediaSourceDeviceCollectionViewItem.h"
#import "VLCMediaSourceCollectionViewItem.h"
#import "VLCMediaSourceDataSource.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSTableCellView+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCInputNodePathControlItem.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"

NSString * const VLCMediaSourceBaseDataSourceNodeChanged = @"VLCMediaSourceBaseDataSourceNodeChanged";

@interface VLCLANDeviceRecord : NSObject
@property (readonly) VLCMediaSource *mediaSource;
@property (readonly) VLCInputNode *inputNode;

- (instancetype)initWithMediaSource:(VLCMediaSource *)mediaSource
                          inputNode:(VLCInputNode *)inputNode;
@end

@implementation VLCLANDeviceRecord

- (instancetype)initWithMediaSource:(VLCMediaSource *)mediaSource
                          inputNode:(VLCInputNode *)inputNode
{
    self = [super init];
    if (self) {
        _mediaSource = mediaSource;
        _inputNode = inputNode;
    }
    return self;
}

@end

@interface VLCMediaSourceBaseDataSource () <NSCollectionViewDataSource, NSCollectionViewDelegate, NSTableViewDelegate, NSTableViewDataSource>
{
    NSArray<VLCMediaSource *> *_mediaSources;
    NSArray<VLCLANDeviceRecord *> *_lanDeviceSnapshot;
}
@end

@implementation VLCMediaSourceBaseDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _mediaSources = @[];
        _lanDeviceSnapshot = @[];
        _mediaSourceMode = VLCMediaSourceModeLAN;
        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
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
        [self loadMediaSources];
        [self returnHome];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

#pragma mark - view and model state management

- (VLCLibraryViewModeSegment)viewMode
{
    VLCLibraryWindowPersistentPreferences * const libraryWindowPrefs =
        VLCLibraryWindowPersistentPreferences.sharedInstance;

    switch (_mediaSourceMode) {
        case VLCMediaSourceModeLAN:
            return libraryWindowPrefs.browseLibraryViewMode;
        case VLCMediaSourceModeInternet:
            return libraryWindowPrefs.streamLibraryViewMode;
        default:
            return VLCLibraryGridViewModeSegment;
    }
}

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

    [self togglePathControlVisibility:NO];

    self.tableView.dataSource = self;
    self.tableView.delegate = self;

    NSNib * const tableCellViewNib = [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class) bundle:nil];
    [self.tableView registerNib:tableCellViewNib forIdentifier:VLCLibraryTableCellViewIdentifier];

    [self reloadViews];
}

- (void)reloadViews
{
    const VLCLibraryViewModeSegment viewModeSegment = self.viewMode;
    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        self.collectionViewScrollView.hidden = NO;
        self.tableViewScrollView.hidden = YES;
        [self.collectionView reloadData];
    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        self.collectionViewScrollView.hidden = YES;
        self.tableViewScrollView.hidden = NO;
        [self.tableView reloadData];
    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
    [self togglePathControlVisibility:self.childDataSource != nil];
}

- (void)loadMediaSources
{
    [self.pathControl clearInputNodePathControlItems];

    NSArray *mediaSources;
    if (self.mediaSourceMode == VLCMediaSourceModeLAN) {
        mediaSources = VLCMediaSourceProvider.listOfLocalMediaSources;
    } else {
        mediaSources = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_INTERNET];
    }
    NSAssert(mediaSources != nil, @"Media sources array should not be nil");

    for (VLCMediaSource * const mediaSource in mediaSources) {
        VLCInputNode * const rootNode = [mediaSource rootNode];
        if (rootNode == nil)
            continue;
        NSError * const error = [mediaSource preparseInputNodeWithinTree:rootNode];
        if (error == nil)
            [self.navigationStack installHandlersOnMediaSource:mediaSource];
    }

    _mediaSources = mediaSources;
    _lanDeviceSnapshot = self.mediaSourceMode == VLCMediaSourceModeLAN ? [self buildLANDeviceSnapshot] : @[];
    [self reloadData];
}

- (void)setMediaSourceMode:(VLCMediaSourceMode)mediaSourceMode
{
    if (mediaSourceMode == self.mediaSourceMode) {
        return;
    }
    _mediaSourceMode = mediaSourceMode;
    [self loadMediaSources];
    [self returnHome];
}

- (BOOL)hasDisplayedItems
{
    if (_childDataSource != nil) {
        return _childDataSource.nodeToDisplay.numberOfChildren > 0;
    }

    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        return _lanDeviceSnapshot.count > 0;
    }

    return _mediaSources.count > 0;
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
        VLCMediaSource * const mediaSource = _mediaSources[section];
        VLCInputNode * const rootNode = mediaSource.rootNode;
        return rootNode.numberOfChildren;
    }

    return _mediaSources.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCMediaSourceDeviceCollectionViewItem * const viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceDeviceCellIdentifier forIndexPath:indexPath];
    VLCMediaSource * const mediaSource = _mediaSources[indexPath.section];
    
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCInputNode * const rootNode = mediaSource.rootNode;
        NSArray * const nodeChildren = rootNode.children;
        if (nodeChildren == nil) {
            NSLog(@"No children found for media source root node: %@ cannot provide viewItem correctly", rootNode);
            return viewItem;
        }
        VLCInputNode * const childNode = nodeChildren[indexPath.item];
        VLCInputItem * const childRootInput = childNode.inputItem;
        viewItem.titleTextField.stringValue = childRootInput.name;

        const enum input_item_type_e inputType = childRootInput.inputType;
        const BOOL isStream = childRootInput.isStream;
        
        NSURL * const artworkURL = childRootInput.artworkURL;

        NSImage *placeholder;
        if (mediaSource.category == SD_CAT_LAN) {
            placeholder = NSImage.VLCBWMusicImage;
        } else {
            switch (inputType) {
                case ITEM_TYPE_DIRECTORY:
                    if ([childRootInput.name containsString:@"home"]) {
                        placeholder = NSImage.VLCBWHomeImage;
                    } else {
                        placeholder = indexPath.item % 2 ? NSImage.VLCBWServer1Image : NSImage.VLCBWServer2Image;
                    }
                    break;
                case ITEM_TYPE_DISC:
                    if (isStream) {
                        placeholder = indexPath.item % 2 ? NSImage.VLCBWServer1Image : NSImage.VLCBWServer2Image;
                    } else {
                        placeholder = indexPath.item % 2 ? NSImage.VLCBWUsb1Image : NSImage.VLCBWUsb2Image;;
                    }
                    break;
                default:
                    placeholder = NSImage.VLCBWMusicImage;
                    break;
            }
        }
        NSAssert(placeholder != nil, @"Placeholder image should not be nil");
        
        if (artworkURL) {
            [viewItem.mediaImageView setImageURL:artworkURL placeholderImage:placeholder];
        } else {
            viewItem.mediaImageView.image = placeholder;
        }
    } else {
        VLCMediaSource * const mediaSource = _mediaSources[indexPath.item];
        viewItem.titleTextField.stringValue = mediaSource.mediaSourceDescription;
        viewItem.mediaImageView.image = NSImage.VLCBWMusicImage;
    }

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath * const indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }

    VLCMediaSource *mediaSource;
    VLCInputNode *childNode;

    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        mediaSource = _mediaSources[indexPath.section];
        VLCInputNode * const rootNode = mediaSource.rootNode;
        NSArray * const nodeChildren = rootNode.children;
        if (nodeChildren == nil) {
            NSLog(@"No children found for media source root node: %@ cannot access item correctly", rootNode);
            return;
        }
        childNode = nodeChildren[indexPath.item];
    } else {
        mediaSource = _mediaSources[indexPath.item];
        childNode = mediaSource.rootNode;
    }

    NSAssert(mediaSource != nil, @"Media source should not be nil");
    NSAssert(childNode != nil, @"Child node should not be nil");

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
        VLCMediaSource * const mediaSource = _mediaSources[section];
        VLCInputNode * const rootNode = mediaSource.rootNode;
        // Hide Section if no children under the root node are found.
        return rootNode.numberOfChildren == 0 ? CGSizeZero : VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;
    }
    
    return VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;
}

- (NSSize)collectionView:(NSCollectionView *)collectionView
                  layout:(NSCollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewFlowLayout * const collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionViewLayout;
    NSAssert(collectionViewLayout, @"This should be a flow layout and thus a valid pointer");
    return [VLCLibraryUIUnits adjustedCollectionViewItemSizeForCollectionView:collectionView
                                                                   withLayout:collectionViewFlowLayout
                                                         withItemsAspectRatio:VLCLibraryCollectionViewItemAspectRatioDefaultItem];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        return _lanDeviceSnapshot.count;
    }

    return _mediaSources.count;
}

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    if ([tableColumn.identifier isEqualToString:@"VLCMediaSourceTableNameColumn"]) {
        VLCLibraryTableCellView * const cellView =
            [tableView makeViewWithIdentifier:VLCLibraryTableCellViewIdentifier owner:self];

        if (_mediaSourceMode == VLCMediaSourceModeLAN) {
            VLCInputItem * const currentNodeInput = _lanDeviceSnapshot[row].inputNode.inputItem;
            NSURL * const artworkURL = currentNodeInput.artworkURL;
            NSImage * const placeholder = NSImage.VLCDefaultAppIconImage;
            if (artworkURL) {
                [cellView.representedImageView setImageURL:artworkURL placeholderImage:placeholder];
            } else {
                cellView.representedImageView.image = placeholder;
            }
        } else {
            cellView.representedImageView.image = NSImage.VLCFollowImage;
        }

        NSString * const name = _mediaSourceMode == VLCMediaSourceModeLAN
            ? _lanDeviceSnapshot[row].inputNode.inputItem.name
            : _mediaSources[row].mediaSourceDescription;

        cellView.primaryTitleTextField.hidden = YES;
        cellView.secondaryTitleTextField.hidden = YES;
        cellView.singlePrimaryTitleTextField.hidden = NO;
        cellView.singlePrimaryTitleTextField.stringValue = name;
        return cellView;
    } else if ([tableColumn.identifier isEqualToString:@"VLCMediaSourceTableKindColumn"]) {
        static NSString * const basicCellViewIdentifier = @"BasicTableCellViewIdentifier";
        NSTableCellView *cellView =
            [tableView makeViewWithIdentifier:basicCellViewIdentifier owner:self];
        if (cellView == nil) {
            cellView = [NSTableCellView tableCellViewWithIdentifier:basicCellViewIdentifier
                                                      showingString:@""];
        }
        NSAssert(cellView, @"Cell view should not be nil");

        if (_mediaSourceMode == VLCMediaSourceModeLAN) {
            VLCInputItem * const currentNodeInput = _lanDeviceSnapshot[row].inputNode.inputItem;
            if (currentNodeInput.inputType == ITEM_TYPE_DIRECTORY) {
                cellView.textField.stringValue = _NS("Directory");
            }
        } else {
            VLCMediaSource * const mediaSource = _mediaSources[row];
            switch(mediaSource.category) {
                case SD_CAT_DEVICES:
                    cellView.textField.stringValue = _NS("Devices");
                    break;
                case SD_CAT_LAN:
                    cellView.textField.stringValue = _NS("LAN");
                    break;
                case SD_CAT_INTERNET:
                    cellView.textField.stringValue = _NS("Internet");
                    break;
                case SD_CAT_MYCOMPUTER:
                    cellView.textField.stringValue = _NS("My Computer");
                    break;
            }
        }
        return cellView;
    }
    return nil;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    const NSInteger selectedRow = self.tableView.selectedRow;
    if (selectedRow < 0) {
        return;
    }

    VLCMediaSource *mediaSource = nil;
    VLCInputNode *childNode = nil;
    if (_mediaSourceMode == VLCMediaSourceModeLAN) {
        VLCLANDeviceRecord * const record = _lanDeviceSnapshot[selectedRow];
        mediaSource = record.mediaSource;
        childNode = record.inputNode;
    } else {
        mediaSource = _mediaSources[selectedRow];
        childNode = mediaSource.rootNode;
    }
    NSAssert(mediaSource != nil, @"Media source should not be nil");
    NSAssert(childNode != nil, @"Child node should not be nil");

    [self configureChildDataSourceWithNode:childNode andMediaSource:mediaSource];
    [self reloadData];
}

#pragma mark - LAN device snapshot

- (NSArray<VLCLANDeviceRecord *> *)buildLANDeviceSnapshot
{
    NSMutableArray<VLCLANDeviceRecord *> *records;
    @synchronized (_mediaSources) {
        records = [[NSMutableArray alloc] initWithCapacity:_mediaSources.count];
        for (VLCMediaSource * const mediaSource in _mediaSources) {
            VLCInputNode * const rootNode = mediaSource.rootNode;
            for (VLCInputNode * const child in rootNode.children) {
                VLCLANDeviceRecord * const record =
                    [[VLCLANDeviceRecord alloc] initWithMediaSource:mediaSource
                                                          inputNode:child];
                [records addObject:record];
            }
        }
    }
    return [records copy];
}

#pragma mark - glue code

- (void)configureChildDataSourceWithNode:(VLCInputNode *)node andMediaSource:(VLCMediaSource *)mediaSource
{
    if (!node || !mediaSource) {
        NSLog(@"Received bad node or media source, could not configure child data media source");
        return;
    }

    NSError * const error = [mediaSource preparseInputNodeWithinTree:node];
    if (error) {
        NSAlert * const alert = [NSAlert alertWithError:error];
        alert.alertStyle = NSAlertStyleCritical;
        [alert runModal];
        return;
    }
    
    VLCMediaSourceDataSource * const newChildDataSource =
        [[VLCMediaSourceDataSource alloc] initWithParentBaseDataSource:self];
    
    newChildDataSource.displayedMediaSource = mediaSource;
    newChildDataSource.nodeToDisplay = node;
    newChildDataSource.collectionView = self.collectionView;
    newChildDataSource.pathControl = self.pathControl;
    newChildDataSource.tableView = self.tableView;
    newChildDataSource.navigationStack = self.navigationStack;

    [self setChildDataSource:newChildDataSource];
    [self.navigationStack appendCurrentLibraryState];

    [self togglePathControlVisibility:YES];
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
        VLCInputNode * const node = childDataSource.nodeToDisplay;
        VLCInputNodePathControlItem * const nodePathItem = 
            [[VLCInputNodePathControlItem alloc] initWithInputNode:node];

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

- (void)togglePathControlVisibility:(BOOL)visible
{
    _pathControlContainerView.hidden = !visible;

    const CGFloat pathControlContainerViewHeight = _pathControlContainerView.frame.size.height;
    const CGFloat scrollViewsTopSpace = visible ? pathControlContainerViewHeight : 0;

    NSEdgeInsets scrollViewInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    scrollViewInsets.top += scrollViewsTopSpace;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = scrollViewInsets;
    _collectionViewScrollView.scrollerInsets = scrollerInsets;

    _tableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _tableViewScrollView.contentInsets =
        NSEdgeInsetsMake(scrollViewsTopSpace + _tableViewScrollView.window.titlebarHeight, 0, 0, 0);
}

- (void)returnHome
{
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;

    _childDataSource = nil;
    [self.pathControl clearInputNodePathControlItems];
    [self.navigationStack clear];

    [self reloadData];

    [self togglePathControlVisibility:NO];
}

- (void)homeButtonAction:(id)sender
{
    [self returnHome];
}

- (void)pathControlAction:(id)sender
{
    if (self.pathControl.clickedPathItem == nil || self.childDataSource == nil) {
        return;
    }

    NSPathControlItem * const selectedItem = self.pathControl.clickedPathItem;
    NSString * const itemNodeMrl = selectedItem.image.accessibilityDescription;

    VLCInputNodePathControlItem * const matchingItem = [self.pathControl.inputNodePathControlItems objectForKey:itemNodeMrl];
    if (matchingItem != nil) {
        self.childDataSource.nodeToDisplay = matchingItem.inputNode;
        [self.pathControl clearPathControlItemsAheadOf:selectedItem];
    } else {
        NSLog(@"Could not find matching item for clicked path item: %@", selectedItem);
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
    if (self.mediaSourceMode == VLCMediaSourceModeLAN) {
        _lanDeviceSnapshot = [self buildLANDeviceSnapshot];
    }
    if (self.viewMode == VLCLibraryGridViewModeSegment) {
        [self.collectionView reloadData];
    } else {
        [self.tableView reloadData];
    }

    [NSNotificationCenter.defaultCenter postNotificationName:VLCMediaSourceBaseDataSourceNodeChanged
                                                      object:self];
}

- (void)reloadData
{
    if (self.viewMode == VLCLibraryGridViewModeSegment) {
        [self.collectionView reloadData];
    } else {
        [self.tableView reloadData];
    }

    [NSNotificationCenter.defaultCenter postNotificationName:VLCMediaSourceBaseDataSourceNodeChanged
                                                      object:self];
}

- (void)browseFolderByMrl:(NSString *)mrl
{
    vlc_preparser_t *p_preparser = getNetworkPreparser();
    VLCMediaSource * const mediaSource =
        [[VLCMediaSource alloc] initWithFolderMrl:mrl andPreparser:p_preparser];
    if (mediaSource == nil) {
        NSLog(@"Could not create valid media source for mrl: %@", mrl);
        return;
    }
    VLCInputNode * const entryNode = mediaSource.category == SD_CAT_LAN
        ? mediaSource.rootNode.children.firstObject
        : mediaSource.rootNode;
    if (entryNode == nil) {
        NSLog(@"No entry node for mrl: %@", mrl);
        return;
    }
    [self configureChildDataSourceWithNode:entryNode andMediaSource:mediaSource];
    [self reloadData];
}

@end
