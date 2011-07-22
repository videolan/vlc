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

#import "intf.h"
#import "AudioEffects.h"
#import "../../audio_filter/equalizer_presets.h"

#import <vlc_common.h>
#import <vlc_aout_intf.h>

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
        p_intf = VLCIntf;
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
    [o_comp_reset_btn setTitle:_NS("Reset")];
    [o_comp_band1_lbl setStringValue:_NS("RMS/peak")];;
    [o_comp_band2_lbl setStringValue:_NS("Attack")];
    [o_comp_band3_lbl setStringValue:_NS("Release")];
    [o_comp_band4_lbl setStringValue:_NS("Threshold")];
    [o_comp_band5_lbl setStringValue:_NS("Ratio")];
    [o_comp_band6_lbl setStringValue:_NS("Knee radius")];
    [o_comp_band7_lbl setStringValue:_NS("Makeup gain")];
    
    /* Spatializer */
    [o_spat_enable_ckb setTitle:_NS("Enable Spatializer")];
    [o_spat_reset_btn setTitle:_NS("Reset")];
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
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
}

- (IBAction)toggleWindow:(id)sender
{
    if( [o_window isVisible] )
        [o_window orderOut:sender];
    else
        [o_window makeKeyAndOrderFront:sender];
}

- (void)setAudioFilter: (char *)psz_name on:(BOOL)b_on
{
    char *psz_tmp;
    aout_instance_t * p_aout = getAout();
    if( p_aout )
        psz_tmp = var_GetNonEmptyString( p_aout, "audio-filter" );
    else
        psz_tmp = config_GetPsz( p_intf, "audio-filter" );        
    
    if( b_on )
    {
        if(! psz_tmp)
            config_PutPsz( p_intf, "audio-filter", psz_name );
        else if( (NSInteger)strstr( psz_tmp, psz_name ) == NO )
        {
            psz_tmp = (char *)[[NSString stringWithFormat: @"%s:%s", psz_tmp, psz_name] UTF8String];
            config_PutPsz( p_intf, "audio-filter", psz_tmp );
        }
    } else {
        if( psz_tmp )
        {
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithFormat:@":%s",psz_name]]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithFormat:@"%s:",psz_name]]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithUTF8String:psz_name]]] UTF8String];
            config_PutPsz( p_intf, "audio-filter", psz_tmp );
        }
    }

    if( p_aout ) {
        aout_EnableFilter( pl_Get( p_intf ), psz_name, b_on );
        vlc_object_release( p_aout );
    }
}

