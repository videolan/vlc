/*****************************************************************************
 * VLCMainWindowController.m: VLCMainWindowController implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id: VLCMainWindow.h 24209 2008-01-09 22:05:17Z pdherbemont $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import "VLCMainWindowController.h"
#import "VLCAppAdditions.h"
#import "ImageAndTextCell.h"

/******************************************************************************
 * @implementation VLCMainWindowController
 */

@implementation VLCMainWindowController

@synthesize mediaPlayer;
@synthesize videoView;
@synthesize mediaArrayController;

- (void)awakeFromNib
{
    NSTableColumn * tableColumn;

    /***********************************
     * Init the media player
     */

    NSAssert( mediaPlayer, @"No mediaPlayer" );
    
    categoriesTreeController = [[NSTreeController alloc] init];
    
    /***********************************
     * CategoriesList OutlineView content
     */
    /* categoriesTreeController */ 
    NSAssert( categoriesTreeController, @"No categoriesTreeController" );
    NSAssert( categoriesListView, @"No categoriesListView" );
    NSAssert( controller, @"No controller" );

    [categoriesTreeController setContent:controller.categories];
    //[categoriesTreeController bind:@"content" toObject:controller withKeyPath:@"categories" options:nil];
  
    [categoriesTreeController setChildrenKeyPath:@"childrenInCategoriesList"];

    /* Bind the "name" table column */
    tableColumn = [categoriesListView tableColumnWithIdentifier:@"name"];
    [tableColumn bind:@"value" toObject:categoriesTreeController withKeyPath:@"arrangedObjects.descriptionInCategoriesList" options:nil];

    /* Use an ImageAndTextCell in the "name" table column */
    ImageAndTextCell * cell = [[ImageAndTextCell alloc] init];
    [cell setFont:[[tableColumn dataCell] font]];
    [cell setImageKeyPath:@"image"];
    [tableColumn setDataCell: cell];

    /* Other setup */
    [categoriesListView setIndentationMarkerFollowsCell:YES];
    [categoriesListView setAutoresizesOutlineColumn:NO];
    [categoriesListView setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleSourceList];
    [categoriesListView setDelegate:self];

    [categoriesListView registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, @"VLCMediaURLType", nil]];
    [categoriesListView setDataSource: self];

    /***********************************
     * mediaListView setup
     */

    /* 1- Drag and drop */
    NSAssert( mediaArrayController, @"No mediaArrayController" );
    NSAssert( mediaListView, @"No mediaListView" );
    [mediaListView registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil]];
    [mediaListView setDataSource:mediaArrayController];
	/* 3- binding for "title" column */
    tableColumn = [mediaListView tableColumnWithIdentifier:@"title"];
	[tableColumn bind:@"value" toObject: mediaArrayController withKeyPath:@"arrangedObjects.metaDictionary.title" options:nil];


    /* 2- Double click */
    [mediaListView setTarget:self];
    [mediaListView setDoubleAction:@selector(mediaListViewItemDoubleClicked:)];


	/* 4- binding for "state" column */
    tableColumn = [mediaListView tableColumnWithIdentifier:@"state"];
	[tableColumn bind:@"value" toObject: mediaArrayController withKeyPath:@"arrangedObjects.stateAsImage" options:nil];

    /* 6- Bind the @"contentArray" and contentMediaList of the mediaArrayController */
    [mediaArrayController bind:@"contentArray" toObject:categoriesTreeController withKeyPath:@"selection.childrenInCategoriesListForDetailView.media" options:nil];

    [mediaArrayController bind:@"contentMediaList" toObject:categoriesTreeController withKeyPath:@"selection.childrenInCategoriesListForDetailView.parentMediaList" options:nil];
    
    /* 7- Aspect */
    [mediaListView setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleSourceList];
    [mediaListView setAllowsTypeSelect:YES];

    /***********************************
     * videoView setup
     */
    [videoView setItemsTree:controller.categories];
    [videoView setNodeKeyPath:@"childrenInVideoView"];
    [videoView setContentKeyPath:@"descriptionInVideoView"];
    [videoView setTarget:self];
    [videoView setAction:@selector(videoViewItemClicked:)];
    
    /***********************************
     * Toolbar setup
     */

    /***********************************
     * Other interface element setup
     */
