/*****************************************************************************
* simple_prefs.m: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008 the VideoLAN team
* $Id$
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

    [self resetControls];
    
    /* setup the toolbar */
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
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, nil];
}

- (void)initStrings
{
    [o_sprefs_reset_btn setEnabled: NO];
    msg_Warn( p_intf, "localisation of the simple preferences not implemented!" );
}

- (void)resetControls
{
    module_config_t *p_item;
    int i, y;
    char *psz_tmp;

    /**********************
     * interface settings *
     **********************/
    [o_intf_lang_pop removeAllItems];
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "language" );
    for( i = 0; p_item->ppsz_list[i] != nil; i++ )
    {
        [o_intf_lang_pop addItemWithTitle: _NS( p_item->ppsz_list_text[i] )];
        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) )
            y = i;
    }
    [o_intf_lang_pop selectItemAtIndex: y];

    [o_intf_art_pop removeAllItems];
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "album-art" );
    for( i = 0; i < p_item->i_list; i++ )
        [o_intf_art_pop addItemWithTitle: _NS( p_item->ppsz_list_text[i] )];
    [o_intf_art_pop selectItemAtIndex: 0];
    [o_intf_art_pop selectItemAtIndex: p_item->value.i];

    [o_intf_meta_ckb setState: config_GetInt( p_intf, "fetch-meta" )];
    [o_intf_fspanel_ckb setState: config_GetInt( p_intf, "macosx-fspanel" )];


    /******************
     * audio settings *
     ******************/
    [o_audio_enable_ckb setState: config_GetInt( p_intf, "audio" )];
    [o_audio_vol_fld setIntValue: config_GetInt( p_intf, "volume" )];
    [o_audio_vol_sld setIntValue: config_GetInt( p_intf, "volume" )];

    [o_audio_spdif_ckb setState: config_GetInt( p_intf, "spdif" )];

    [o_audio_dolby_pop removeAllItems];
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "force-dolby-surround" );
    for( i = 0; i < p_item->i_list; i++ )
        [o_audio_dolby_pop addItemWithTitle: _NS( p_item->ppsz_list_text[i] )];
    [o_audio_dolby_pop selectItemAtIndex: 0];
    [o_audio_dolby_pop selectItemAtIndex: p_item->value.i];
    
    [o_audio_lang_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "audio-language" )]];

    [o_audio_headphone_ckb setState: config_GetInt( p_intf, "headphone-dolby" )];
    
    psz_tmp = config_GetPsz( p_intf, "audio-filter" );
    if( psz_tmp )
        [o_audio_norm_ckb setState: (int)strstr( psz_tmp, "normvol" )];
    [o_audio_norm_fld setFloatValue: config_GetFloat( p_intf, "norm-max-level" )];
    
    // visualizer
    msg_Warn( p_intf, "visualizer not implemented!" );
    
    /* Last.FM is optional */
    if( module_Exists( p_intf, "audioscrobbler" ) )
    {
        [o_audio_lastuser_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "lastfm-username" )]];
        [o_audio_lastpwd_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "lastfm-password" )]];
        
        if( config_ExistIntf( VLC_OBJECT( p_intf ), "audioscrobbler" ) )
            [o_audio_last_ckb setState: NSOnState];
        else
            [o_audio_last_ckb setState: NSOffState];
    }

    /******************
     * video settings *
     ******************/
    
    /*******************
     * codecs settings *
     *******************/

    /*********************
     * subtitle settings *
     *********************/
    
    /********************
     * hotkeys settings *
     ********************/
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
        [self saveChangedSettings];
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
        [self resetControls];
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
        [self resetControls];
    }
}

