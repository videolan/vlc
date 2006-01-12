/*****************************************************************************
 * sfilter.m: MacOS X Subpicture filters dialogue
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id:$
 *
 * Authors: Felix KŸhne <fkuehne@users.sf.net>
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


/*****************************************************************************
 * Note: 
 * the code used to bind with VLC's core is partially based upon the 
 * RC-interface, written by Antoine Cellerier and Mark F. Moriarty  
 * (members of the VideoLAN team) 
 *****************************************************************************/

#import "sfilters.h"
#import "intf.h"
#import <vlc/vout.h>

/* TODO:
    - check for memory leaks
    - save the preferences, if requested
    - fix stupid compilation warnings
*/

@implementation VLCsFilters

static VLCsFilters *_o_sharedInstance = nil;

+ (VLCsFilters *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [o_colors release];

    [super dealloc];
}

- (void)initStrings
{
    [o_sfilter_win setTitle: _NS("Subpicture Filters")];
    [[o_sfilter_tabView tabViewItemAtIndex: 0] setLabel: _NS("Logo")];
    [[o_sfilter_tabView tabViewItemAtIndex: 1] setLabel: _NS("Time")];
    [[o_sfilter_tabView tabViewItemAtIndex: 2] setLabel: _NS("Marquee")];
    [o_sfilter_saveSettings_ckb setTitle: _NS("Save settings")];
    [o_logo_image_btn setTitle: _NS("Browse...")];
    [o_logo_enabled_ckb setTitle: _NS("Enabled")];
    [o_logo_image_lbl setStringValue: [_NS("Image") \
        stringByAppendingString: @":"]];
    [o_logo_pos_lbl setStringValue: [_NS("Position") \
        stringByAppendingString: @":"]];
    [o_logo_opaque_lbl setStringValue: [_NS("Opaqueness") \
        stringByAppendingString: @":"]];
    [o_time_enabled_ckb setTitle: _NS("Enabled")];
    [o_time_stamp_lbl setStringValue: [_NS("Timestamp") \
        stringByAppendingString: @":"]];
    [o_time_size_lbl setStringValue: [_NS("Size") \
        stringByAppendingString: @":"]];
    [o_time_color_lbl setStringValue: [_NS("Color") \
        stringByAppendingString: @":"]];
    [o_time_opaque_lbl setStringValue: [_NS("Opaqueness") \
        stringByAppendingString: @":"]];
    [o_time_pos_lbl setStringValue: [_NS("Position") \
        stringByAppendingString: @":"]];
    [o_time_size_inPx_lbl setStringValue: _NS("(in pixels)")];
    [o_marq_enabled_ckb setTitle: _NS("Enabled")];
    [o_marq_color_lbl setStringValue: [_NS("Color") \
        stringByAppendingString: @":"]];
    [o_marq_marq_lbl setStringValue: [_NS("Marquee") \
        stringByAppendingString: @":"]];
    [o_marq_opaque_lbl setStringValue: [_NS("Opaqueness") \
        stringByAppendingString: @":"]];
    [o_marq_tmOut_lbl setStringValue: [_NS("Timeout") \
        stringByAppendingString: @":"]];
    [o_marq_tmOut_ms_lbl setStringValue: _NS("ms")];
    [o_marq_pos_lbl setStringValue: [_NS("Position") \
        stringByAppendingString: @":"]];
    [o_marq_size_lbl setStringValue: [_NS("Size") \
        stringByAppendingString: @":"]];
    [o_time_color_lbl setStringValue: _NS("(in pixels)")];
}