#if 0

    [mediaListItemsCount bind:@"displayPatternValue1" toObject:mediaArrayController withKeyPath:@"arrangedObjects.@count" options:[NSDictionary dictionaryWithObject:@"%{value1}@ items" forKey:NSDisplayPatternBindingOption]];
    [mediaListItemFetchedStatus bind:@"animate" toObject:categoriesTreeController withKeyPath:@"selection.currentlyFetchingItems" options:[NSDictionary dictionaryWithObject:@"%{value1}@ items" forKey:NSDisplayPatternBindingOption]];

    [fillScreenButton bind:@"value" toObject:videoView withKeyPath:@"fillScreen" options: nil];
    [fullScreenButton bind:@"value" toObject:videoView withKeyPath:@"fullScreen" options: nil];
    [fullScreenButton bind:@"enabled" toObject:mediaPlayer withKeyPath:@"media" options: [NSDictionary dictionaryWithObject:@"NonNilAsBoolTransformer" forKey:NSValueTransformerNameBindingOption]];
    [fillScreenButton bind:@"enabled" toObject:mediaPlayer withKeyPath:@"media" options: [NSDictionary dictionaryWithObject:@"NonNilAsBoolTransformer" forKey:NSValueTransformerNameBindingOption]];
    [mediaReadingProgressSlider bind:@"enabled" toObject:mediaPlayer withKeyPath:@"media" options: [NSDictionary dictionaryWithObject:@"NonNilAsBoolTransformer" forKey:NSValueTransformerNameBindingOption]];
    [mediaReadingProgressSlider bind:@"enabled2" toObject:mediaPlayer withKeyPath:@"seekable" options: nil];

    [mediaReadingProgressSlider bind:@"value" toObject:mediaPlayer withKeyPath:@"position" options:
        [NSDictionary dictionaryWithObjectsAndKeys:@"Float10000FoldTransformer", NSValueTransformerNameBindingOption,
                                                  [NSNumber numberWithBool:NO], NSConditionallySetsEnabledBindingOption, nil ]];
    [mediaReadingProgressText bind:@"value" toObject:mediaPlayer withKeyPath:@"time.stringValue" options: nil];
    [mediaDescriptionText bind:@"value" toObject:mediaPlayer withKeyPath:@"description" options: nil];
    [self bind:@"representedFilename" toObject:mediaPlayer withKeyPath:@"media.url" options: [NSDictionary dictionaryWithObject:@"URLToRepresentedFileNameTransformer" forKey:NSValueTransformerNameBindingOption]];
    [self bind:@"title" toObject:mediaPlayer withKeyPath:@"description" options: nil];

    [navigatorViewToggleButton bind:@"value" toObject:self withKeyPath:@"navigatorViewVisible" options: nil];
#endif

    /* Playlist buttons */
#if 0
    [removePlaylistButton bind:@"enabled" toObject:categoriesTreeController withKeyPath:@"selection.editableInCategoriesList" options: nil];
#endif
    [removePlaylistButton setTarget:categoriesTreeController];
    [removePlaylistButton setAction:@selector(remove:)];
    [addPlaylistButton setTarget:controller];
    [addPlaylistButton setAction:@selector(addPlaylist:)];

    /* mediaPlayer */
#if 0
    [mediaPlayerPlayPauseStopButton bind:@"enabled" toObject:mediaPlayer withKeyPath:@"media" options: [NSDictionary dictionaryWithObject:@"NonNilAsBoolTransformer" forKey:NSValueTransformerNameBindingOption]];
    [mediaPlayerPlayPauseStopButton bind:@"state"   toObject:mediaPlayer withKeyPath:@"playing" options: nil];
    [mediaPlayerPlayPauseStopButton bind:@"alternateImage" toObject:mediaPlayer withKeyPath:@"stateAsButtonAlternateImage" options: nil];
    [mediaPlayerPlayPauseStopButton bind:@"image"   toObject:mediaPlayer withKeyPath:@"stateAsButtonImage" options: nil];
    [mediaPlayerBackwardPrevButton  bind:@"enabled" toObject:mediaPlayer withKeyPath:@"playing" options: nil];
    [mediaPlayerForwardNextButton   bind:@"enabled" toObject:mediaPlayer withKeyPath:@"playing" options: nil];
