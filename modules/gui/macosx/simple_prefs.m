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
static NSString* VLCVideoSettingToolbarIdentifier = @"Video Settings Item Identifier";
static NSString* VLCOSDSettingToolbarIdentifier = @"Subtitles Settings Item Identifier";
static NSString* VLCInputSettingToolbarIdentifier = @"Input Settings Item Identifier";
static NSString* VLCHotkeysSettingToolbarIdentifier = @"Hotkeys Settings Item Identifier";

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
    else if( [o_itemIdent isEqual: VLCVideoSettingToolbarIdentifier] )
    {
        o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease];

        [o_toolbarItem setLabel: _NS("Video")];
        [o_toolbarItem setPaletteLabel: _NS("General Video settings")];

        [o_toolbarItem setToolTip: _NS("General Video settings")];
        [o_toolbarItem setImage: [NSImage imageNamed: @"spref_cone_Video_64"]];

        [o_toolbarItem setTarget: self];
        [o_toolbarItem setAction: @selector(showVideoSettings)];

        [o_toolbarItem setEnabled: YES];
        [o_toolbarItem setAutovalidates: YES];
    }
    else if( [o_itemIdent isEqual: VLCOSDSettingToolbarIdentifier] )
    {
        o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease];

        [o_toolbarItem setLabel: _NS("Subtitles & OSD")];
        [o_toolbarItem setPaletteLabel: _NS("Subtitles & OSD settings")];

        [o_toolbarItem setToolTip: _NS("Subtitles & OSD settings")];
        [o_toolbarItem setImage: [NSImage imageNamed: @"spref_cone_Subtitles_64"]];

        [o_toolbarItem setTarget: self];
        [o_toolbarItem setAction: @selector(showOSDSettings)];

        [o_toolbarItem setEnabled: YES];
        [o_toolbarItem setAutovalidates: YES];
    }
    else if( [o_itemIdent isEqual: VLCInputSettingToolbarIdentifier] )
    {
        o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease];

        [o_toolbarItem setLabel: _NS("Input & Codecs")];
        [o_toolbarItem setPaletteLabel: _NS("Input & Codec settings")];

        [o_toolbarItem setToolTip: _NS("Input & Codec settings")];
        [o_toolbarItem setImage: [NSImage imageNamed: @"spref_cone_Input_64"]];

        [o_toolbarItem setTarget: self];
        [o_toolbarItem setAction: @selector(showInputSettings)];

        [o_toolbarItem setEnabled: YES];
        [o_toolbarItem setAutovalidates: YES];
    }

    return o_toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier, 
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier, 
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier, 
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, nil];
}

- (void)initStrings
{
    msg_Warn( p_intf, "localisation of the simple preferences is not implemented!" );
}