- (void)awakeFromNib
{
    /* colors as implemented at the beginning of marq.c and time.c
     * feel free to add more colors, but remember to add them to these files 
     * as well to keep a certain level of consistency across the interfaces */
    NSArray * o_default;
    NSArray * o_black;
    NSArray * o_gray;
    NSArray * o_silver;
    NSArray * o_white;
    NSArray * o_maroon;
    NSArray * o_red;
    NSArray * o_fuchsia;
    NSArray * o_yellow;
    NSArray * o_olive;
    NSArray * o_green;
    NSArray * o_teal;
    NSArray * o_lime;
    NSArray * o_purple;
    NSArray * o_navy;
    NSArray * o_blue;
    NSArray * o_aqua;
    o_default = [NSArray arrayWithObjects: _NS("Default"), @"-1", nil];
    o_black = [NSArray arrayWithObjects: _NS("Black"), @"0x000000", nil];
    o_gray = [NSArray arrayWithObjects: _NS("Gray"), @"0x808080", nil];
    o_silver = [NSArray arrayWithObjects: _NS("Silver"), @"0xC0C0C0", nil];
    o_white = [NSArray arrayWithObjects: _NS("White"), @"0xFFFFFF", nil];
    o_maroon = [NSArray arrayWithObjects: _NS("Maroon"), @"0x800000", nil];
    o_red = [NSArray arrayWithObjects: _NS("Red"), @"0xFF0000", nil];
    o_fuchsia = [NSArray arrayWithObjects: _NS("Fuchsia"), @"0xFF00FF", nil];
    o_yellow = [NSArray arrayWithObjects: _NS("Yellow"), @"0xFFFF00", nil];
    o_olive = [NSArray arrayWithObjects: _NS("Olive"), @"0x808000", nil];
    o_green = [NSArray arrayWithObjects: _NS("Green"), @"0x008000", nil];
    o_teal = [NSArray arrayWithObjects: _NS("Teal"), @"0x008080", nil];
    o_lime = [NSArray arrayWithObjects: _NS("Lime"), @"0x00FF00", nil];
    o_purple = [NSArray arrayWithObjects: _NS("Purple"), @"0x800080", nil];
    o_navy = [NSArray arrayWithObjects: _NS("Navy"), @"0x000080", nil];
    o_blue = [NSArray arrayWithObjects: _NS("Blue"), @"0x0000FF", nil];
    o_aqua = [NSArray arrayWithObjects: _NS("Aqua"), @"0x00FFFF", nil];
    o_colors = [[NSArray alloc] initWithObjects: o_default, o_black, o_gray, \
        o_silver, o_white, o_maroon, o_red, o_fuchsia, o_yellow, o_olive, \
        o_green, o_teal, o_lime, o_purple, o_navy, o_blue, o_aqua, nil];

    unsigned int x = 0;
    [o_marq_color_pop removeAllItems];
    [o_time_color_pop removeAllItems];
    
    /* we are adding tags to the items, so we can easily identify them even if 
     * the menu was sorted */
    while (x != [o_colors count])
    {
        [o_marq_color_pop addItemWithTitle: [[o_colors objectAtIndex:x] \
            objectAtIndex:0]];
        [[o_marq_color_pop lastItem] setTag: x];
        
        [o_time_color_pop addItemWithTitle: [[o_colors objectAtIndex:x] \
            objectAtIndex:0]];
        [[o_time_color_pop lastItem] setTag: x];
        
        x = (x + 1);
    }

    [o_marq_color_pop selectItemAtIndex:0];
    [o_time_color_pop selectItemAtIndex:0];

    /* define the relative positions and copy them to the menues
     * we can destroy the array afterwards, because we are saving the ints 
     * as tags to the menu-items */
    NSArray * o_cnt_cnt;
    NSArray * o_lft_cnt;
    NSArray * o_rht_cnt;
    NSArray * o_cnt_top;
    NSArray * o_lft_top;
    NSArray * o_rht_top;
    NSArray * o_cnt_btm;
    NSArray * o_lft_btm;
    NSArray * o_rht_btm;
    NSArray * o_positions;
    o_cnt_cnt = [NSArray arrayWithObjects: _NS("Center-Center"), @"0", nil];
    o_lft_cnt = [NSArray arrayWithObjects: _NS("Left-Center"), @"1", nil];
    o_rht_cnt = [NSArray arrayWithObjects: _NS("Right-Center"), @"2", nil];
    o_cnt_top = [NSArray arrayWithObjects: _NS("Center-Top"), @"4", nil];
    o_lft_top = [NSArray arrayWithObjects: _NS("Left-Top"), @"5", nil];
    o_rht_top = [NSArray arrayWithObjects: _NS("Right-Top"), @"6", nil];
    o_cnt_btm = [NSArray arrayWithObjects: _NS("Center-Bottom"), @"8", nil];
    o_lft_btm = [NSArray arrayWithObjects: _NS("Left-Bottom"), @"9", nil];
    o_rht_btm = [NSArray arrayWithObjects: _NS("Right-Bottom"), @"10", nil];
    o_positions = [[NSArray alloc] initWithObjects: o_cnt_cnt, o_lft_cnt, \
        o_rht_cnt, o_cnt_top, o_lft_top, o_rht_top, o_cnt_btm, o_lft_btm, \
        o_rht_btm, nil];
        
    x = 0;
    [o_time_pos_rel_pop removeAllItems];
    [o_marq_pos_rel_pop removeAllItems];
    [o_logo_pos_rel_pop removeAllItems];
    
    /* we are adding a tag here, so we can easily select an item later on */
    while ( x != [o_positions count] )
    {
        [o_time_pos_rel_pop addItemWithTitle: [[o_positions objectAtIndex:x] \
            objectAtIndex:0]];
        [[o_time_pos_rel_pop lastItem] setTag: [[[o_positions objectAtIndex:x] \
            objectAtIndex:1] intValue]];
        [o_marq_pos_rel_pop addItemWithTitle: [[o_positions objectAtIndex:x] \
            objectAtIndex:0]];
        [[o_marq_pos_rel_pop lastItem] setTag: [[[o_positions objectAtIndex:x] \
            objectAtIndex:1] intValue]];
        [o_logo_pos_rel_pop addItemWithTitle: [[o_positions objectAtIndex:x] \
            objectAtIndex:0]];
        [[o_logo_pos_rel_pop lastItem] setTag: [[[o_positions objectAtIndex:x] \
            objectAtIndex:1] intValue]];

        x = (x + 1);
    }
    [o_positions release];

    NSArray * o_sizes;
    o_sizes = [[NSArray alloc] initWithObjects: @"6", @"8", @"10", @"11", @"12",\
        @"14", @"13", @"16", @"18", @"24", @"36", @"48", @"64", @"72", @"96", \
        @"144", @"288", nil];
    [o_marq_size_pop removeAllItems];
    [o_marq_size_pop addItemsWithTitles: o_sizes];
    [o_time_size_pop removeAllItems];
    [o_time_size_pop addItemsWithTitles: o_sizes];
    [o_sizes release];
}