#endif

    [mediaPlayerForwardNextButton   setTarget:mediaPlayer];
    [mediaPlayerForwardNextButton   setAction:@selector(fastForward)];
    [mediaPlayerBackwardPrevButton  setTarget:mediaPlayer];
    [mediaPlayerBackwardPrevButton  setAction:@selector(rewind)];
    [mediaPlayerPlayPauseStopButton setTarget:mediaPlayer];
    [mediaPlayerPlayPauseStopButton setAction:@selector(pause)];

    /* Last minute setup */
    [categoriesListView expandItem:nil expandChildren:YES];
    [categoriesListView selectRowIndexes:[NSIndexSet indexSetWithIndex:[categoriesListView numberOfRows] > 0 ? [categoriesListView numberOfRows]-1 : 0] byExtendingSelection:NO];
    [self setNavigatorViewVisible:NO animate:NO];
    [self showWindow:self];
    [mainSplitView setDelegate:self];
}

- (BOOL)navigatorViewVisible
{
    return [mainSplitView sliderPosition] <= [mainSplitView bounds].size.width - [mainSplitView dividerThickness] - 30.f /* To be tolerant */;
}

- (void)setNavigatorViewVisible:(BOOL)wantsVisible animate:(BOOL)animate
{
    if( [self navigatorViewVisible] == wantsVisible )
        return;

    if( !animate ) [self willChangeValueForKey:@"navigatorViewVisible"];

    VLCOneSplitView * splitView = animate ? [mainSplitView animator] : mainSplitView;
        
    if( wantsVisible )
    {
        if( navigatorViewWidth >= [mainSplitView bounds].size.width - 200.f )
            navigatorViewWidth = [mainSplitView bounds].size.width - 200.f;
        [splitView setSliderPosition:navigatorViewWidth];
    }
    else
    {
        navigatorViewWidth = [videoView frame].size.width;
        [splitView setSliderPosition:[mainSplitView bounds].size.width - [mainSplitView dividerThickness]];
    }
    if( !animate ) [self didChangeValueForKey:@"navigatorViewVisible"];
}

- (void)setNavigatorViewVisible:(BOOL)wantsVisible
{
    [self setNavigatorViewVisible:wantsVisible animate:YES];
}

- (IBAction)mediaListViewItemDoubleClicked:(id)sender
{
    if([[mediaArrayController selectedObjects] count] <= 0 )
        return;
    [mediaPlayer setMedia:[[mediaArrayController selectedObjects] objectAtIndex:0]];
    [mediaPlayer play];
}

- (void)videoViewItemClicked:(id)sender
{
    id object = [sender selectedObject];
    NSAssert( [object isKindOfClass:[VLCMedia class]], @"Object is not a VLCMedia" );

    [mediaPlayer setMedia:object];
    [mediaPlayer play];
}

@end

/******************************************************************************
 * @implementation VLCMainWindowController (NSToolbarDelegating)
 */

