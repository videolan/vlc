/*****************************************************************************
 * AudioEffects.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jérôme Decoodt <djc@videolan.org>
 *          
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

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import "AudioEffects.h"
#import "intf.h"
#import "../../audio_filter/equalizer_presets.h"

#import <vlc_common.h>
#import <vlc_aout.h>

#import <math.h>

#pragma mark -
#pragma mark Initialization & Generic code

@implementation VLCAudioEffects
static VLCAudioEffects *_o_sharedInstance = nil;

+ (VLCAudioEffects *)sharedInstance
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

- (void)awakeFromNib
{
    /* setup the user's language */
    /* Equalizer */
    [o_eq_enable_ckb setTitle:_NS("Enable")];
    [o_eq_twopass_ckb setTitle:_NS("2 Pass")];
    [o_eq_preamp_lbl setStringValue:_NS("Preamp")];
    [o_eq_presets_popup removeAllItems];
    for( int i = 0; i < 18 ; i++ )
        [o_eq_presets_popup insertItemWithTitle: _NS(preset_list_text[i]) atIndex: i];
    
    /* Compressor */
    [o_comp_enable_ckb setTitle:_NS("Enable dynamic range compressor")];
    [o_comp_band1_lbl setStringValue:_NS("RMS/peak")];;
    [o_comp_band2_lbl setStringValue:_NS("Attack")];
    [o_comp_band3_lbl setStringValue:_NS("Release")];
    [o_comp_band4_lbl setStringValue:_NS("Threshold")];
    [o_comp_band5_lbl setStringValue:_NS("Ratio")];
    [o_comp_band6_lbl setStringValue:_NS("Knee radius")];
    [o_comp_band7_lbl setStringValue:_NS("Makeup gain")];
    
    /* Spatializer */
    [o_spat_enable_ckb setTitle:_NS("Enable Spatializer")];
    [o_spat_band1_lbl setStringValue:_NS("Size")];
    [o_spat_band2_lbl setStringValue:_NS("Width")];
    [o_spat_band3_lbl setStringValue:_NS("Wet")];
    [o_spat_band4_lbl setStringValue:_NS("Dry")];
    [o_spat_band5_lbl setStringValue:_NS("Dump")];
    
    /* Filter */
    [o_filter_headPhone_ckb setTitle:_NS("Headphone virtualization")];
    [o_filter_normLevel_ckb setTitle:_NS("Volume normalization")];
    [o_filter_normLevel_lbl setStringValue:_NS("Maximum level")];
    
    /* generic */
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"equalizer"]] setLabel:_NS("Equalizer")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"compressor"]] setLabel:_NS("Compressor")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"spatializer"]] setLabel:_NS("Spatializer")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"filter"]] setLabel:_NS("Filter")];
    [o_window setTitle:_NS("Audio Effects")];
    [o_window setExcludedFromWindowsMenu:YES];

    [self setupEqualizer];
}

- (IBAction)toggleWindow:(id)sender
{
    if( [o_window isVisible] )
        [o_window orderOut:sender];
    else
        [o_window makeKeyAndOrderFront:sender];
}

#pragma mark -
#pragma mark Equalizer
static bool GetFiltersStatus( intf_thread_t *p_intf,
                             char *psz_name )
{
    char *psz_parser, *psz_string = NULL;
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));
    
    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
        psz_string = config_GetPsz( p_intf, "audio-filter" );
    
    if(! psz_string )
        psz_string = var_GetNonEmptyString( p_object, "audio-filter" );
    
    vlc_object_release( p_object );
    
    if( !psz_string ) return false;
    
    psz_parser = strstr( psz_string, psz_name );
    
    free( psz_string );
    
    if ( psz_parser )
        return true;
    else
        return false;
}