#pragma mark -
#pragma mark Equalizer
static bool GetEqualizerStatus( intf_thread_t *p_custom_intf,
                             char *psz_name )
{
    char *psz_parser, *psz_string = NULL;
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_custom_intf ));
    
    if( (BOOL)config_GetInt( p_custom_intf, "macosx-eq-keep" ) == YES )
        psz_string = config_GetPsz( p_custom_intf, "audio-filter" );
    
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
        
        for( i = 0 ; i < NB_PRESETS ; i++ )
        {
            if( strcmp( preset_list[i], psz_preset ) )
                continue;
            
            [o_eq_presets_popup selectItemAtIndex: i];
            
            
            [o_eq_preamp_sld setFloatValue: eqz_preset_10b[i].f_preamp];
            [self setBandSlidersValues: (float *)eqz_preset_10b[i].f_amp];
            
            if( strcmp( psz_preset, "flat" ) )
            {
                char psz_bands[100];
                
                snprintf( psz_bands, sizeof( psz_bands ),
                         "%.1f %.1f %.1f %.1f %.1f %.1f %.1f "
                         "%.1f %.1f %.1f",
                         eqz_preset_10b[i].f_amp[0],
                         eqz_preset_10b[i].f_amp[1],
                         eqz_preset_10b[i].f_amp[2],
                         eqz_preset_10b[i].f_amp[3],
                         eqz_preset_10b[i].f_amp[4],
                         eqz_preset_10b[i].f_amp[5],
                         eqz_preset_10b[i].f_amp[6],
                         eqz_preset_10b[i].f_amp[7],
                         eqz_preset_10b[i].f_amp[8],
                         eqz_preset_10b[i].f_amp[9] );
                
                var_Create( p_object, "equalizer-preamp", VLC_VAR_FLOAT |
                           VLC_VAR_DOINHERIT );
                var_Create( p_object, "equalizer-bands", VLC_VAR_STRING |
                           VLC_VAR_DOINHERIT );
                var_SetFloat( p_object, "equalizer-preamp",
                             eqz_preset_10b[i].f_preamp );
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
    float f_preamp, f_band[10];
    char *psz_bands, *psz_bands_init, *p_next;
    bool b_2p;
    int i;
    bool b_enabled = GetEqualizerStatus( p_intf, (char *)"equalizer" );
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
    [self setAudioFilter: "equalizer" on:[sender state]];
}

- (IBAction)eq_bandSliderUpdated:(id)sender
{
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
    }
    
    vlc_object_release( p_object );
}
- (IBAction)eq_changePreset:(id)sender
{
    vlc_object_t *p_object= VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));
    
    var_SetString( p_object , "equalizer-preset" , preset_list[[sender indexOfSelectedItem]] );
    
    NSString *preset = @"";
    const char *psz_values;
    for( int i = 0; i < EQZ_BANDS_MAX; i++ )
    {
        preset = [preset stringByAppendingFormat:@"%.1f ", eqz_preset_10b[[sender indexOfSelectedItem]].f_amp[i] ];
    }
    psz_values = [preset UTF8String];
    var_SetString( p_object, "equalizer-bands", psz_values );
    var_SetFloat( p_object, "equalizer-preamp", eqz_preset_10b[[sender indexOfSelectedItem]].f_preamp);
    
    [o_eq_preamp_sld setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]].f_preamp];
    
    [self setBandSlidersValues:(float *)eqz_preset_10b[[sender indexOfSelectedItem]].f_amp];
    
    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        /* save changed to config */
        config_PutPsz( p_intf, "equalizer-bands", psz_values );
        config_PutFloat( p_intf, "equalizer-preamp", eqz_preset_10b[[sender indexOfSelectedItem]].f_preamp );
        config_PutPsz( p_intf, "equalizer-preset", preset_list[[sender indexOfSelectedItem]] );
    }
    
    vlc_object_release( p_object );
}
- (IBAction)eq_preampSliderUpdated:(id)sender
{
    float f_preamp = [sender floatValue] ;

    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if( p_object == NULL )
        p_object = vlc_object_hold(pl_Get( p_intf ));

    var_SetFloat( p_object, "equalizer-preamp", f_preamp );

    if( (BOOL)config_GetInt( p_intf, "macosx-eq-keep" ) == YES )
    {
        /* save changed to config */
        config_PutFloat( p_intf, "equalizer-preamp", f_preamp );
    }

    vlc_object_release( p_object );
}
- (IBAction)eq_twopass:(id)sender
{
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
    }
    
    vlc_object_release( p_object );
}

#pragma mark -
#pragma mark Compressor
- (void)resetCompressor
{
    char * psz_afilters;
    psz_afilters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_afilters ) {
        [o_comp_enable_ckb setState: (NSInteger)strstr( psz_afilters, "compressor" ) ];
        free( psz_afilters );
    } 
    else
        [o_comp_enable_ckb setState: NSOffState];

    [o_comp_band1_sld setFloatValue: config_GetFloat( p_intf, "compressor-rms-peak" )];
    [o_comp_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_comp_band1_sld floatValue]]];
    [o_comp_band2_sld setFloatValue: config_GetFloat( p_intf, "compressor-attack" )];
    [o_comp_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [o_comp_band2_sld floatValue]]];
    [o_comp_band3_sld setFloatValue: config_GetFloat( p_intf, "compressor-release" )];
    [o_comp_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [o_comp_band3_sld floatValue]]];
    [o_comp_band4_sld setFloatValue: config_GetFloat( p_intf, "compressor-threshold" )];
    [o_comp_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [o_comp_band4_sld floatValue]]];
    [o_comp_band5_sld setFloatValue: config_GetFloat( p_intf, "compressor-ratio" )];
    [o_comp_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [o_comp_band5_sld floatValue]]];
    [o_comp_band6_sld setFloatValue: config_GetFloat( p_intf, "compressor-knee" )];
    [o_comp_band6_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [o_comp_band6_sld floatValue]]];
    [o_comp_band7_sld setFloatValue: config_GetFloat( p_intf, "compressor-makeup-gain" )];
    [o_comp_band7_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [o_comp_band7_sld floatValue]]];
}

