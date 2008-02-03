/*****************************************************************************
* simple_prefs.m: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008 the VideoLAN team
* $Id:$
*
* Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "intf.h"
#import <vlc/vlc.h>
#import "simple_prefs.h"
#import "prefs.h"

static NSString* VLCSPrefsToolbarIdentifier = @"Our Simple Preferences Toolbar Identifier";
static NSString* VLCIntfSettingToolbarIdentifier = @"Intf Settings Item Identifier";
static NSString* VLCAudioSettingToolbarIdentifier = @"Audio Settings Item Identifier";

@implementation VLCSimplePrefs

static VLCSimplePrefs *_o_sharedInstance = nil;

+ (VLCSimplePrefs *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        p_intf = VLCIntf;
        _o_sharedInstance = [super init];
    }
    
    return _o_sharedInstance;
}

- (void)dealloc
{
    [o_currentlyShownCategoryView release];
    [o_sprefs_toolbar release];
    
    [super dealloc];
}

- (void)awakeFromNib
{
    [self initStrings];

    o_sprefs_toolbar = [[[NSToolbar alloc] initWithIdentifier: VLCSPrefsToolbarIdentifier] autorelease];

    [o_sprefs_toolbar setAllowsUserCustomization: NO];
    [o_sprefs_toolbar setAutosavesConfiguration: NO];
    [o_sprefs_toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [o_sprefs_toolbar setSizeMode: NSToolbarSizeModeRegular];

    [o_sprefs_toolbar setDelegate: self];

    [o_sprefs_win setToolbar: o_sprefs_toolbar];    
}

- (NSToolbarItem *) toolbar: (NSToolbar *)o_sprefs_toolbar 
      itemForItemIdentifier: (NSString *)o_itemIdent 
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted
{
    NSToolbarItem *o_toolbarItem = nil;
    
    if( [o_itemIdent isEqual: VLCIntfSettingToolbarIdentifier] )
    {
        o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease];

        [o_toolbarItem setLabel: _NS("Interface")];
        [o_toolbarItem setPaletteLabel: _NS("Interface settings")];

        [o_toolbarItem setToolTip: _NS("Interface settings")];
        [o_toolbarItem setImage: [NSImage imageNamed: @"spref_cone_Interface_64"]];

        [o_toolbarItem setTarget: self];
        [o_toolbarItem setAction: @selector(showInterfaceSettings)];

        [o_toolbarItem setEnabled: YES];
        [o_toolbarItem setAutovalidates: YES];
    }
    else if( [o_itemIdent isEqual: VLCAudioSettingToolbarIdentifier] )
    {
        o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease];

        [o_toolbarItem setLabel: _NS("Audio")];
        [o_toolbarItem setPaletteLabel: _NS("General Audio settings")];

        [o_toolbarItem setToolTip: _NS("General Audio settings")];
        [o_toolbarItem setImage: [NSImage imageNamed: @"spref_cone_Audio_64"]];

        [o_toolbarItem setTarget: self];
        [o_toolbarItem setAction: @selector(showAudioSettings)];

        [o_toolbarItem setEnabled: YES];
        [o_toolbarItem setAutovalidates: YES];
    }
    
    return o_toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (void)initStrings
{
    [o_sprefs_reset_btn setEnabled: NO];
    msg_Warn( p_intf, "localisation of the simple preferences not implemented!" );
}

- (void)showSimplePrefs
{
    /* we want to show the interface settings, if no category was chosen */
    if( [o_sprefs_toolbar selectedItemIdentifier] == nil )
    {
        [o_sprefs_toolbar setSelectedItemIdentifier: VLCIntfSettingToolbarIdentifier];
        [self showInterfaceSettings];
    }

    [o_sprefs_win makeKeyAndOrderFront: self];
}

- (IBAction)buttonAction:(id)sender
{
    if( sender == o_sprefs_cancel_btn )
        [o_sprefs_win orderOut: sender];
    else if( sender == o_sprefs_save_btn )
    {
        msg_Warn( p_intf, "sprefs saving not implemented, your changes have no effect!" );
        [o_sprefs_win orderOut: sender];
    }
    else if( sender == o_sprefs_reset_btn )
        NSBeginInformationalAlertSheet( _NS("Reset Preferences"), _NS("Cancel"),
                                        _NS("Continue"), nil, o_sprefs_win, self,
                                        @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil,
                                        _NS("Beware this will reset the VLC media player preferences.\n"
                                            "Are you sure you want to continue?") );
    else if( sender == o_sprefs_basicFull_matrix )
    {
        [o_sprefs_win orderOut: self];
        [[[VLCMain sharedInstance] getPreferences] showPrefs];
        /* TODO: reset our selector controls here */
    }
    else
        msg_Err( p_intf, "unknown buttonAction sender" );
}

- (void)sheetDidEnd:(NSWindow *)o_sheet 
         returnCode:(int)i_return
        contextInfo:(void *)o_context
{
    if( i_return == NSAlertAlternateReturn )
    {
        config_ResetAll( p_intf );
        /* TODO: we need to reset our views here */
    }
}

- (void)showSettingsForCategory: (id)o_new_category_view
{
    msg_Dbg( p_intf, "switching to another category" );
    NSRect o_win_rect, o_view_rect, o_old_view_rect;
    o_win_rect = [o_sprefs_win frame];
    o_view_rect = [o_new_category_view frame];
    
    if( o_currentlyShownCategoryView != nil )
    {
        /* restore our window's height, if we've shown another category previously */
        o_old_view_rect = [o_currentlyShownCategoryView frame];
        o_win_rect.size.height = o_win_rect.size.height - o_old_view_rect.size.height;
        
        /* remove our previous category view */
        [o_currentlyShownCategoryView removeFromSuperviewWithoutNeedingDisplay];
    }
    
    o_win_rect.size.height = o_win_rect.size.height + o_view_rect.size.height;
    
    [o_sprefs_win displayIfNeeded];
    [o_sprefs_win setFrame: o_win_rect display:YES animate: YES];
    
    [o_new_category_view setFrame: NSMakeRect( 0, 
                                               [o_sprefs_controls_box frame].size.height, 
                                               o_view_rect.size.width, 
                                               o_view_rect.size.height )];
    [o_new_category_view setNeedsDisplay: YES];
    [o_new_category_view setAutoresizesSubviews: YES];
    [[o_sprefs_win contentView] addSubview: o_new_category_view];
    
    /* keep our current category for further reference */
    [o_currentlyShownCategoryView release];
    o_currentlyShownCategoryView = o_new_category_view;
    [o_currentlyShownCategoryView retain];
}

- (IBAction)interfaceSettingChanged:(id)sender
{
}

- (void)showInterfaceSettings
{
    msg_Dbg( p_intf, "showing interface settings" );
    [self showSettingsForCategory: o_intf_view];
}

- (IBAction)audioSettingChanged:(id)sender
{
}

- (void)showAudioSettings
{
    msg_Dbg( p_intf, "showing audio settings" );
    [self showSettingsForCategory: o_audio_view];
}
@end