- (void)saveChangedSettings
{
    module_config_t *p_item;
    char *psz_tmp;
    int i;
    
    /**********************
     * interface settings *
     **********************/
    if( b_intfSettingChanged )
    {
        p_item = config_FindConfig( VLC_OBJECT(p_intf), "language" );
        if( [o_intf_lang_pop indexOfSelectedItem] >= 0 )
            config_PutPsz( p_intf, "language", strdup( p_item->ppsz_list[[o_intf_lang_pop indexOfSelectedItem]] ) );
        else
            config_PutPsz( p_intf, "language", strdup( [[VLCMain sharedInstance] delocalizeString: [o_intf_lang_pop stringValue]] ) );

        p_item = config_FindConfig( VLC_OBJECT(p_intf), "album-art" );
        if( [o_intf_art_pop indexOfSelectedItem] >= 0 )
            config_PutInt( p_intf, "album-art", p_item->pi_list[[o_intf_art_pop indexOfSelectedItem]] );
        else
            config_PutInt( p_intf, "album-art", [o_intf_art_pop intValue] );

        config_PutInt( p_intf, "fetch-meta", [o_intf_meta_ckb state] );
        config_PutInt( p_intf, "macosx-fspanel", [o_intf_fspanel_ckb state] );

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
            msg_Err( p_intf, "An error occured while saving the Audio settings using SimplePrefs" );

        b_intfSettingChanged = NO;
    }
    
    /******************
     * audio settings *
     ******************/
    if( b_audioSettingChanged )
    {
        config_PutInt( p_intf, "audio", [o_audio_enable_ckb state] );
        config_PutInt( p_intf, "volume", [o_audio_vol_sld intValue] );
        config_PutInt( p_intf, "spdif", [o_audio_spdif_ckb state] );

        p_item = config_FindConfig( VLC_OBJECT(p_intf), "force-dolby-surround" );
        if( [o_audio_dolby_pop indexOfSelectedItem] >= 0 )
            config_PutInt( p_intf, "force-dolby-surround", p_item->pi_list[[o_audio_dolby_pop indexOfSelectedItem]] );
        else
            config_PutInt( p_intf, "force-dolby-surround", [o_audio_dolby_pop intValue] );

        config_PutPsz( p_intf, "audio-language", [[o_audio_lang_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "headphone-dolby", [o_audio_headphone_ckb state] );

        psz_tmp = config_GetPsz( p_intf, "audio-filter" );
        if(! psz_tmp)
            config_PutPsz( p_intf, "audio-filter", "volnorm" );
        else if( (int)strstr( psz_tmp, "normvol" ) == NO )
        {
            /* work-around a GCC 4.0.1 bug */
            psz_tmp = (char *)[[NSString stringWithFormat: @"%s:volnorm", psz_tmp] UTF8String];
            config_PutPsz( p_intf, "audio-filter", psz_tmp );
        }
        else
        {
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@":volnorm"]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"volnorm:"]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"volnorm"]] UTF8String];
            config_PutPsz( p_intf, "audio-filter", psz_tmp );
        }
        config_PutFloat( p_intf, "norm-max-level", [o_audio_norm_fld floatValue] );

        msg_Warn( p_intf, "visualizer not implemented!" );

        if( [o_audio_last_ckb state] == NSOnState )
            config_AddIntf( VLC_OBJECT( p_intf ), "audioscrobbler" );
        else
            config_RemoveIntf( VLC_OBJECT( p_intf ), "audioscrobbler" );

        config_PutPsz( p_intf, "lastfm-username", [[o_audio_lastuser_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "lastfm-password", [[o_audio_lastuser_fld stringValue] UTF8String] );

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "audioscrobbler" );
        i = i + config_SaveConfigFile( p_intf, "volnorm" );

        if( i != 0 )
            msg_Err( p_intf, "An error occured while saving the Audio settings using SimplePrefs" );
        b_audioSettingChanged = NO;
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
    b_intfSettingChanged = YES;
}

- (void)showInterfaceSettings
{
    msg_Dbg( p_intf, "showing interface settings" );
    [self showSettingsForCategory: o_intf_view];
}

- (IBAction)audioSettingChanged:(id)sender
{
    if( sender == o_audio_vol_sld )
        [o_audio_vol_fld setIntValue: [o_audio_vol_sld intValue]];
    
    if( sender == o_audio_vol_fld )
        [o_audio_vol_sld setIntValue: [o_audio_vol_fld intValue]];
    
    b_audioSettingChanged = YES;
}

- (void)showAudioSettings
{
    msg_Dbg( p_intf, "showing audio settings" );
    [self showSettingsForCategory: o_audio_view];
}
@end