- (void)resetControls
{
    module_config_t *p_item;
    int i, y = 0;
    char *psz_tmp;

    #define SetupIntList( object, name ) \
    [object removeAllItems]; \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    for( i = 0; i < p_item->i_list; i++ ) \
    { \
        if( p_item->ppsz_list_text[i] != NULL) \
            [object addItemWithTitle: _NS( p_item->ppsz_list_text[i] )]; \
        else \
            [object addItemWithTitle: [NSString stringWithUTF8String: p_item->ppsz_list[i]]]; \
    } \
    if( p_item->value.i < [object numberOfItems] ) \
        [object selectItemAtIndex: p_item->value.i]; \
    else \
        [object selectItemAtIndex: 0]

    #define SetupStringList( object, name ) \
        [object removeAllItems]; \
        y = 0; \
        p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
        for( i = 0; p_item->ppsz_list[i] != nil; i++ ) \
        { \
            [object addItemWithTitle: _NS( p_item->ppsz_list_text[i] )]; \
            if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) ) \
                y = i; \
        } \
        [object selectItemAtIndex: y]

    /**********************
     * interface settings *
     **********************/
    SetupStringList( o_intf_lang_pop, "language" );
    SetupIntList( o_intf_art_pop, "album-art" );

    [o_intf_meta_ckb setState: config_GetInt( p_intf, "fetch-meta" )];
    [o_intf_fspanel_ckb setState: config_GetInt( p_intf, "macosx-fspanel" )];
    [o_intf_embedded_ckb setState: config_GetInt( p_intf, "embedded-video" )];

    /******************
     * audio settings *
     ******************/
    [o_audio_enable_ckb setState: config_GetInt( p_intf, "audio" )];
    [o_audio_vol_fld setIntValue: config_GetInt( p_intf, "volume" )];
    [o_audio_vol_sld setIntValue: config_GetInt( p_intf, "volume" )];

    [o_audio_spdif_ckb setState: config_GetInt( p_intf, "spdif" )];

    SetupIntList( o_audio_dolby_pop, "force-dolby-surround" );

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
    [o_video_enable_ckb setState: config_GetInt( p_intf, "video" )];
    [o_video_fullscreen_ckb setState: config_GetInt( p_intf, "fullscreen" )];
    [o_video_onTop_ckb setState: config_GetInt( p_intf, "video-on-top" )];
    [o_video_skipFrames_ckb setState: config_GetInt( p_intf, "skip-frames" )];
    [o_video_black_ckb setState: config_GetInt( p_intf, "macosx-black" )];

    msg_Warn( p_intf, "vout module and display device selectors not implemented!" );

    if( config_GetPsz( p_intf, "snapshot-path" ) != NULL )
        [o_video_snap_folder_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "snapshot-path" )]];
    [o_video_snap_prefix_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "snapshot-prefix" )]];
    [o_video_snap_seqnum_ckb setState: config_GetInt( p_intf, "snapshot-sequential" )];
    
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "snapshot-format" );
    for( i = 0; p_item->ppsz_list[i] != nil; i++ )
    {
        [o_video_snap_format_pop addItemWithTitle: [NSString stringWithUTF8String: p_item->ppsz_list[i]]];
        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) )
            y = i;
    }
    [o_video_snap_format_pop selectItemAtIndex: y];

    /***************************
     * input & codecs settings *
     ***************************/
    [o_input_serverport_fld setIntValue: config_GetInt( p_intf, "server-port" )];
    if( config_GetPsz( p_intf, "http-proxy" ) != NULL )
        [o_input_httpproxy_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "http-proxy" )]];
    [o_input_postproc_fld setIntValue: config_GetInt( p_intf, "ffmpeg-pp-q" )];

    SetupIntList( o_input_avi_pop, "avi-index" );

    [o_input_rtsp_ckb setState: config_GetInt( p_intf, "rtsp-tcp" )];

    psz_tmp = config_GetPsz( p_intf, "access-filter" );
    if( psz_tmp )
    {
        [o_input_record_ckb setState: (int)strstr( psz_tmp, "record" )];
        [o_input_dump_ckb setState: (int)strstr( psz_tmp, "dump" )];
        [o_input_bandwidth_ckb setState: (int)strstr( psz_tmp, "bandwidth" )];
        [o_input_timeshift_ckb setState: (int)strstr( psz_tmp, "timeshift" )];
    }

    [o_input_cachelevel_pop removeAllItems];
    [o_input_cachelevel_pop addItemsWithTitles: 
        [NSArray arrayWithObjects: _NS("Custom"), _NS("Lowest latency"), _NS("Low latency"), _NS("Normal"),
            _NS("High latency"), _NS("Higher latency"), nil]];
    [[o_input_cachelevel_pop itemAtIndex: 0] setTag: 0];
    [[o_input_cachelevel_pop itemAtIndex: 1] setTag: 100];
    [[o_input_cachelevel_pop itemAtIndex: 2] setTag: 200];
    [[o_input_cachelevel_pop itemAtIndex: 3] setTag: 300];
    [[o_input_cachelevel_pop itemAtIndex: 4] setTag: 400];
    [[o_input_cachelevel_pop itemAtIndex: 5] setTag: 500];
    
#define TestCaC( name ) \
    b_cache_equal =  b_cache_equal && \
        ( i_cache == config_GetInt( p_intf, name ) )