@implementation VLCMainWindowController (NSToolbarDelegating)
/* Our item identifiers */
static NSString * VLCToolbarMediaControl     = @"VLCToolbarMediaControl";
static NSString * VLCToolbarMediaAudioVolume = @"VLCToolbarMediaAudioVolume";
static NSString * VLCToolbarMediaDescription = @"VLCToolbarMediaDescription";

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:
                        NSToolbarCustomizeToolbarItemIdentifier,
                        NSToolbarFlexibleSpaceItemIdentifier,
                        NSToolbarSpaceItemIdentifier,
                        NSToolbarSeparatorItemIdentifier,
                        VLCToolbarMediaControl,
                        VLCToolbarMediaAudioVolume,
                        VLCToolbarMediaDescription,
                        nil ];
}

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *) toolbar
{
    return [NSArray arrayWithObjects:
                        VLCToolbarMediaControl,
                        VLCToolbarMediaAudioVolume,
                        VLCToolbarMediaDescription,
                        nil ];
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
    NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: itemIdentifier] autorelease];
 
    if( [itemIdentifier isEqual: VLCToolbarMediaControl] )
    {
        [toolbarItem setLabel:@"Media Controls"];
        [toolbarItem setPaletteLabel:@"Media Controls"];
     
        [toolbarItem setView:toolbarMediaControl];
        [toolbarItem setMinSize:[[toolbarItem view] frame].size];
        [toolbarItem setMaxSize:[[toolbarItem view] frame].size];

        /* TODO: setup a menu */
    }
    else if( [itemIdentifier isEqual: VLCToolbarMediaAudioVolume] )
    {
        [toolbarItem setLabel:@"Audio Volume"];
        [toolbarItem setPaletteLabel:@"Audio Volume"];
     
        [toolbarItem setView:toolbarMediaAudioVolume];
        [toolbarItem setMinSize:[[toolbarItem view] frame].size];
        [toolbarItem setMaxSize:[[toolbarItem view] frame].size];

        /* TODO: setup a menu */
    }
    else  if( [itemIdentifier isEqual: VLCToolbarMediaDescription] )
    {
        [toolbarItem setLabel:@"Media Description"];
        [toolbarItem setPaletteLabel:@"Media Description"];
     
        [toolbarItem setView:toolbarMediaDescription];
        [toolbarItem setMinSize:[[toolbarItem view] frame].size];
        [toolbarItem setMaxSize:NSMakeSize(10000 /* Can be really big */, NSHeight([[toolbarItem view] frame]))];

        /* TODO: setup a menu */
    }
    else
    {
        /* itemIdentifier referred to a toolbar item that is not
         * provided or supported by us or Cocoa
         * Returning nil will inform the toolbar
         * that this kind of item is not supported */
        toolbarItem = nil;
    }
    return toolbarItem;
}
@end

/******************************************************************************
 * VLCMainWindowController (CategoriesListDelegate)
 */
@implementation VLCMainWindowController (CategoriesListDelegate)
- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item
{
    return [[item representedObject] isKindOfClass:[NSDictionary class]];
}
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
    return !([[item representedObject] isKindOfClass:[NSDictionary class]]);
}
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    [cell setRepresentedObject:[item representedObject]];
}
@end

/******************************************************************************
 * VLCMainWindowController (CategoriesListDataSource)
 */
@implementation VLCMainWindowController (CategoriesListDataSource)
/* Drag and drop */
- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id < NSDraggingInfo >)info item:(id)item childIndex:(NSInteger)index
{
    int i;

    if(![item respondsToSelector:@selector(representedObject)])
        return NO;
    
    NSArray *droppedItems = [[info draggingPasteboard] propertyListForType:@"VLCMediaURLType"];
    if( !droppedItems )
        droppedItems = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if( !droppedItems )
        droppedItems = [[info draggingPasteboard] propertyListForType:NSURLPboardType];

    NSAssert( droppedItems, @"Dropped an unsupported object type on the outline View" );

    VLCMediaList * mediaList = [(VLCMedia *)[item representedObject] subitems];

    for (i = 0; i < [droppedItems count]; i++)
    {
        NSString * filename = [droppedItems objectAtIndex:i];
		VLCMedia *media = [VLCMedia mediaWithPath:filename];
        [mediaList lock];
		[mediaList insertMedia:media atIndex:index+1];
        [mediaList unlock];
    }
    return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id < NSDraggingInfo >)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    NSArray *droppedItems = [[info draggingPasteboard] propertyListForType:@"VLCMediaURLType"];
    if( !droppedItems )
        droppedItems = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if( !droppedItems )
        droppedItems = [[info draggingPasteboard] propertyListForType:NSURLPboardType];

    if(! droppedItems ||
       ![item respondsToSelector:@selector(representedObject)] ||
       ![[item representedObject] isKindOfClass:[VLCMedia class]] )
    {
        return NSDragOperationNone;
    }

    return NSDragOperationMove;
}
@end

/******************************************************************************
 * VLCMainWindowController (SplitViewDelegate)
 */
@implementation VLCMainWindowController (SplitViewDelegate)

- (void)splitViewWillResizeSubviews:(NSNotification *)aNotification
{
    [self willChangeValueForKey:@"navigatorViewVisible"];
}
- (void)splitViewDidResizeSubviews:(NSNotification *)aNotification
{
    [self didChangeValueForKey:@"navigatorViewVisible"];
}

@end
