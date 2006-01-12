/*****************************************************************************
 * equalizer.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Jerome Decoodt <djc@videolan.org>
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <aout_internal.h>

#include "intf.h"

#include <math.h>

#include "equalizer.h"
#include "../../audio_filter/equalizer_presets.h"

/*****************************************************************************
 * VLCEqualizer implementation 
 *****************************************************************************/
@implementation VLCEqualizer

static void ChangeFiltersString( intf_thread_t *p_intf,
                                 char *psz_name, vlc_bool_t b_add )
{
    char *psz_parser, *psz_string;
    int i;
    vlc_object_t *p_object = vlc_object_find( p_intf,
                                VLC_OBJECT_AOUT, FIND_ANYWHERE );
    aout_instance_t *p_aout = (aout_instance_t *)p_object;
    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return;

    psz_string = var_GetString( p_object, "audio-filter" );

    if( !psz_string ) psz_string = strdup( "" );

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, ( *psz_string ) ? "%s,%s" : "%s%s",
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
            memmove( psz_parser, psz_parser + strlen( psz_name ) +
                            ( *( psz_parser + strlen( psz_name ) ) == ',' ? 1 : 0 ),
                            strlen( psz_parser + strlen( psz_name ) ) + 1 );

            if( *( psz_string+strlen( psz_string ) - 1 ) == ',' )
            {
                *( psz_string+strlen( psz_string ) - 1 ) = '\0';
            }
         }
         else
         {
             free( psz_string );
             return;
         }
    }

    var_SetString( p_object, "audio-filter", psz_string );
    if( p_aout )
    {
        for( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
    }
    free( psz_string );
    vlc_object_release( p_object );
}

static vlc_bool_t GetFiltersStatus( intf_thread_t *p_intf,
                                 char *psz_name )
{
    char *psz_parser, *psz_string;
    vlc_object_t *p_object = vlc_object_find( p_intf,
                                VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return VLC_FALSE;

    psz_string = var_GetString( p_object, "audio-filter" );

    vlc_object_release( p_object );

    if( !psz_string ) return VLC_FALSE;

    psz_parser = strstr( psz_string, psz_name );

    free( psz_string );

    if ( psz_parser )
        return VLC_TRUE;
    else
        return VLC_FALSE;
}

- (void)initStrings
{
    int i;
    [o_btn_equalizer setToolTip: _NS("Equalizer")];
    [o_ckb_2pass setTitle: _NS("2 Pass")];
    [o_ckb_2pass setToolTip: _NS("If you enable this settting, the "
        "equalizer filter will be applied twice. The effect will be sharper.")];
    [o_ckb_enable setTitle: _NS("Enable")];
    [o_ckb_enable setToolTip: _NS("Enable the equalizer. You can either "
        "manually change the bands or use a preset.")];
    [o_fld_preamp setStringValue: _NS("Preamp")];

    [o_popup_presets removeAllItems];
    for( i = 0; i < 18 ; i++ )
    {
        [o_popup_presets insertItemWithTitle: _NS(preset_list_text[i]) atIndex: i];
    }
    [o_window setTitle: _NS("Equalizer")];

	/*
    [o_slider_band1 setFloatValue: 0];
    [o_slider_band2 setFloatValue: 0];
    [o_slider_band3 setFloatValue: 0];
    [o_slider_band4 setFloatValue: 0];
    [o_slider_band5 setFloatValue: 0];
    [o_slider_band6 setFloatValue: 0];
    [o_slider_band7 setFloatValue: 0];
    [o_slider_band8 setFloatValue: 0];
    [o_slider_band9 setFloatValue: 0];
    [o_slider_band10 setFloatValue: 0];
	*/
	[self initBandSliders];
    [o_ckb_enable setState: NSOffState];
    [o_ckb_2pass setState: NSOffState];
}

- (void)equalizerUpdated
{
    intf_thread_t *p_intf = VLCIntf;
    float f_preamp, f_band[10];
    char *psz_bands, *psz_bands_init, *p_next;
    vlc_bool_t b_2p;
    int i;
    vlc_bool_t b_enabled = GetFiltersStatus( p_intf, "equalizer" );
    vlc_object_t *p_object = vlc_object_find( p_intf,
                                VLC_OBJECT_AOUT, FIND_ANYWHERE );

    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return;

    var_Create( p_object, "equalizer-preamp", VLC_VAR_FLOAT |
                                                    VLC_VAR_DOINHERIT );
    var_Create( p_object, "equalizer-bands", VLC_VAR_STRING |
                                                    VLC_VAR_DOINHERIT );

    f_preamp = var_GetFloat( p_object, "equalizer-preamp" );
    psz_bands = var_GetString( p_object, "equalizer-bands" );

    if( !strcmp( psz_bands, "" ) )
        psz_bands = strdup( "0 0 0 0 0 0 0 0 0 0" );

    b_2p = var_GetBool( p_object, "equalizer-2pass" );

    vlc_object_release( p_object );

/* Set the preamp slider */
    [o_slider_preamp setFloatValue: f_preamp];

/* Set the bands slider */
    psz_bands_init = psz_bands;

    for( i = 0; i < 10; i++ )
    {
        /* Read dB -20/20 */
#ifdef HAVE_STRTOF
        f_band[i] = strtof( psz_bands, &p_next );
#else
        f_band[i] = (float)strtod( psz_bands, &p_next );
#endif
        if( !p_next || p_next == psz_bands ) break; /* strtof() failed */

        if( !*psz_bands ) break; /* end of line */
        psz_bands = p_next+1;
    }
    free( psz_bands_init );
	[self setBandSlidersValues:f_band];

	/*
    [o_slider_band1 setFloatValue: f_band[0]];
    [o_slider_band2 setFloatValue: f_band[1]];
    [o_slider_band3 setFloatValue: f_band[2]];
    [o_slider_band4 setFloatValue: f_band[3]];
    [o_slider_band5 setFloatValue: f_band[4]];
    [o_slider_band6 setFloatValue: f_band[5]];
    [o_slider_band7 setFloatValue: f_band[6]];
    [o_slider_band8 setFloatValue: f_band[7]];
    [o_slider_band9 setFloatValue: f_band[8]];
    [o_slider_band10 setFloatValue: f_band[9]];
	*/
	
/* Set the the checkboxes */
    if( b_enabled == VLC_TRUE )
        [o_ckb_enable setState:NSOnState];
    else
        [o_ckb_enable setState:NSOffState];

    [o_ckb_2pass setState:( ( b_2p == VLC_TRUE ) ? NSOnState : NSOffState )];
}

- (IBAction)bandSliderUpdated:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    vlc_object_t *p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE );
    char psz_values[102];
    memset( psz_values, 0, 102 );

    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return;

    /* Write the new bands values */
