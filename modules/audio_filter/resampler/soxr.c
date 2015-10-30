/*****************************************************************************
 * soxr.c: resampler/converter using The SoX Resampler library
 *****************************************************************************
 * Copyright (C) 2015 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

#include <math.h>
#include <soxr.h>

#define SOXR_QUALITY_TEXT N_( "Sox Resampling quality" )

static const int soxr_resampler_quality_vlclist[] = { 0, 1, 2, 3, 4 };
static const char *const soxr_resampler_quality_vlctext[] =
{
    N_( "Quick cubic interpolation" ),
    N_( "Low 16-bit with larger roll-off" ),
    N_( "Medium 16-bit with medium roll-off" ),
    N_( "High quality" ),
    N_( "Very high quality" )
};
static const soxr_datatype_t soxr_resampler_quality_list[] =
{
    SOXR_QQ,
    SOXR_LQ,
    SOXR_MQ,
    SOXR_HQ,
    SOXR_VHQ
};
#define MAX_SOXR_QUALITY 4

static int OpenConverter( vlc_object_t * );
static int OpenResampler( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( "SoX Resampler" )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    add_integer( "soxr-resampler-quality", 2,
                SOXR_QUALITY_TEXT, NULL, true )
        change_integer_list( soxr_resampler_quality_vlclist,
                             soxr_resampler_quality_vlctext )
    set_capability ( "audio converter", 0 )
    set_callbacks( OpenConverter, Close )

    add_submodule()
    set_capability( "audio resampler", 0 )
    set_callbacks( OpenResampler, Close )
    add_shortcut( "soxr" )
vlc_module_end ()

struct filter_sys_t
{
    soxr_t  soxr;
    double  f_fixed_ratio;
    block_t *p_last_in;
};

static block_t *Resample( filter_t *, block_t * );

static bool
SoXR_GetFormat( vlc_fourcc_t i_format, soxr_datatype_t *p_type )
{
    switch( i_format )
    {
        case VLC_CODEC_FL64:
            *p_type = SOXR_FLOAT64_I;
            return true;
        case VLC_CODEC_FL32:
            *p_type = SOXR_FLOAT32_I;
            return true;
        case VLC_CODEC_S32N:
            *p_type = SOXR_INT32_I;
            return true;
        case VLC_CODEC_S16N:
            *p_type = SOXR_INT16_I;
            return true;
        default:
            return false;
    }
}

static int
Open( vlc_object_t *p_obj, bool b_change_ratio )
{
    filter_t *p_filter = (filter_t *)p_obj;

    /* Cannot remix */
    if( p_filter->fmt_in.audio.i_physical_channels
            != p_filter->fmt_out.audio.i_physical_channels
     || p_filter->fmt_in.audio.i_original_channels
            != p_filter->fmt_out.audio.i_original_channels )
        return VLC_EGENERIC;

    /* Get SoXR input/output format */
    soxr_datatype_t i_itype, i_otype;
    if( !SoXR_GetFormat( p_filter->fmt_in.audio.i_format, &i_itype )
     || !SoXR_GetFormat( p_filter->fmt_out.audio.i_format, &i_otype ) )
        return VLC_EGENERIC;

    filter_sys_t *p_sys = calloc( 1, sizeof( struct filter_sys_t ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    /* Setup SoXR */
    int64_t i_vlc_q = var_InheritInteger( p_obj, "soxr-resampler-quality" );
    if( i_vlc_q < 0 )
        i_vlc_q = 0;
    else if( i_vlc_q > MAX_SOXR_QUALITY )
        i_vlc_q = MAX_SOXR_QUALITY;
    const unsigned long i_recipe = soxr_resampler_quality_list[i_vlc_q];
    const unsigned i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    const double f_ratio = p_filter->fmt_out.audio.i_rate
                           / (double) p_filter->fmt_in.audio.i_rate;
    /* XXX: Performances are worse with Variable-Rate */
    const unsigned long i_flags = b_change_ratio ? SOXR_VR : 0;

    p_sys->f_fixed_ratio = b_change_ratio ? 0.0f : f_ratio;
    soxr_error_t error;
    /* IO spec */
    soxr_io_spec_t io_spec = soxr_io_spec( i_itype, i_otype );
    /* Quality spec */
    soxr_quality_spec_t q_spec = soxr_quality_spec( i_recipe, i_flags );
    /* Create SoXR */
    p_sys->soxr = soxr_create( 1, f_ratio, i_channels,
                               &error, &io_spec, &q_spec, NULL );
    if( error )
    {
        msg_Err( p_filter, "soxr_create failed: %s", soxr_strerror( error ) );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if( b_change_ratio )
        soxr_set_io_ratio( p_sys->soxr, 1 / f_ratio, 0 );

    msg_Dbg( p_filter, "Using SoX Resampler with '%s' engine and '%s' quality "
             "to convert %4.4s/%dHz to %4.4s/%dHz.",
             soxr_engine( p_sys->soxr ), soxr_resampler_quality_vlctext[i_vlc_q],
             (const char *)&p_filter->fmt_in.audio.i_format,
             p_filter->fmt_in.audio.i_rate,
             (const char *)&p_filter->fmt_out.audio.i_format,
             p_filter->fmt_out.audio.i_rate );

    p_filter->p_sys = p_sys;
    p_filter->pf_audio_filter = Resample;
    return VLC_SUCCESS;
}

static int
OpenResampler( vlc_object_t *p_obj )
{
    filter_t *p_filter = (filter_t *)p_obj;

    /* A resampler doesn't convert the format */
    if( p_filter->fmt_in.audio.i_format != p_filter->fmt_out.audio.i_format )
        return VLC_EGENERIC;
    return Open( p_obj, true );
}

static int
OpenConverter( vlc_object_t *p_obj )
{
    filter_t *p_filter = (filter_t *)p_obj;

    /* Don't use SoXR to convert format. Prefer to use converter/format.c that
     * has better performances */
    if( p_filter->fmt_in.audio.i_rate == p_filter->fmt_out.audio.i_rate )
        return VLC_EGENERIC;
    return Open( p_obj, false );
}

static void
Close( vlc_object_t *p_obj )
{
    filter_t *p_filter = (filter_t *)p_obj;
    filter_sys_t *p_sys = p_filter->p_sys;

    soxr_delete( p_sys->soxr );

    if( unlikely( p_sys->p_last_in ) )
        block_Release( p_sys->p_last_in );

    free( p_sys );
}

static block_t *
Resample( filter_t *p_filter, block_t *p_in )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Prepend last remaining input buffer to the current one */
    if( unlikely( p_sys->p_last_in ) )
    {
        p_in = block_Realloc( p_in, p_sys->p_last_in->i_buffer, p_in->i_buffer );
        if( unlikely( p_in == NULL ) )
            return NULL;

        memcpy( p_in->p_buffer, p_sys->p_last_in->p_buffer,
                p_sys->p_last_in->i_buffer );
        p_in->i_nb_samples += p_sys->p_last_in->i_nb_samples;
        block_Release( p_sys->p_last_in );
        p_sys->p_last_in = NULL;
    }

    const size_t i_oframesize = p_filter->fmt_out.audio.i_bytes_per_frame;
    const size_t i_ilen = p_in->i_nb_samples;
    size_t i_olen, i_idone, i_odone;

    if( p_sys->f_fixed_ratio == 0.0f )
    {
        /* "audio resampler" with variable ratio */

        const double f_ratio = p_filter->fmt_out.audio.i_rate
                             / (double) p_filter->fmt_in.audio.i_rate;
        if( f_ratio == 1.0f )
            return p_in;

        /* processed output len might be a little bigger than expected */
        i_olen = lrint( ( i_ilen + 2 ) * f_ratio * 11. / 10. );

        soxr_set_io_ratio( p_sys->soxr, 1 / f_ratio, i_olen );
    }
    else
        i_olen = lrint( i_ilen * p_sys->f_fixed_ratio );

    /* Use input buffer as output if there is enough room */
    block_t *p_out = i_ilen >= i_olen ? p_in
                   : block_Alloc( i_olen * i_oframesize );
    if( unlikely( p_out == NULL ) )
        goto error;

    /* Process SoXR */
    soxr_error_t error = soxr_process( p_sys->soxr,
                                       p_in->p_buffer, i_ilen, &i_idone,
                                       p_out->p_buffer, i_olen, &i_odone );
    if( error )
    {
        msg_Err( p_filter, "soxr_process failed: %s", soxr_strerror( error ) );
        goto error;
    }

    if( unlikely( i_idone < i_ilen ) )
    {
        msg_Warn( p_filter, "processed input len < input len, "
                  "keeping buffer for next Resample call" );
        const size_t i_done_size = i_idone
                                 * p_filter->fmt_out.audio.i_bytes_per_frame;

        /* Realloc since p_in can be used as p_out */
        p_sys->p_last_in = block_Alloc( p_in->i_buffer - i_done_size );
        if( unlikely( p_sys->p_last_in == NULL ) )
            goto error;
        memcpy( p_sys->p_last_in->p_buffer,
                p_in->p_buffer + i_done_size, p_in->i_buffer - i_done_size );
        p_sys->p_last_in->i_nb_samples = p_in->i_nb_samples - i_idone;
    }

    p_out->i_buffer = i_odone * i_oframesize;
    p_out->i_nb_samples = i_odone;
    p_out->i_pts = p_in->i_pts;
    p_out->i_length = i_odone * CLOCK_FREQ / p_filter->fmt_out.audio.i_rate;

    if( p_out != p_in )
        block_Release( p_in );
    return p_out;

error:

    if( p_out && p_out != p_in )
        block_Release( p_out );
    block_Release( p_in );
    return NULL;
}