#define TestCaCi( name, int ) \
        b_cache_equal = b_cache_equal &&  \
        ( ( i_cache * int ) == config_GetInt( p_intf, name ) )

    /* Select the accurate value of the PopupButton */
    bool b_cache_equal = true;
    int i_cache = config_GetInt( p_intf, "file-caching");
    
    TestCaC( "udp-caching" );
    if( module_Exists (p_intf, "dvdread") )
        TestCaC( "dvdread-caching" );
    if( module_Exists (p_intf, "dvdnav") )
        TestCaC( "dvdnav-caching" );
    TestCaC( "tcp-caching" );
    TestCaC( "fake-caching" ); 
    TestCaC( "cdda-caching" );
    TestCaC( "screen-caching" ); 
    TestCaC( "vcd-caching" );
    TestCaCi( "rtsp-caching", 4 ); 
    TestCaCi( "ftp-caching", 2 );
    TestCaCi( "http-caching", 4 );
    if(module_Exists (p_intf, "access_realrtsp"))
        TestCaCi( "realrtsp-caching", 10 );
    TestCaCi( "mms-caching", 19 );
    if( b_cache_equal )
        [o_input_cachelevel_pop selectItemWithTag: i_cache];
    else
        [o_input_cachelevel_pop selectItemWithTitle: _NS("Custom")];

    /*********************
     * subtitle settings *
     *********************/
    [o_osd_osd_ckb setState: config_GetInt( p_intf, "osd" )];
    
    [o_osd_encoding_pop removeAllItems];
    y = 0;
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "subsdec-encoding" );
    for( i = 0; p_item->ppsz_list[i] != nil; i++ )
    {
        if( p_item->ppsz_list[i] != "" )
            [o_osd_encoding_pop addItemWithTitle: _NS( p_item->ppsz_list[i] )];
        else
            [o_osd_encoding_pop addItemWithTitle: @" "];

        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) )
            y = i;
    }
    [o_osd_encoding_pop selectItemAtIndex: y];
    
    [o_osd_lang_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "sub-language" )]];
    if( config_GetPsz( p_intf, "freetype-font" ) != NULL )
        [o_osd_font_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "freetype-font" )]];

    SetupIntList( o_osd_font_color_pop, "freetype-color" );
    SetupIntList( o_osd_font_size_pop, "freetype-rel-fontsize" );
    SetupIntList( o_osd_font_effect_pop, "freetype-effect" );

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
        b_intfSettingChanged = b_videoSettingChanged = b_audioSettingChanged = YES;
        [self resetControls];
    }
}