/* TODO: write a generic code instead of ten times the same thing */

    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band1 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band2 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band3 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band4 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band5 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band6 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band7 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band8 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band9 floatValue] );
    sprintf( psz_values, "%s %.1f", psz_values, [o_slider_band10 floatValue] );

    var_SetString( p_object, "equalizer-bands", psz_values );
    vlc_object_release( p_object );
}

- (IBAction)changePreset:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    int i;
    vlc_object_t *p_object= vlc_object_find( p_intf,
                                VLC_OBJECT_AOUT, FIND_ANYWHERE );
    char psz_values[102];
    memset( psz_values, 0, 102 );

    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return;

    var_SetString( p_object , "equalizer-preset" , preset_list[[sender indexOfSelectedItem]] );

    for( i = 0; i < 10; i++ )
        sprintf( psz_values, "%s %.1f", psz_values, eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[i] );
    var_SetString( p_object, "equalizer-bands", psz_values );
    var_SetFloat( p_object, "equalizer-preamp", eqz_preset_10b[[sender indexOfSelectedItem]]->f_preamp);

    [o_slider_preamp setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_preamp];
	/*
    [o_slider_band1 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[0]];
    [o_slider_band2 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[1]];
    [o_slider_band3 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[2]];
    [o_slider_band4 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[3]];
    [o_slider_band5 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[4]];
    [o_slider_band6 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[5]];
    [o_slider_band7 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[6]];
    [o_slider_band8 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[7]];
    [o_slider_band9 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[8]];
    [o_slider_band10 setFloatValue: eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp[9]];
	*/
	[self setBandSlidersValues:(float *)eqz_preset_10b[[sender indexOfSelectedItem]]->f_amp];
	
    vlc_object_release( p_object );
}

- (IBAction)enable:(id)sender
{
    ChangeFiltersString( VLCIntf, "equalizer", [sender state] );
}

- (IBAction)preampSliderUpdated:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    float f_preamp = [sender floatValue] ;

    vlc_object_t *p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return;

    var_SetFloat( p_object, "equalizer-preamp", f_preamp );

    vlc_object_release( p_object );
}

