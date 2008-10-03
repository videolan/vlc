/*****************************************************************************
 * vlm.m: VLM Configuration panel for Mac OS X
 *****************************************************************************
 * Copyright (c) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#import "vlm.h"

#ifdef ENABLE_VLM

static NSString * VLCVLMToolbarIdentifier = @"Our VLM Toolbar Identifier";
static NSString * VLCVODToolbarIdentifier = @"VLM Item";
static NSString * VLCSchedToolbarIdentifier = @"Sched Item";
static NSString * VLCBcastToolbarIdentifier = @"Bcast Item";

@implementation VLCVLMController

static VLCVLMController *_o_sharedInstance = nil;

+ (VLCVLMController *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedInstance )
        [self dealloc];
    else
    {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    [self initStrings];
    
    /* setup the toolbar */
    NSToolbar * o_vlm_toolbar = [[[NSToolbar alloc] initWithIdentifier: VLCVLMToolbarIdentifier] autorelease];
    [o_vlm_toolbar setAllowsUserCustomization: NO];
    [o_vlm_toolbar setAutosavesConfiguration: NO];
    [o_vlm_toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [o_vlm_toolbar setSizeMode: NSToolbarSizeModeRegular];
    [o_vlm_toolbar setDelegate: self];
    [o_vlm_win setToolbar: o_vlm_toolbar];
}

#define CreateToolbarItem( o_name, o_desc, o_img, sel ) \
    o_toolbarItem = create_toolbar_item(o_itemIdent, o_name, o_desc, o_img, self, @selector(sel));
static inline NSToolbarItem *
create_toolbar_item( NSString * o_itemIdent, NSString * o_name, NSString * o_desc, NSString * o_img, id target, SEL selector )
{
    NSToolbarItem *o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease]; \

    [o_toolbarItem setLabel: o_name];
    [o_toolbarItem setPaletteLabel: o_desc];

    [o_toolbarItem setToolTip: o_desc];
    [o_toolbarItem setImage: [NSImage imageNamed: o_img]];

    [o_toolbarItem setTarget: target];
    [o_toolbarItem setAction: selector];

    [o_toolbarItem setEnabled: YES];
    [o_toolbarItem setAutovalidates: YES];

    return o_toolbarItem;
}

- (NSToolbarItem *) toolbar: (NSToolbar *)o_vlm_toolbar 
      itemForItemIdentifier: (NSString *)o_itemIdent 
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted
{
    NSToolbarItem *o_toolbarItem = nil;

    if( [o_itemIdent isEqual: VLCVODToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Video On Demand"), _NS("Video On Demand"), @"add_vod", addVOD );
    }
    else if( [o_itemIdent isEqual: VLCSchedToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Schedule"), _NS("Schedule"), @"add_schedule", addSched );
    }
    else if( [o_itemIdent isEqual: VLCBcastToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Broadcast"), _NS("Broadcast"), @"add_broadcast", addBcast );
    }

    return o_toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCBcastToolbarIdentifier, VLCSchedToolbarIdentifier, VLCVODToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCBcastToolbarIdentifier, VLCSchedToolbarIdentifier, VLCVODToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (void)initStrings
{
    /* not implemented */
}

- (void)showVLMWindow
{
    [o_vlm_win makeKeyAndOrderFront: self];
}

- (void)addBcast
{
    [NSApp beginSheet: o_bcast_panel modalForWindow: o_vlm_win
        modalDelegate: self didEndSelector: nil contextInfo: nil];
}

- (void)addVOD
{
    [NSApp beginSheet: o_vod_panel modalForWindow: o_vlm_win
        modalDelegate: self didEndSelector: nil contextInfo: nil];
}

- (void)addSched
{ 
    [NSApp beginSheet: o_sched_panel modalForWindow: o_vlm_win
         modalDelegate: self didEndSelector: nil contextInfo: nil];
}

- (IBAction)bcastButtonAction:(id)sender
{
    [NSApp endSheet: o_bcast_panel];
    [o_bcast_panel close];
}

- (IBAction)listDoubleClickAction:(id)sender
{
}

- (IBAction)schedButtonAction:(id)sender
{
    [NSApp endSheet: o_sched_panel];
    [o_sched_panel close];
}

- (IBAction)vodButtonAction:(id)sender
{
    [NSApp endSheet: o_vod_panel];
    [o_vod_panel close];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return 0;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    return @"dummy";
}

@end

#endif