- (void)setupEqualizer
{
    int i;
    vlc_object_t *p_object= VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( VLCIntf ));
    
    if( p_object )
    {
        char *psz_preset;
        
        var_Create( p_object, "equalizer-preset", VLC_VAR_STRING |
                   VLC_VAR_DOINHERIT );
        psz_preset = var_GetNonEmptyString( p_object, "equalizer-preset" );
        
        for( i = 0 ; (psz_preset != NULL) && (i < 18) ; i++ )
        {
            if( strcmp( preset_list[i], psz_preset ) )
                continue;
            
            [o_eq_presets_popup selectItemAtIndex: i];
            
            
            [o_eq_preamp_sld setFloatValue: eqz_preset_10b[i]->f_preamp];
            [self setBandSlidersValues: (float *)eqz_preset_10b[i]->f_amp];
            
            if( strcmp( psz_preset, "flat" ) )
            {
                char psz_bands[100];
                
                snprintf( psz_bands, sizeof( psz_bands ),
                         "%.1f %.1f %.1f %.1f %.1f %.1f %.1f "
                         "%.1f %.1f %.1f",
                         eqz_preset_10b[i]->f_amp[0],
                         eqz_preset_10b[i]->f_amp[1],
                         eqz_preset_10b[i]->f_amp[2],
                         eqz_preset_10b[i]->f_amp[3],
                         eqz_preset_10b[i]->f_amp[4],
                         eqz_preset_10b[i]->f_amp[5],
                         eqz_preset_10b[i]->f_amp[6],
                         eqz_preset_10b[i]->f_amp[7],
                         eqz_preset_10b[i]->f_amp[8],
                         eqz_preset_10b[i]->f_amp[9] );
                
                var_Create( p_object, "equalizer-preamp", VLC_VAR_FLOAT |
                           VLC_VAR_DOINHERIT );
                var_Create( p_object, "equalizer-bands", VLC_VAR_STRING |
                           VLC_VAR_DOINHERIT );
                var_SetFloat( p_object, "equalizer-preamp",
                             eqz_preset_10b[i]->f_preamp );
                var_SetString( p_object, "equalizer-bands", psz_bands );
            }
        }
        free( psz_preset );
        vlc_object_release( p_object );
    }
    
    [self equalizerUpdated];
}

- (void)equalizerUpdated
{
    intf_thread_t *p_intf = VLCIntf;
    float f_preamp, f_band[10];
    char *psz_bands, *psz_bands_init, *p_next;
    bool b_2p;
    int i;
    bool b_enabled = GetFiltersStatus( p_intf, (char *)"equalizer" );
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));
    
    var_Create( p_object, "equalizer-preamp", VLC_VAR_FLOAT |
               VLC_VAR_DOINHERIT );
    var_Create( p_object, "equalizer-bands", VLC_VAR_STRING |
               VLC_VAR_DOINHERIT );
    
    psz_bands = var_GetNonEmptyString( p_object, "equalizer-bands" );
    
    if( psz_bands == NULL )
        psz_bands = strdup( "0 0 0 0 0 0 0 0 0 0" );
    
    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        b_2p = (BOOL)config_GetInt( p_object, "equalizer-2pass" );
        f_preamp = config_GetFloat( p_object, "equalizer-preamp" );
    }
    else
    {
        b_2p = var_GetBool( p_object, "equalizer-2pass" );
        f_preamp = var_GetFloat( p_object, "equalizer-preamp" );
    }
    
    vlc_object_release( p_object );
    
    /* Set the preamp slider */
    [o_eq_preamp_sld setFloatValue: f_preamp];

    /* Set the bands slider */
    psz_bands_init = psz_bands;
    
    for( i = 0; i < 10; i++ )
    {
        /* Read dB -20/20 */
        f_band[i] = strtof( psz_bands, &p_next );
        if( !p_next || p_next == psz_bands ) break; /* strtof() failed */
        
        if( !*psz_bands ) break; /* end of line */
        psz_bands = p_next+1;
    }
    free( psz_bands_init );
    [self setBandSlidersValues:f_band];
    
    /* Set the the checkboxes */
    [o_eq_enable_ckb setState: b_enabled];
    [o_eq_twopass_ckb setState: b_2p];
}

- (id)sliderByIndex:(int)index
{
    switch(index)
    {
        case 0 : return o_eq_band1_sld;
        case 1 : return o_eq_band2_sld;
        case 2 : return o_eq_band3_sld;
        case 3 : return o_eq_band4_sld;
        case 4 : return o_eq_band5_sld;
        case 5 : return o_eq_band6_sld;
        case 6 : return o_eq_band7_sld;
        case 7 : return o_eq_band8_sld;
        case 8 : return o_eq_band9_sld;
        case 9 : return o_eq_band10_sld;
        default : return nil;
    }
}