- (IBAction)toggleWindow:(id)sender
{
    if( [o_window isVisible] )
    {
        [o_window orderOut:sender];
        [o_btn_equalizer setState:NSOffState];
    }
    else
    {
        [o_window makeKeyAndOrderFront:sender];
        [o_btn_equalizer setState:NSOnState];
    }
}

- (IBAction)twopass:(id)sender
{
    intf_thread_t *p_intf = VLCIntf;
    vlc_bool_t b_2p = [sender state] ? VLC_TRUE : VLC_FALSE;
    vlc_object_t *p_object= vlc_object_find( p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE );
    aout_instance_t *p_aout = (aout_instance_t *)p_object;
    if( p_object == NULL )
        p_object = vlc_object_find( p_intf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_object == NULL )
        return;

    var_SetBool( p_object, "equalizer-2pass", b_2p );
    if( ( [o_ckb_enable state] ) && ( p_aout != NULL ) )
    {
       int i;
        for( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
    }

    vlc_object_release( p_object );
}

- (void)windowWillClose:(NSNotification *)aNotification
{
    [o_btn_equalizer setState: NSOffState];
}

- (void)awakeFromNib
{
    int i;
    vlc_object_t *p_object= vlc_object_find( VLCIntf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if( p_object == NULL )
                p_object = vlc_object_find( VLCIntf,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    [o_window setExcludedFromWindowsMenu: TRUE];

    [self initStrings];

    if( p_object )
    {
        char *psz_preset;

        var_Create( p_object, "equalizer-preset", VLC_VAR_STRING |
                                                        VLC_VAR_DOINHERIT );
        psz_preset = var_GetString( p_object, "equalizer-preset" );

        for( i = 0 ; i < 18 ; i++ )
        {
            if( !strcmp( preset_list[i], psz_preset ) )
            {
                [o_popup_presets selectItemAtIndex: i];

                [o_slider_preamp setFloatValue: eqz_preset_10b[i]->f_preamp];
				[self setBandSlidersValues: (float *)eqz_preset_10b[i]->f_amp];
				
				/*
                [o_slider_band1 setFloatValue: eqz_preset_10b[i]->f_amp[0]];
                [o_slider_band2 setFloatValue: eqz_preset_10b[i]->f_amp[1]];
                [o_slider_band3 setFloatValue: eqz_preset_10b[i]->f_amp[2]];
                [o_slider_band4 setFloatValue: eqz_preset_10b[i]->f_amp[3]];
                [o_slider_band5 setFloatValue: eqz_preset_10b[i]->f_amp[4]];
                [o_slider_band6 setFloatValue: eqz_preset_10b[i]->f_amp[5]];
                [o_slider_band7 setFloatValue: eqz_preset_10b[i]->f_amp[6]];
                [o_slider_band8 setFloatValue: eqz_preset_10b[i]->f_amp[7]];
                [o_slider_band9 setFloatValue: eqz_preset_10b[i]->f_amp[8]];
                [o_slider_band10 setFloatValue: eqz_preset_10b[i]->f_amp[9]];
				*/
				
                if( strcmp( psz_preset, "flat" ) )
                {
                    char psz_bands[100];
                    memset( psz_bands, 0, 100 );

                    sprintf( psz_bands, "%.1f %.1f %.1f %.1f %.1f %.1f %.1f "
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
        }
        free( psz_preset );
        vlc_object_release( p_object );
    }

    [self equalizerUpdated];

}


- (id)getSliderByIndex:(int)index
{
	switch(index)
	{
		case 0 : return o_slider_band1;
		case 1 : return o_slider_band2;
		case 2 : return o_slider_band3;
		case 3 : return o_slider_band4;
		case 4 : return o_slider_band5;
		case 5 : return o_slider_band6;
		case 6 : return o_slider_band7;
		case 7 : return o_slider_band8;
		case 8 : return o_slider_band9;
		case 9 : return o_slider_band10;
		default : return nil;
	}
}

- (void)setBandSlidersValues:(float *)values
{
	int i = 0;
	for (i = 0 ; i<= 9 ; i++)
	{
		[self setValue:values[i] forSlider:i];
	}
}

- (void)initBandSliders
{
	int i = 0;
	for (i = 0 ; i< 9 ; i++)
	{
		[self setValue:0.0 forSlider:i];
	}
}

- (void)setValue:(float)value forSlider:(int)index
{
	id slider = [self getSliderByIndex:index];
	
	if (slider != nil)
	{
		[slider setFloatValue:value];
	}
}

@end