- (IBAction)resetCompressorValues:(id)sender
{
    config_PutFloat( p_intf, "compressor-rms-peak", 0.000000 );
    config_PutFloat( p_intf, "compressor-attack", 25.000000 );
    config_PutFloat( p_intf, "compressor-release", 100.000000 );
    config_PutFloat( p_intf, "compressor-threshold", -11.000000 );
    config_PutFloat( p_intf, "compressor-ratio", 8.000000 );
    config_PutFloat( p_intf, "compressor-knee", 2.500000 );
    config_PutFloat( p_intf, "compressor-makeup-gain", 7.000000 );

    aout_instance_t * p_aout = getAout();
    if (p_aout) {
        var_SetFloat( p_aout, "compressor-rms-peak", 0.000000 );
        var_SetFloat( p_aout, "compressor-attack", 25.000000 );
        var_SetFloat( p_aout, "compressor-release", 100.000000 );
        var_SetFloat( p_aout, "compressor-threshold", -11.000000 );
        var_SetFloat( p_aout, "compressor-ratio", 8.000000 );
        var_SetFloat( p_aout, "compressor-knee", 2.500000 );
        var_SetFloat( p_aout, "compressor-makeup-gain", 7.000000 );
        vlc_object_release( p_aout );
    }
    [self resetCompressor];
}

- (IBAction)comp_enable:(id)sender
{
    [self setAudioFilter:"compressor" on:[sender state]];
}

- (IBAction)comp_sliderUpdated:(id)sender
{
    aout_instance_t * p_aout = getAout();
    char * value;
    if( sender == o_comp_band1_sld )
        value = "compressor-rms-peak";
    else if( sender == o_comp_band2_sld )
        value = "compressor-attack";
    else if( sender == o_comp_band3_sld )
        value = "compressor-release";
    else if( sender == o_comp_band4_sld )
        value = "compressor-threshold";
    else if( sender == o_comp_band5_sld )
        value = "compressor-ratio";
    else if( sender == o_comp_band6_sld )
        value = "compressor-knee";
    else if( sender == o_comp_band7_sld )
        value = "compressor-makeup-gain";

    if( p_aout ) {
        var_SetFloat( p_aout, value, [sender floatValue] );
        vlc_object_release( p_aout );
    }
    config_PutFloat( p_intf, value, [sender floatValue] );

    if( sender == o_comp_band1_sld )
        [o_comp_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if( sender == o_comp_band2_sld )
        [o_comp_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [sender floatValue]]];
    else if( sender == o_comp_band3_sld )
        [o_comp_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [sender floatValue]]];
    else if( sender == o_comp_band4_sld )
        [o_comp_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [sender floatValue]]];
    else if( sender == o_comp_band5_sld )
        [o_comp_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [sender floatValue]]];
    else if( sender == o_comp_band6_sld )
        [o_comp_band6_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [sender floatValue]]];
    else if( sender == o_comp_band7_sld )
        [o_comp_band7_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [sender floatValue]]];
}

#pragma mark -
#pragma mark Spatializer
- (void)resetSpatializer
{
    char * psz_afilters;
    psz_afilters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_afilters ) {
        [o_spat_enable_ckb setState: (NSInteger)strstr( psz_afilters, "spatializer" ) ];
        free( psz_afilters );
    } 
    else
        [o_spat_enable_ckb setState: NSOffState];
    
    [o_spat_band1_sld setFloatValue: config_GetFloat( p_intf, "spatializer-roomsize" )];
    [o_spat_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_spat_band1_sld floatValue]]];
    [o_spat_band2_sld setFloatValue: config_GetFloat( p_intf, "spatializer-width" )];
    [o_spat_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_spat_band2_sld floatValue]]];
    [o_spat_band3_sld setFloatValue: config_GetFloat( p_intf, "spatializer-wet" )];
    [o_spat_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_spat_band3_sld floatValue]]];
    [o_spat_band4_sld setFloatValue: config_GetFloat( p_intf, "spatializer-dry" )];
    [o_spat_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_spat_band4_sld floatValue]]];
    [o_spat_band5_sld setFloatValue: config_GetFloat( p_intf, "spatializer-damp" )];
    [o_spat_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_spat_band5_sld floatValue]]];
}

