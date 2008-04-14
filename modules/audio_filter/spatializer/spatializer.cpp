/*****************************************************************************
 * spatializer.cpp:
 *****************************************************************************
 * Copyright (C) 2004, 2006, 2007 the VideoLAN team
 *
 * Google Summer of Code 2007
 *
 * Authors: Biodun Osunkunle <biodun@videolan.org>
 *
 * Mentor : Jean-Baptiste Kempf <jb@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <math.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include "vlc_aout.h"
#include "revmodel.hpp"
#define SPAT_AMP 0.3
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("spatializer") );
    set_shortname( _("spatializer" ) );
    set_capability( "audio filter", 0 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AFILTER );

    set_callbacks( Open, Close );
    add_shortcut( "spatializer" );
    add_float( "Roomsize", 1.05, NULL, NULL,NULL, true);
    add_float( "Width", 10.0, NULL, NULL,NULL, true);
    add_float( "Wet", 3.0, NULL, NULL,NULL, true);
    add_float( "Dry", 2.0, NULL, NULL,NULL, true);
    add_float( "Damp", 1.0, NULL, NULL,NULL, true);
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct aout_filter_sys_t
{
    /* reverb static config */
    bool b_first;

} aout_filter_sys_t;

static revmodel reverbm;

static const char *psz_control_names[] =
{
    "Roomsize", "Width" , "Wet", "Dry", "Damp"
};
static void DoWork( aout_instance_t *, aout_filter_t *,
                    aout_buffer_t *, aout_buffer_t * );

static int  SpatInit( aout_filter_t *);
static void SpatFilter( aout_instance_t *,aout_filter_t *, float *, float *,
                        int, int );
static void SpatClean( aout_filter_t * );
static int RoomCallback ( vlc_object_t *, char const *,
                                           vlc_value_t, vlc_value_t, void * );
static int WetCallback ( vlc_object_t *, char const *,
                                           vlc_value_t, vlc_value_t, void * );
static int DryCallback ( vlc_object_t *, char const *,
                                           vlc_value_t, vlc_value_t, void * );
static int DampCallback ( vlc_object_t *, char const *,
                                           vlc_value_t, vlc_value_t, void * );
static int WidthCallback ( vlc_object_t *, char const *,
                                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys;
    bool         b_fit = true;
    msg_Dbg(p_this, "Opening filter spatializer %s %s %d\n", __FILE__,__func__,__LINE__);

    if( p_filter->input.i_format != VLC_FOURCC('f','l','3','2' ) ||
        p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        b_fit = false;
        p_filter->input.i_format = VLC_FOURCC('f','l','3','2');
        p_filter->output.i_format = VLC_FOURCC('f','l','3','2');
        msg_Warn( p_filter, "bad input or output format" );
    }
    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        b_fit = false;
        memcpy( &p_filter->output, &p_filter->input,
                sizeof(audio_sample_format_t) );
        msg_Warn( p_filter, "input and output formats are not similar" );
    }

    if ( ! b_fit )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = true;

     /* Allocate structure */
    p_sys = p_filter->p_sys = (aout_filter_sys_t*)malloc( sizeof( aout_filter_sys_t ) );
    reverbm.setroomsize(1.05);
    reverbm.setwet(10.0f);
    reverbm.setdry(1.0f);
    reverbm.setdamp(0.3);
    reverbm.setwidth(0.9);
    SpatInit( p_filter);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    SpatClean( p_filter );
    free( p_sys );
    msg_Dbg(p_this, "Closing filter spatializer %s %s %d\n", __FILE__,__func__,__LINE__);
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 *
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;

    SpatFilter( p_aout, p_filter, (float*)p_out_buf->p_buffer,
               (float*)p_in_buf->p_buffer, p_in_buf->i_nb_samples,
               aout_FormatNbChannels( &p_filter->input ) );
}

static int SpatInit( aout_filter_t *p_filter )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    int i, ch;
    vlc_value_t val1, val2, val3, val4, val5;
    aout_instance_t *p_aout = (aout_instance_t *)p_filter->p_parent;

    for( int i = 0; i < 5 ; i ++ )
    {
     var_CreateGetFloatCommand( p_aout, psz_control_names[i] );
    }

    /* Get initial values */
    var_Get( p_aout, psz_control_names[0], &val1 );
    var_Get( p_aout, psz_control_names[1], &val2 );
    var_Get( p_aout, psz_control_names[2], &val3 );
    var_Get( p_aout, psz_control_names[3], &val4 );
    var_Get( p_aout, psz_control_names[4], &val5);

    RoomCallback( VLC_OBJECT( p_aout ), NULL, val1, val1, p_sys );
    WidthCallback( VLC_OBJECT( p_aout ), NULL, val2, val2, p_sys );
    WetCallback( VLC_OBJECT( p_aout ), NULL, val3, val3, p_sys );
    DryCallback( VLC_OBJECT( p_aout ), NULL, val4, val4, p_sys );
    DampCallback( VLC_OBJECT( p_aout ), NULL, val5, val5, p_sys );

    msg_Dbg( p_filter, "%f", val1.f_float );
    /* Add our own callbacks */
    var_AddCallback( p_aout, psz_control_names[0], RoomCallback, p_sys );
    var_AddCallback( p_aout, psz_control_names[1], WidthCallback, p_sys );
    var_AddCallback( p_aout, psz_control_names[2], WetCallback, p_sys );
    var_AddCallback( p_aout, psz_control_names[3], DryCallback, p_sys );
    var_AddCallback( p_aout, psz_control_names[4], DampCallback, p_sys );

    return VLC_SUCCESS;
}

static void SpatFilter( aout_instance_t *p_aout,
                       aout_filter_t *p_filter, float *out, float *in,
                       int i_samples, int i_channels )
{
    int i, ch, j;
    for( i = 0; i < i_samples; i++ )
    {
        for( ch = 0 ; ch < 2; ch++)
        {
            in[ch] = in[ch] * SPAT_AMP;
        }
           reverbm.processreplace( in, out , 1, i_channels);
         in  += i_channels;
         out += i_channels;
    }
}

static void SpatClean( aout_filter_t *p_filter )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( (aout_instance_t *)p_filter->p_parent,
                        "Roomsize", RoomCallback, p_sys );
    var_DelCallback( (aout_instance_t *)p_filter->p_parent,
                        "Width", WidthCallback, p_sys );
    var_DelCallback( (aout_instance_t *)p_filter->p_parent,
                        "Wet", WetCallback, p_sys );
    var_DelCallback( (aout_instance_t *)p_filter->p_parent,
                        "Dry", DryCallback, p_sys );
    var_DelCallback( (aout_instance_t *)p_filter->p_parent,
                        "Damp", DampCallback, p_sys );

}

static int RoomCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    msg_Dbg (p_this,"room callback %3.1f %s %s %d\n", newval.f_float, __FILE__,__func__,__LINE__);
    reverbm.setroomsize(newval.f_float);
    return VLC_SUCCESS;
}

static int WidthCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    reverbm.setwidth(newval.f_float);
    msg_Dbg (p_this,"width callback %3.1f %s %s %d\n", newval.f_float,  __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}
static int WetCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    reverbm.setwet(newval.f_float);
    msg_Dbg (p_this,"wet callback %3.1f %s %s %d\n", newval.f_float,  __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}
static int DryCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    reverbm.setdry(newval.f_float);
    msg_Dbg (p_this,"dry callback %3.1f %s %s %d\n", newval.f_float, __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}
static int DampCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    reverbm.setdamp(newval.f_float);
    msg_Dbg (p_this, "damp callback %3.1f %s %s %d\n", newval.f_float, __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}

