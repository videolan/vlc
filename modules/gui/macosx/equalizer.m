/*****************************************************************************
 * equalizer.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: equalizer.m 1 2004-08-07 23:50:00Z djc $
 *
 * Authors: JŽr™me Decoodt <djc@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <aout_internal.h>

#include "intf.h"

#include <math.h>

#include "equalizer.h"

/*****************************************************************************
 * VLCEqualizer implementation 
 *****************************************************************************/
@implementation VLCEqualizer

static void ChangeFiltersString( intf_thread_t *p_intf,
                                 aout_instance_t * p_aout,
                                 char *psz_name, vlc_bool_t b_add )
{
    char *psz_parser, *psz_string;

    if( p_aout )
    {
        psz_string = var_GetString( p_aout, "audio-filter" );
    }
    else
    {
        psz_string = config_GetPsz( p_intf, "audio-filter" );
    }

    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s,%s" : "%s%s",
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
                            (*(psz_parser + strlen(psz_name)) == ',' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            if( *(psz_string+strlen(psz_string ) -1 ) == ',' )
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

    if( p_aout == NULL )
    {
        config_PutPsz( p_intf, "audio-filter", psz_string );
    }
    else
    {
        int i;
        var_SetString( p_aout, "audio-filter", psz_string );
        for( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
    }
    free( psz_string );
}

static vlc_bool_t GetFiltersStatus( intf_thread_t *p_intf,
                                 aout_instance_t * p_aout,
                                 char *psz_name )
{
    char *psz_parser, *psz_string;

    if( p_aout )
    {
        psz_string = var_GetString( p_aout, "audio-filter" );
    }
    else
    {
        psz_string = config_GetPsz( p_intf, "audio-filter" );
    }

    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    free( psz_string );

    if (psz_parser)
        return VLC_TRUE;
    else
        return VLC_FALSE;
}

- (IBAction)bandSliderUpdated:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    char psz_values[102];
    memset( psz_values, 0, 102 );

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

    if( p_aout == NULL )
    {
        config_PutPsz( p_intf, "equalizer-bands", psz_values );
        msg_Dbg( p_intf, "equalizer settings set to %s", psz_values);
    }
    else
    {
        var_SetString( p_aout, "equalizer-bands", psz_values );
        msg_Dbg( p_intf, "equalizer settings changed to %s", psz_values);
        vlc_object_release( p_aout );
    }
}

- (IBAction)changePreset:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    float preamp, band[10];
    char *p, *p_next;
    int b_2p;
    int i;
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                VLC_OBJECT_AOUT, FIND_ANYWHERE);

    vlc_bool_t enabled = GetFiltersStatus( p_intf, p_aout, "equalizer" );

static char *preset_list[] = {
    "flat", "classical", "club", "dance", "fullbass", "fullbasstreeble",
    "fulltreeble", "headphones","largehall", "live", "party", "pop", "reggae",
    "rock", "ska", "soft", "softrock", "techno"
};

    if( p_aout == NULL )
    {
        config_PutPsz( p_intf, "equalizer-preset",preset_list[[sender indexOfSelectedItem]] );
    }
    else
    {
        var_SetString( p_aout , "equalizer-preset" ,preset_list[[sender indexOfSelectedItem]] );
        vlc_object_release( p_aout );
    }

        
    if( ( p_aout == NULL ) || ( enabled == VLC_FALSE ) )
    {
        preamp = config_GetFloat( p_intf, "equalizer-preamp" );
        p = config_GetPsz( p_intf, "equalizer-bands");
        b_2p = config_GetInt( p_intf, "equalizer-2pass" );
    }
    else
    {
        preamp = var_GetFloat( p_aout, "equalizer-preamp" );
        p = var_GetString( p_aout, "equalizer-bands" );
        b_2p = var_GetBool( p_aout, "equalizer-2pass" );
        vlc_object_release( p_aout );
    }

/* Set the preamp slider */
    [o_slider_preamp setFloatValue: preamp];