- (IBAction)resetSpatializerValues:(id)sender
{
    config_PutFloat( p_intf, "spatializer-roomsize", 1.050000 );
    config_PutFloat( p_intf, "spatializer-width", 10.000000 );
    config_PutFloat( p_intf, "spatializer-wet", 3.000000 );
    config_PutFloat( p_intf, "spatializer-dry", 2.000000 );
    config_PutFloat( p_intf, "spatializer-damp", 1.000000 );

    aout_instance_t * p_aout = getAout();
    if (p_aout) {
        var_SetFloat( p_aout, "spatializer-roomsize", 1.050000 );
        var_SetFloat( p_aout, "spatializer-width", 10.000000 );
        var_SetFloat( p_aout, "spatializer-wet", 3.000000 );
        var_SetFloat( p_aout, "spatializer-dry", 2.000000 );
        var_SetFloat( p_aout, "spatializer-damp", 1.000000 );
        vlc_object_release( p_aout );
    }
    [self resetSpatializer];
}

- (IBAction)spat_enable:(id)sender
{
    [self setAudioFilter:"spatializer" on:[sender state]];    
}

- (IBAction)spat_sliderUpdated:(id)sender
{
    aout_instance_t * p_aout = getAout();
    char * value;
    if( sender == o_spat_band1_sld )
        value = "spatializer-roomsize";
    else if( sender == o_spat_band2_sld )
        value = "spatializer-width";
    else if( sender == o_spat_band3_sld )
        value = "spatializer-wet";
    else if( sender == o_spat_band4_sld )
        value = "spatializer-dry";
    else if( sender == o_spat_band5_sld )
        value = "spatializer-damp";
    
    if( p_aout ) {
        var_SetFloat( p_aout, value, [sender floatValue] );
        vlc_object_release( p_aout );
    }
    config_PutFloat( p_intf, value, [sender floatValue] );
    
    if( sender == o_spat_band1_sld )
        [o_spat_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if( sender == o_spat_band2_sld )
        [o_spat_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if( sender == o_spat_band3_sld )
        [o_spat_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if( sender == o_spat_band4_sld )
        [o_spat_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if( sender == o_spat_band5_sld )
        [o_spat_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
}

#pragma mark -
#pragma mark Filter
- (void)resetAudioFilters
{
    char * psz_afilters;
    psz_afilters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_afilters )
    {
        [o_filter_headPhone_ckb setState: (NSInteger)strstr( psz_afilters, "headphone" ) ];
        [o_filter_normLevel_ckb setState: (NSInteger)strstr( psz_afilters, "normvol" ) ];
        free( psz_afilters );
    } else {
        [o_filter_headPhone_ckb setState: NSOffState];
        [o_filter_normLevel_ckb setState: NSOffState];
    }
    [o_filter_normLevel_sld setFloatValue: config_GetFloat( p_intf, "norm-max-level" )];
}

- (IBAction)filter_enableHeadPhoneVirt:(id)sender
{
    [self setAudioFilter: "headphone" on:[sender state]];
}

- (IBAction)filter_enableVolumeNorm:(id)sender
{
    [self setAudioFilter: "normvol" on:[sender state]];    
}

- (IBAction)filter_volNormSliderUpdated:(id)sender
{
    aout_instance_t * p_aout= getAout();

    if( p_aout )
    {
        var_SetFloat( p_aout, "norm-max-level", [o_filter_normLevel_sld floatValue] );
        vlc_object_release( p_aout );
    }

    config_PutFloat( p_intf, "norm-max-level", [o_filter_normLevel_sld floatValue] );
}

@end