- (void)showAsPanel
{
    /* called from intf.m */
    [o_sfilter_win displayIfNeeded];
    [o_sfilter_win makeKeyAndOrderFront:nil];

    intf_thread_t * p_intf = VLCIntf;

    /* retrieve the marquee settings */
    int x = 0;
    int tempInt = config_GetInt( p_intf, "marq-color" );
    while( strtol([[[o_colors objectAtIndex:x] objectAtIndex:1] UTF8String], \
        NULL, 0) != tempInt )
    {
        x = (x + 1);
        
        if( x >= [o_marq_color_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    }
    [o_marq_color_pop selectItemAtIndex: x];
    [o_marq_marq_fld setStringValue: [NSString stringWithUTF8String: \
        config_GetPsz( p_intf, "marq-marquee" )]];
    [o_marq_opaque_sld setIntValue: config_GetInt( p_intf, "marq-opacity")];
    x = 0;
    tempInt = config_GetInt( p_intf, "marq-position" );
    while( tempInt != [[o_marq_pos_rel_pop itemAtIndex:x] tag] )
    {
        x = (x + 1);
        
        if( x >= [o_marq_pos_rel_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    }
    [o_marq_pos_rel_pop selectItemAtIndex:x];
    x = 0;
    tempInt = config_GetInt( p_intf, "marq-size" );
    while( [[[o_marq_size_pop itemAtIndex: x] title] intValue] != tempInt )
        x = (x + 1);
        
        if( x >= [o_marq_size_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    [o_marq_size_pop selectItemAtIndex: x];
    [o_marq_tmOut_fld setStringValue: [[NSNumber numberWithInt: \
        config_GetInt( p_intf, "marq-timeout" )] stringValue]];
    
    /* retrieve the time settings */
    x = 0;
    tempInt = config_GetInt( p_intf, "time-color" );
    while( strtol([[[o_colors objectAtIndex:x] objectAtIndex:1] UTF8String], \
        NULL, 0) != tempInt )
    {
        x = (x + 1);
        
        if( x >= [o_time_color_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    }
    [o_time_color_pop selectItemAtIndex: x];
    [o_time_stamp_fld setStringValue: [NSString stringWithUTF8String: \
        config_GetPsz( p_intf, "time-format" )]];
    [o_time_opaque_sld setIntValue: config_GetInt( p_intf, "time-opacity")];
    x = 0;
    tempInt = config_GetInt( p_intf, "time-size" );
    while( [[[o_time_size_pop itemAtIndex: x] title] intValue] != tempInt )
        x = (x + 1);
        
        if( x >= [o_time_size_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    [o_time_size_pop selectItemAtIndex: x];
    x = 0;
    tempInt = config_GetInt( p_intf, "time-position" );
    while( tempInt != [[o_time_pos_rel_pop itemAtIndex:x] tag] )
    {
        x = (x + 1);
        
        if( x >= [o_time_pos_rel_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    }
    
    /* retrieve the logo settings */
    [o_logo_opaque_sld setIntValue: config_GetInt( p_intf, "logo-transparency")];
    /* in case that no path has been saved yet */
    NSString * tempString = [[NSString alloc] initWithUTF8String: \
        config_GetPsz( p_intf, "logo-file" )];
    if( [tempString length] == 0 )
    {
        [o_logo_image_fld setStringValue: @""];
    }
    else
    {
        [o_logo_image_fld setStringValue: tempString ];
    }
    [tempString release];
    x = 0;
    tempInt = config_GetInt( p_intf, "logo-position" );
    while( tempInt != [[o_logo_pos_rel_pop itemAtIndex:x] tag] )
    {
        x = (x + 1);
        
        if( x >= [o_logo_pos_rel_pop numberOfItems] )
        {
            x = 0;
            return;
        }
    }
    
    /* enable the wanted filters */
    char * psz_subfilters;
    psz_subfilters = config_GetPsz( p_intf, "sub-filter" );
    if( psz_subfilters )
    {
        [o_marq_enabled_ckb setState: (int)strstr( psz_subfilters, "marq")];
        [o_logo_enabled_ckb setState: (int)strstr( psz_subfilters, "logo")];
        [o_time_enabled_ckb setState: (int)strstr( psz_subfilters, "time")];
        [self enableMarq];
        [self enableLogo];
        [self enableTime];
    }
}

- (IBAction)logo_selectFile:(id)sender
{
    NSOpenPanel * openPanel = [NSOpenPanel openPanel];
    SEL sel = @selector(logo_getFile:returnCode:contextInfo:);
    [openPanel beginSheetForDirectory:nil file:nil types: [NSArray \
        arrayWithObjects: @"png", @"PNG", @"'PNGf'", nil] modalForWindow: \
        o_sfilter_win modalDelegate:self didEndSelector:sel contextInfo:nil];
}

- (void)logo_getFile: (NSOpenPanel *)sheet returnCode: \
    (int)returnCode contextInfo: (void *)contextInfo
{
    if (returnCode == NSOKButton)
    {
        [o_logo_image_fld setStringValue: [sheet filename]];
    }
}

- (IBAction)propertyChanged:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );

    vlc_value_t val;

    /* general properties */
    if( sender == o_sfilter_saveSettings_ckb)
    {
        o_save_settings = [o_sfilter_saveSettings_ckb state]; 
    }

    /* marquee */
    else if( sender == o_marq_marq_fld )
    {
        if( [[o_marq_marq_fld stringValue] length] == 0 )
        {
            val.psz_string = "";
        }
        else
        {
            val.psz_string = [[o_marq_marq_fld stringValue] UTF8String];
        }

        if( p_input )
            var_Set( p_input->p_libvlc, "marq-marquee", val );

        config_PutPsz( p_intf, "marq-marquee", val.psz_string );
    }
    
    else if( sender == o_marq_pos_rel_pop )
    {
        val.i_int = [[o_marq_pos_rel_pop selectedItem] tag];

        if( p_input )
            var_Set( p_input->p_libvlc, "marq-position", val );

        config_PutInt( p_intf, "marq-position", val.i_int );
    }
    
    else if( sender == o_marq_color_pop )
    {
        val.i_int = strtol( [[[o_colors objectAtIndex: [o_marq_color_pop \
            indexOfSelectedItem]] objectAtIndex: 1] UTF8String], NULL, 0 );

        if( p_input )
            var_Set( p_input->p_libvlc, "marq-color", val );

        config_PutInt( p_intf, "marq-color", val.i_int );
    }
    
    else if( sender == o_marq_opaque_sld )
    {
        val.i_int = [o_marq_opaque_sld intValue];

        if( p_input )
            var_Set( p_input->p_libvlc, "marq-opacity", val );

        config_PutInt( p_intf, "marq-opacity", val.i_int );
    }
    
    else if( sender == o_marq_size_pop )
    {
        val.i_int = [[o_marq_size_pop titleOfSelectedItem] intValue];

        if( p_input )
            var_Set( p_input->p_libvlc, "marq-size", val );

        config_PutInt( p_intf, "marq-size", val.i_int );
    }
    
    else if( sender == o_marq_tmOut_fld && [[sender stringValue] length] > 0 )
    {
        val.i_int = [o_marq_tmOut_fld intValue];

        if( p_input )
            var_Set( p_input->p_libvlc, "marq-timeout", val );

        config_PutInt( p_intf, "marq-timeout", val.i_int );
    }
    
    /* time */
    
    else if( sender == o_time_stamp_fld )
    {
        if( [[o_time_stamp_fld stringValue] length] == 0 )
        {
            val.psz_string = "";
        }
        else
        {
            val.psz_string = [[o_time_stamp_fld stringValue] UTF8String];
        }

        if( p_input )
            var_Set( p_input->p_libvlc, "time-format", val );

        config_PutPsz( p_intf, "time-format", val.psz_string );
    }

    else if( sender == o_time_pos_rel_pop )
    {
        val.i_int = [[o_time_pos_rel_pop selectedItem] tag];

        if( p_input )
            var_Set( p_input->p_libvlc, "time-position", val );

        config_PutInt( p_intf, "time-position", val.i_int );
    }
    
    else if( sender == o_time_color_pop )
    {
        val.i_int = strtol( [[[o_colors objectAtIndex: [o_time_color_pop \
            indexOfSelectedItem]] objectAtIndex: 1] UTF8String], NULL, 0 );

        if( p_input )
            var_Set( p_input->p_libvlc, "time-color", val );

        config_PutInt( p_intf, "time-color", val.i_int );
    }
    
    else if( sender == o_time_opaque_sld )
    {
        val.i_int = [o_time_opaque_sld intValue];

        if( p_input )
            var_Set( p_input->p_libvlc, "time-opacity", val );

        config_PutInt( p_intf, "time-opacity", val.i_int );
    }
    
    else if( sender == o_time_size_pop )
    {
        val.i_int = [[o_time_size_pop titleOfSelectedItem] intValue];

        if( p_input )
            var_Set( p_input->p_libvlc, "time-size", val );

        config_PutInt( p_intf, "time-size", val.i_int );
    }

    /* logo */
    else if( sender == o_logo_opaque_sld )
    {
        val.i_int = [o_logo_opaque_sld intValue];

        if( p_input )
            var_Set( p_input->p_libvlc, "logo-transparency", val );

        config_PutInt( p_intf, "logo-transparency", val.i_int );
    }
    
    else if( sender == o_logo_pos_rel_pop )
    {
        val.i_int = [[o_logo_pos_rel_pop selectedItem] tag];

        if( p_input )
            var_Set( p_input->p_libvlc, "logo-position", val );

        config_PutInt( p_intf, "logo-position", val.i_int );
    }

    /* clean up */
    if ( p_input )
    {
        o_config_changed = YES;
        vlc_object_release( p_input );
    }
}

- (IBAction)enableFilter:(id)sender
{
    if( sender == o_marq_enabled_ckb )
    {
        if( [o_marq_enabled_ckb state] == NSOnState )
        {
            [self changeFiltersString:"marq" onOrOff:VLC_TRUE];
        }
        else
        {
            [self changeFiltersString:"marq" onOrOff:VLC_FALSE];
        }
        [self enableMarq];
    }
    if( sender == o_logo_enabled_ckb )
    {
        if( [o_logo_enabled_ckb state] == NSOnState )
        {
            [self changeFiltersString:"logo" onOrOff:VLC_TRUE];
        }
        else
        {
            [self changeFiltersString:"logo" onOrOff:VLC_FALSE];
        }
        [self enableLogo];
    }
    if( sender == o_time_enabled_ckb )
    {
        if( [o_time_enabled_ckb state] == NSOnState )
        {
            [self changeFiltersString:"time" onOrOff:VLC_TRUE];
        }
        else
        {
            [self changeFiltersString:"time" onOrOff:VLC_FALSE];
        }
        [self enableTime];
    }    
}

- (void)enableMarq
{
    [o_marq_color_pop setEnabled: [o_marq_enabled_ckb state]];
    [o_marq_marq_fld setEnabled: [o_marq_enabled_ckb state]];
    [o_marq_opaque_sld setEnabled: [o_marq_enabled_ckb state]];
    [o_marq_size_pop setEnabled: [o_marq_enabled_ckb state]];
    [o_marq_tmOut_fld setEnabled: [o_marq_enabled_ckb state]];
    [o_marq_pos_rel_pop setEnabled: [o_marq_enabled_ckb state]];
}

- (void)enableTime
{
    [o_time_color_pop setEnabled: [o_time_enabled_ckb state]];
    [o_time_stamp_fld setEnabled: [o_time_enabled_ckb state]];
    [o_time_opaque_sld setEnabled: [o_time_enabled_ckb state]];
    [o_time_size_pop setEnabled: [o_time_enabled_ckb state]];
    [o_time_pos_rel_pop setEnabled: [o_time_enabled_ckb state]];
}

- (void)enableLogo
{
    [o_logo_image_btn setEnabled: [o_logo_enabled_ckb state]];
    [o_logo_image_fld setEnabled: [o_logo_enabled_ckb state]];
    [o_logo_opaque_sld setEnabled: [o_logo_enabled_ckb state]];
    [o_logo_pos_rel_pop setEnabled: [o_logo_enabled_ckb state]];
}

- (void)changeFiltersString:(char *)psz_name onOrOff:(vlc_bool_t )b_add
{
    /* copied from ../wxwidgets/extrapanel.cpp
     * renamed to conform with Cocoa's rules
     * and slightly modified to suit our needs */

    intf_thread_t * p_intf = VLCIntf;
    
    char *psz_parser, *psz_string;
    psz_string = config_GetPsz( p_intf, "sub-filter" );
    
    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                            psz_string, psz_name );
            free( psz_parser );
        }
        else
        {
            return;
        }
    }
    else
    {
        if( psz_parser )
        {
            memmove( psz_parser, psz_parser + strlen(psz_name) +
                            (*(psz_parser + strlen(psz_name)) == ':' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            /* Remove trailing : : */
            if( *(psz_string+strlen(psz_string ) -1 ) == ':' )
            {
                *(psz_string+strlen(psz_string ) -1 ) = '\0';
            }
         }
         else
         {
             free( psz_string );
             return;
         }
    }
    
    config_PutPsz( p_intf, "sub-filter", psz_string );
    
    /* Try to set on the fly */
    /* FIXME: enable this once we support on-the-fly addition of this kind of
     * filters...
    vout_thread_t *p_vout;
    p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout )
    {
        var_SetString( p_vout, "sub-filter", psz_string );
        vlc_object_release( p_vout );
    }*/

    free( psz_string );

    vlc_object_release( p_intf );

    o_config_changed = YES;
}
@end