- (void)setBandSlidersValues:(float *)values
{
    for (int i = 0 ; i<= 9 ; i++)
        [self setValue:values[i] forSlider:i];
}

- (void)initBandSliders
{
    for (int i = 0 ; i< 9 ; i++)
        [self setValue:0.0 forSlider:i];
}

- (void)setValue:(float)value forSlider:(int)index
{
    id slider = [self sliderByIndex:index];
    
    if (slider != nil)
        [slider setFloatValue:value];
}

- (IBAction)eq_enable:(id)sender
{
    aout_EnableFilter( pl_Get( VLCIntf ), (char *)"equalizer", [sender state]);
}

- (IBAction)eq_bandSliderUpdated:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));
    
    const char *psz_values;
    NSString *preset = [NSString stringWithFormat:@"%.1f ", [o_eq_band1_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band2_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band3_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band4_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band5_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band6_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band7_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band8_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f ", [o_eq_band9_sld floatValue] ];
    preset = [preset stringByAppendingFormat:@"%.1f", [o_eq_band10_sld floatValue] ];

    psz_values = [preset UTF8String];
    var_SetString( p_object, "equalizer-bands", psz_values );
    
    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        /* save changed to config */
        config_PutPsz( p_intf, "equalizer-bands", psz_values );
        
        /* save to vlcrc */
        config_SaveConfigFile( p_intf, "equalizer" );
    }
    
    vlc_object_release( p_object );
}
- (IBAction)eq_changePreset:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    int i;
    vlc_object_t *p_object= VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));
    
    var_SetString( p_object , "equalizer-preset" , preset_list[[sender indexOfSelectedItem]] );
    
    NSString *preset = @"";
    const char *psz_values;
    for( i = 0; i < 10; i++ )
    {
        preset = [preset stringByAppendingFormat:@"%.1f ", eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[i] ];
    }
    psz_values = [preset UTF8String];
    var_SetString( p_object, "equalizer-bands", psz_values );
    var_SetFloat( p_object, "equalizer-preamp", eqz_preset_10b[[sender indexOfSelectedItem]]->f_preamp);
    
    [o_eq_preamp_sld setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_preamp];
    
    [self setBandSlidersValues:(float *)eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp];
    
    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        /* save changed to config */
        config_PutPsz( p_intf, "equalizer-bands", psz_values );
        config_PutFloat( p_intf, "equalizer-preamp", eqz_preset_10b[[sender indexOfSelectedItem]]->f_preamp );
        config_PutPsz( p_intf, "equalizer-preset", preset_list[[sender indexOfSelectedItem]] );
        
        /* save to vlcrc */
        config_SaveConfigFile( p_intf, "equalizer" );
    }
    
    vlc_object_release( p_object );
}
- (IBAction)eq_preampSliderUpdated:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    float f_preamp = [sender floatValue] ;

    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));

    var_SetFloat( p_object, "equalizer-preamp", f_preamp );

    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        /* save changed to config */
        config_PutFloat( p_intf, "equalizer-preamp", f_preamp );

        /* save to vlcrc */
        config_SaveConfigFile( p_intf, "equalizer" );
    }

    vlc_object_release( p_object );
}
- (IBAction)eq_twopass:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    bool b_2p = [sender state] ? true : false;
    aout_instance_t *p_aout = getAout();
    vlc_object_t *p_object= VLC_OBJECT(p_aout);
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));
    
    var_SetBool( p_object, "equalizer-2pass", b_2p );
    
    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        /* save changed to config */
        config_PutInt( p_intf, "equalizer-2pass", (int)b_2p );
        
        /* save to vlcrc */
        config_SaveConfigFile( p_intf, "equalizer" );
    }
    
    vlc_object_release( p_object );
}

#pragma mark -
#pragma mark Compressor


#pragma mark -
#pragma mark Spatializer


#pragma mark -
#pragma mark Filter

@end