/* Set the bands slider */
    for( i = 0; i < 10; i++ )
    {
        /* Read dB -20/20 */
#ifdef HAVE_STRTOF
        band[i] = strtof( p, &p_next );
#else
        band[i] = (float) strtod( p, &p_next );
#endif
        if( !p_next || p_next == p ) break; /* strtof() failed */

        if( !*p ) break; /* end of line */
        p=p_next+1;
    }
    [o_slider_band1 setFloatValue: band[0]];
    [o_slider_band2 setFloatValue: band[1]];
    [o_slider_band3 setFloatValue: band[2]];
    [o_slider_band4 setFloatValue: band[3]];
    [o_slider_band5 setFloatValue: band[4]];
    [o_slider_band6 setFloatValue: band[5]];
    [o_slider_band7 setFloatValue: band[6]];
    [o_slider_band8 setFloatValue: band[7]];
    [o_slider_band9 setFloatValue: band[8]];
    [o_slider_band10 setFloatValue: band[9]];
                                 
    if( enabled == VLC_TRUE )
        [o_btn_enable setState:NSOnState];
    else
        [o_btn_enable setState:NSOffState];

    [o_btn_2pass setState:( ( b_2p == VLC_TRUE )?NSOnState:NSOffState )];
}

- (IBAction)enable:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    ChangeFiltersString( p_intf,p_aout, "equalizer", [sender state]);

    if( p_aout != NULL )
        vlc_object_release( p_aout );
}

- (IBAction)preampSliderUpdated:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    float f= [sender floatValue] ;

    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);

    if( p_aout == NULL )
    {
        config_PutFloat( p_intf, "equalizer-preamp", f );
    }
    else
    {
        var_SetFloat( p_aout, "equalizer-preamp", f );
        vlc_object_release( p_aout );
    }
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
        intf_thread_t * p_intf = VLCIntf;
        float preamp, band[10];
        char *p, *p_next;
        int b_2p;
        int i;
        aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                    VLC_OBJECT_AOUT, FIND_ANYWHERE);

        vlc_bool_t enabled = GetFiltersStatus( p_intf, p_aout, "equalizer" );
        
        if( ( p_aout == NULL ) || ( enabled == VLC_FALSE ) )
        {
            preamp = config_GetFloat( p_intf, "equalizer-preamp" );
            p = config_GetPsz( p_intf, "equalizer-bands");
            b_2p = config_GetInt( p_intf, "equalizer-2pass" );
        }
        else
        {
            preamp = var_GetFloat( p_aout, "equalizer-preamp" );
            p = var_GetString( p_aout, "equalizer-bands" );
            b_2p = var_GetBool( p_aout, "equalizer-2pass" );
            vlc_object_release( p_aout );
        }
        if( !p )
            p = "0 0 0 0 0 0 0 0 0 0";

/* Set the preamp slider */
        [o_slider_preamp setFloatValue: preamp];

/* Set the bands slider */
        for( i = 0; i < 10; i++ )
        {
            /* Read dB -20/20 */
#ifdef HAVE_STRTOF
            band[i] = strtof( p, &p_next );
#else
            band[i] = (float) strtod( p, &p_next );
#endif
            if( !p_next || p_next == p ) break; /* strtof() failed */

            if( !*p ) break; /* end of line */
            p=p_next+1;
        }
        [o_slider_band1 setFloatValue: band[0]];
        [o_slider_band2 setFloatValue: band[1]];
        [o_slider_band3 setFloatValue: band[2]];
        [o_slider_band4 setFloatValue: band[3]];
        [o_slider_band5 setFloatValue: band[4]];
        [o_slider_band6 setFloatValue: band[5]];
        [o_slider_band7 setFloatValue: band[6]];
        [o_slider_band8 setFloatValue: band[7]];
        [o_slider_band9 setFloatValue: band[8]];
        [o_slider_band10 setFloatValue: band[9]];

        if( enabled == VLC_TRUE )
            [o_btn_enable setState:NSOnState];
        else
            [o_btn_enable setState:NSOffState];

        [o_btn_2pass setState:( ( b_2p == VLC_TRUE )?NSOnState:NSOffState )];
        
        [o_window makeKeyAndOrderFront:sender];
        [o_btn_equalizer setState:NSOnState];
    }
}

- (IBAction)twopass:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);

    vlc_bool_t b_2p = [sender state] ? VLC_TRUE : VLC_FALSE;

    if( p_aout == NULL )
    {
        config_PutInt( p_intf, "equalizer-2pass", b_2p );
    }
    else
    {
        var_SetBool( p_aout, "equalizer-2pass", b_2p );
        if( [o_btn_enable state] )
        {
            int i;
            for( i = 0; i < p_aout->i_nb_inputs; i++ )
            {
                p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
            }
        }
        vlc_object_release( p_aout );
    }

}

@end
