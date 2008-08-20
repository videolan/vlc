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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include "vlc_aout.h"
#include "revmodel.hpp"
#define SPAT_AMP 0.3
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( N_("spatializer") );
    set_shortname( N_("spatializer" ) );
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
    vlc_mutex_t lock;
    revmodel *p_reverbm;

} aout_filter_sys_t;

class CLocker
{
public:
    CLocker( vlc_mutex_t *p_lock ) : p_lock(p_lock) {
        vlc_mutex_lock( p_lock );
    }
    virtual ~CLocker() {
        vlc_mutex_unlock( p_lock );
    }
private:
    vlc_mutex_t *p_lock;
};

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
    if( !p_sys )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sys->lock );
    p_sys->p_reverbm = new revmodel();
    p_sys->p_reverbm->setroomsize(1.05);
    p_sys->p_reverbm->setwet(10.0f);
    p_sys->p_reverbm->setdry(1.0f);
    p_sys->p_reverbm->setdamp(0.3);
    p_sys->p_reverbm->setwidth(0.9);
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
    delete p_sys->p_reverbm;
    vlc_mutex_destroy( &p_sys->lock );
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

    for( i = 0; i < 5 ; i ++ )
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
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    CLocker locker( &p_sys->lock );

    int i, ch, j;
    for( i = 0; i < i_samples; i++ )
    {
        for( ch = 0 ; ch < 2; ch++)
        {
            in[ch] = in[ch] * SPAT_AMP;
        }
        p_sys->p_reverbm->processreplace( in, out , 1, i_channels);
        in  += i_channels;
        out += i_channels;
    }
}

static void SpatClean( aout_filter_t *p_filter )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_filter->p_parent;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_aout, psz_control_names[0], RoomCallback, p_sys );
    var_DelCallback( p_aout, psz_control_names[1], WidthCallback, p_sys );
    var_DelCallback( p_aout, psz_control_names[2], WetCallback, p_sys );
    var_DelCallback( p_aout, psz_control_names[3], DryCallback, p_sys );
    var_DelCallback( p_aout, psz_control_names[4], DampCallback, p_sys );
}

static int RoomCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_filter_sys_t *p_sys = (aout_filter_sys_t*)p_data;
    CLocker locker( &p_sys->lock );

    p_sys->p_reverbm->setroomsize(newval.f_float);
    msg_Dbg (p_this,"room callback %3.1f %s %s %d\n", newval.f_float, __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}

static int WidthCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_filter_sys_t *p_sys = (aout_filter_sys_t*)p_data;
    CLocker locker( &p_sys->lock );

    p_sys->p_reverbm->setwidth(newval.f_float);
    msg_Dbg (p_this,"width callback %3.1f %s %s %d\n", newval.f_float,  __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}
static int WetCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_filter_sys_t *p_sys = (aout_filter_sys_t*)p_data;
    CLocker locker( &p_sys->lock );

    p_sys->p_reverbm->setwet(newval.f_float);
    msg_Dbg (p_this,"wet callback %3.1f %s %s %d\n", newval.f_float,  __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}
static int DryCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_filter_sys_t *p_sys = (aout_filter_sys_t*)p_data;
    CLocker locker( &p_sys->lock );

    p_sys->p_reverbm->setdry(newval.f_float);
    msg_Dbg (p_this,"dry callback %3.1f %s %s %d\n", newval.f_float, __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}
static int DampCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_filter_sys_t *p_sys = (aout_filter_sys_t*)p_data;
    CLocker locker( &p_sys->lock );

    p_sys->p_reverbm->setdamp(newval.f_float);
    msg_Dbg (p_this, "damp callback %3.1f %s %s %d\n", newval.f_float, __FILE__,__func__,__LINE__);
    return VLC_SUCCESS;
}