- (void)saveChangedSettings
{
    module_config_t *p_item;
    char *psz_tmp;
    int i;
    
#define SaveIntList( object, name ) \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    if( [object indexOfSelectedItem] >= 0 ) \
        config_PutInt( p_intf, name, p_item->pi_list[[object indexOfSelectedItem]] ); \
    else \
        config_PutInt( p_intf, name, [object intValue] ) \
                    
#define SaveStringList( object, name ) \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    if( [object indexOfSelectedItem] >= 0 ) \
        config_PutPsz( p_intf, name, strdup( p_item->ppsz_list[[object indexOfSelectedItem]] ) ); \
    else \
        config_PutPsz( p_intf, name, strdup( [[VLCMain sharedInstance] delocalizeString: [object stringValue]] ) )

    /**********************
     * interface settings *
     **********************/
    if( b_intfSettingChanged )
    {
        SaveStringList( o_intf_lang_pop, "language" );
        SaveIntList( o_intf_art_pop, "album-art" );

        config_PutInt( p_intf, "fetch-meta", [o_intf_meta_ckb state] );
        config_PutInt( p_intf, "macosx-fspanel", [o_intf_fspanel_ckb state] );
        config_PutInt( p_intf, "embedded-video", [o_intf_embedded_ckb state] );

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occured while saving the Interface settings using SimplePrefs" );
            i = 0;
        }

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

        SaveIntList( o_audio_dolby_pop, "force-dolby-surround" );

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

        /* Last.FM is optional */
        if( module_Exists( p_intf, "audioscrobbler" ) )
        {    
            if( [o_audio_last_ckb state] == NSOnState )
                config_AddIntf( VLC_OBJECT( p_intf ), "audioscrobbler" );
            else
                config_RemoveIntf( VLC_OBJECT( p_intf ), "audioscrobbler" );

            config_PutPsz( p_intf, "lastfm-username", [[o_audio_lastuser_fld stringValue] UTF8String] );
            config_PutPsz( p_intf, "lastfm-password", [[o_audio_lastuser_fld stringValue] UTF8String] );
        }

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "audioscrobbler" );
        i = i + config_SaveConfigFile( p_intf, "volnorm" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occured while saving the Audio settings using SimplePrefs" );
            i = 0;
        }
        b_audioSettingChanged = NO;
    }
    
    /******************
     * video settings *
     ******************/
    if( b_videoSettingChanged )
    {
        config_PutInt( p_intf, "video", [o_video_enable_ckb state] );
        config_PutInt( p_intf, "fullscreen", [o_video_fullscreen_ckb state] );
        config_PutInt( p_intf, "video-on-top", [o_video_onTop_ckb state] );
        config_PutInt( p_intf, "skip-frames", [o_video_skipFrames_ckb state] );
        config_PutInt( p_intf, "macosx-black", [o_video_black_ckb state] );

        msg_Warn( p_intf, "vout module and display device selectors not implemented!" );

        config_PutPsz( p_intf, "snapshot-path", [[o_video_snap_folder_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "snapshot-prefix", [[o_video_snap_prefix_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "snapshot-sequential", [o_video_snap_seqnum_ckb state] );

        if( [o_video_snap_format_pop indexOfSelectedItem] >= 0 )
            config_PutPsz( p_intf, "snapshot-format", [[[o_video_snap_format_pop selectedItem] title] UTF8String] );

        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occured while saving the Video settings using SimplePrefs" );
            i = 0;
        }
        b_videoSettingChanged = NO;
    }
    
    /***************************
     * input & codecs settings *
     ***************************/
    if( b_inputSettingChanged )
    {
        config_PutInt( p_intf, "server-port", [o_input_serverport_fld intValue] );
        config_PutPsz( p_intf, "http-proxy", [[o_input_httpproxy_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "ffmpeg-pp-q", [o_input_postproc_fld intValue] );

        SaveIntList( o_input_avi_pop, "avi-index" );

        config_PutInt( p_intf, "rtsp-tcp", [o_input_rtsp_ckb state] );

        #define CaCi( name, int ) config_PutInt( p_intf, name, int * [[o_input_cachelevel_pop selectedItem] tag] )
        #define CaC( name ) CaCi( name, 1 )
        msg_Dbg( p_intf, "Adjusting all cache values at: %i", [[o_input_cachelevel_pop selectedItem] tag] );
        CaC( "udp-caching" );
        if( module_Exists (p_intf, "dvdread" ) )
        {
            CaC( "dvdread-caching" );
            i = i + config_SaveConfigFile( p_intf, "dvdread" );
        }
        if( module_Exists (p_intf, "dvdnav" ) )
        {
            CaC( "dvdnav-caching" );
            i = i + config_SaveConfigFile( p_intf, "dvdnav" );
        }
        CaC( "tcp-caching" ); CaC( "vcd-caching" );
        CaC( "fake-caching" ); CaC( "cdda-caching" ); CaC( "file-caching" );
        CaC( "screen-caching" );
        CaCi( "rtsp-caching", 4 ); CaCi( "ftp-caching", 2 );
        CaCi( "http-caching", 4 );
        if( module_Exists (p_intf, "access_realrtsp" ) )
        {
            CaCi( "realrtsp-caching", 10 );
            i = i + config_SaveConfigFile( p_intf, "access_realrtsp" );
        }
        CaCi( "mms-caching", 19 );

        #define SaveAccessFilter( object, name ) \
        if( [object state] == NSOnState ) \
        { \
            if( b_first ) \
            { \
                [o_temp appendString: name]; \
                b_first = NO; \
            } \
            else \
                [o_temp appendFormat: @":%@", name]; \
        }

        BOOL b_first = YES;
        NSMutableString *o_temp = [[NSMutableString alloc] init];
        SaveAccessFilter( o_input_record_ckb, @"record" );
        SaveAccessFilter( o_input_dump_ckb, @"dump" );
        SaveAccessFilter( o_input_bandwidth_ckb, @"bandwidth" );
        SaveAccessFilter( o_input_timeshift_ckb, @"timeshift" );
        config_PutPsz( p_intf, "access-filter", [o_temp UTF8String] );
        [o_temp release];

        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "ffmpeg" );
        i = i + config_SaveConfigFile( p_intf, "access_http" );
        i = i + config_SaveConfigFile( p_intf, "access_file" );
        i = i + config_SaveConfigFile( p_intf, "access_tcp" );
        i = i + config_SaveConfigFile( p_intf, "access_fake" );
        i = i + config_SaveConfigFile( p_intf, "cdda" );
        i = i + config_SaveConfigFile( p_intf, "screen" );
        i = i + config_SaveConfigFile( p_intf, "vcd" );
        i = i + config_SaveConfigFile( p_intf, "access_ftp" );
        i = i + config_SaveConfigFile( p_intf, "access_mms" );
        i = i + config_SaveConfigFile( p_intf, "live555" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occured while saving the Input settings using SimplePrefs" );
            i = 0;
        }
        b_inputSettingChanged = NO;
    }
    
    /**********************
     * subtitles settings *
     **********************/
    if( b_osdSettingChanged )
    {
        config_PutInt( p_intf, "osd", [o_osd_osd_ckb state] );

        if( [o_osd_encoding_pop indexOfSelectedItem] >= 0 )
            config_PutPsz( p_intf, "subsdec-encoding", [[[o_osd_encoding_pop selectedItem] title] UTF8String] );

        config_PutPsz( p_intf, "sub-language", [[o_osd_lang_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "freetype-font", [[o_osd_font_fld stringValue] UTF8String] );

        SaveIntList( o_osd_font_color_pop, "freetype-color" );
        SaveIntList( o_osd_font_size_pop, "freetype-rel-fontsize" );
        SaveIntList( o_osd_font_effect_pop, "freetype-effect" );

        i = config_SaveConfigFile( p_intf, NULL );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occured while saving the OSD/Subtitle settings using SimplePrefs" );
            i = 0;
        }
        b_osdSettingChanged = NO;
    }
    
    /********************
     * hotkeys settings *
     ********************/
}

- (void)showSettingsForCategory: (id)o_new_category_view
{
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

- (IBAction)videoSettingChanged:(id)sender
{
    if( sender == o_video_snap_folder_btn )
    {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: YES];
        [o_selectFolderPanel setCanChooseFiles: NO];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the folder to save your video snapshots to.")];
        [o_selectFolderPanel setCanCreateDirectories: YES];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel beginSheetForDirectory: nil file: nil modalForWindow: o_sprefs_win 
                                      modalDelegate: self 
                                     didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:)
                                        contextInfo: o_video_snap_folder_btn];
    }
    else
        b_videoSettingChanged = YES;
}

- (void)savePanelDidEnd:(NSOpenPanel * )panel returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    if( returnCode == NSOKButton )
    {
        if( contextInfo == o_video_snap_folder_btn )
        {
            [o_video_snap_folder_fld setStringValue: [o_selectFolderPanel filename]];
            b_videoSettingChanged = YES;
        }
        else if( contextInfo == o_osd_font_btn )
        {
            [o_osd_font_fld setStringValue: [o_selectFolderPanel filename]];
            b_osdSettingChanged = YES;
        }
    }

    [o_selectFolderPanel release];
}

- (void)showVideoSettings
{
    msg_Dbg( p_intf, "showing video settings" );
    [self showSettingsForCategory: o_video_view];
}

- (IBAction)osdSettingChanged:(id)sender
{
    if( sender == o_osd_font_btn )
    {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: NO];
        [o_selectFolderPanel setCanChooseFiles: YES];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the font to display your Subtitles with.")];
        [o_selectFolderPanel setCanCreateDirectories: NO];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel setAllowedFileTypes: [NSArray arrayWithObjects: @"dfont", @"ttf", @"otf", @"FFIL", nil]];
        [o_selectFolderPanel beginSheetForDirectory: @"/System/Library/Fonts/" file: nil modalForWindow: o_sprefs_win 
                                      modalDelegate: self 
                                     didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:)
                                        contextInfo: o_osd_font_btn];
    }
    else
        b_osdSettingChanged = YES;
}

- (void)showOSDSettings
{
    msg_Dbg( p_intf, "showing OSD settings" );
    [self showSettingsForCategory: o_osd_view];
}

- (IBAction)inputSettingChanged:(id)sender
{
    b_inputSettingChanged = YES;
}

- (void)showInputSettings
{
    msg_Dbg( p_intf, "showing Input Settings" );
    [self showSettingsForCategory: o_input_view];
}

@end
