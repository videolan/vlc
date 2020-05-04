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

#include <assert.h>
#include <math.h>
#include <soxr.h>

#define SOXR_QUALITY_TEXT N_( "Resampling quality" )
#define SOXR_QUALITY_LONGTEXT N_( "Resampling quality, from worst to best" )

static const int soxr_resampler_quality_vlclist[] = { 0, 1, 2, 3, 4 };
static const char *const soxr_resampler_quality_vlctext[] =
{
     "Quick cubic interpolation",
     "Low 16-bit with larger roll-off",
     "Medium 16-bit with medium roll-off",
     "High quality",
     "Very high quality"
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
    set_shortname( N_("SoX Resampler") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_RESAMPLER )
    add_integer( "soxr-resampler-quality", 2,
                SOXR_QUALITY_TEXT, NULL, true )
        change_integer_list( soxr_resampler_quality_vlclist,
                             soxr_resampler_quality_vlctext )
    set_capability ( "audio converter", 51 )
    set_callbacks( OpenConverter, Close )

    add_submodule()
    set_capability( "audio resampler", 51 )
    set_callbacks( OpenResampler, Close )
    add_shortcut( "soxr" )
vlc_module_end ()

typedef struct
{
    soxr_t  soxr;
    soxr_t  vr_soxr;
    soxr_t  last_soxr;
    double  f_fixed_ratio;
    size_t  i_last_olen;
    vlc_tick_t i_last_pts;
} filter_sys_t;

static block_t *Resample( filter_t *, block_t * );
static block_t *Drain( filter_t * );
static void     Flush( filter_t * );

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
    if( p_filter->fmt_in.audio.i_channels != p_filter->fmt_out.audio.i_channels )
        return VLC_EGENERIC;

    /* Get SoXR input/output format */
    soxr_datatype_t i_itype, i_otype;
    if( !SoXR_GetFormat( p_filter->fmt_in.audio.i_format, &i_itype )
     || !SoXR_GetFormat( p_filter->fmt_out.audio.i_format, &i_otype ) )
        return VLC_EGENERIC;

    filter_sys_t *p_sys = calloc( 1, sizeof( filter_sys_t ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    /* Setup SoXR */
    int64_t i_vlc_q = var_InheritInteger( p_obj, "soxr-resampler-quality" );
    if( i_vlc_q < 0 )
        i_vlc_q = 0;
    else if( i_vlc_q > MAX_SOXR_QUALITY )
        i_vlc_q = MAX_SOXR_QUALITY;
    const unsigned long i_recipe = soxr_resampler_quality_list[i_vlc_q];
    const unsigned i_channels = p_filter->fmt_in.audio.i_channels;
    const double f_ratio = p_filter->fmt_out.audio.i_rate
                           / (double) p_filter->fmt_in.audio.i_rate;

    p_sys->f_fixed_ratio = f_ratio;
    soxr_error_t error;
    /* IO spec */
    soxr_io_spec_t io_spec = soxr_io_spec( i_itype, i_otype );
    /* Quality spec */
    soxr_quality_spec_t q_spec = soxr_quality_spec( i_recipe, 0 );
    /* Create SoXR */
    p_sys->soxr = soxr_create( 1, f_ratio, i_channels,
                               &error, &io_spec, &q_spec, NULL );
    if( error )
    {
        msg_Err( p_filter, "soxr_create failed: %s", soxr_strerror( error ) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Create a 'variable-rate' SoXR if needed: it is slower than the fixed
     * one, but it will be only used when the input rate is changing (to catch
     * up a delay).  */
    if( b_change_ratio )
    {
        q_spec = soxr_quality_spec( SOXR_LQ, SOXR_VR );
        p_sys->vr_soxr = soxr_create( 1, f_ratio, i_channels,
                                      &error, &io_spec, &q_spec, NULL );
        if( error )
        {
            msg_Err( p_filter, "soxr_create failed: %s", soxr_strerror( error ) );
            soxr_delete( p_sys->soxr );
            free( p_sys );
            return VLC_EGENERIC;
        }
        soxr_set_io_ratio( p_sys->vr_soxr, 1 / f_ratio, 0 );
    }

    msg_Dbg( p_filter, "Using SoX Resampler with '%s' engine and '%s' quality "
             "to convert %4.4s/%dHz to %4.4s/%dHz.",
             soxr_engine( p_sys->soxr ), soxr_resampler_quality_vlctext[i_vlc_q],
             (const char *)&p_filter->fmt_in.audio.i_format,
             p_filter->fmt_in.audio.i_rate,
             (const char *)&p_filter->fmt_out.audio.i_format,
             p_filter->fmt_out.audio.i_rate );

    p_filter->p_sys = p_sys;
    p_filter->pf_audio_filter = Resample;
    p_filter->pf_flush = Flush;
    p_filter->pf_audio_drain = Drain;
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
    if( p_sys->vr_soxr )
        soxr_delete( p_sys->vr_soxr );

    free( p_sys );
}

static block_t *
SoXR_Resample( filter_t *p_filter, soxr_t soxr, block_t *p_in, size_t i_olen )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    size_t i_idone, i_odone;
    const size_t i_oframesize = p_filter->fmt_out.audio.i_bytes_per_frame;
    const size_t i_ilen = p_in ? p_in->i_nb_samples : 0;

    block_t *p_out;
    if( i_ilen >= i_olen )
    {
        i_olen = i_ilen;
        p_out = p_in;
    }
    else
    {
        p_out = block_Alloc( i_olen * i_oframesize );
        if( p_out == NULL )
            goto error;
    }

    soxr_error_t error = soxr_process( soxr, p_in ? p_in->p_buffer : NULL,
                                       i_ilen, &i_idone, p_out->p_buffer,
                                       i_olen, &i_odone );
    if( error )
    {
        msg_Err( p_filter, "soxr_process failed: %s", soxr_strerror( error ) );
        block_Release( p_out );
        goto error;
    }
    if( unlikely( i_idone < i_ilen ) )
        msg_Err( p_filter, "lost %zd of %zd input frames",
                 i_ilen - i_idone, i_idone);

    p_out->i_buffer = i_odone * i_oframesize;
    p_out->i_nb_samples = i_odone;
    p_out->i_length = vlc_tick_from_samples(i_odone, p_filter->fmt_out.audio.i_rate);

    if( p_in )
    {
        p_sys->i_last_olen = i_olen;
        p_sys->last_soxr = soxr;
    }
    else
    {
        soxr_clear( soxr );
        p_sys->i_last_olen = 0;
        p_sys->last_soxr = NULL;
    }

error:
    if( p_in && p_out != p_in )
        block_Release( p_in );

    return p_out;
}

static size_t
SoXR_GetOutLen( size_t i_ilen, double f_ratio )
{
    /* Processed output len might be a little bigger than expected. Indeed SoXR
     * can hold a few samples to meet the need of the resampling filter. */
    return lrint( ( i_ilen + 2 ) * f_ratio * 11. / 10. );
}

static block_t *
Resample( filter_t *p_filter, block_t *p_in )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const vlc_tick_t i_pts = p_in->i_pts;

    if( p_sys->vr_soxr )
    {
        /* "audio resampler" with variable ratio: use the fixed resampler when
         * the ratio is the same than the fixed one, otherwise use the variable
         * resampler. */

        soxr_t soxr;
        block_t *p_flushed_out = NULL, *p_out = NULL;
        const double f_ratio = p_filter->fmt_out.audio.i_rate
                             / (double) p_filter->fmt_in.audio.i_rate;
        size_t i_olen = SoXR_GetOutLen( p_in->i_nb_samples,
            f_ratio > p_sys->f_fixed_ratio ? f_ratio : p_sys->f_fixed_ratio );

        if( f_ratio != p_sys->f_fixed_ratio )
        {
            /* using variable resampler */
            soxr_set_io_ratio( p_sys->vr_soxr, 1 / f_ratio, 0 /* instant change */ );
            soxr = p_sys->vr_soxr;
        }
        else if( f_ratio == 1.0f )
        {
            /* not using any resampler */
            soxr = NULL;
            p_out = p_in;
        }
        else
        {
            /* using fixed resampler */
            soxr = p_sys->soxr;
        }

        /* If the new soxr is different than the last one, flush it */
        if( p_sys->last_soxr && soxr != p_sys->last_soxr && p_sys->i_last_olen )
        {
            p_flushed_out = SoXR_Resample( p_filter, p_sys->last_soxr,
                                           NULL, p_sys->i_last_olen );
            if( soxr )
                msg_Dbg( p_filter, "Using '%s' engine", soxr_engine( soxr ) );
        }

        if( soxr )
        {
            assert( !p_out );
            p_out = SoXR_Resample( p_filter, soxr, p_in, i_olen );
            if( !p_out )
                goto error;
        }

        if( p_flushed_out )
        {
            /* Prepend the flushed output data to p_out */
            const unsigned i_nb_samples = p_flushed_out->i_nb_samples
                                        + p_out->i_nb_samples;

            block_ChainAppend( &p_flushed_out, p_out );
            p_out = block_ChainGather( p_flushed_out );
            if( !p_out )
                goto error;
            p_out->i_nb_samples = i_nb_samples;
        }
        p_out->i_pts = i_pts;
        return p_out;
    }
    else
    {
        /* "audio converter" with fixed ratio */

        const size_t i_olen = SoXR_GetOutLen( p_in->i_nb_samples,
                                              p_sys->f_fixed_ratio );
        block_t *p_out = SoXR_Resample( p_filter, p_sys->soxr, p_in, i_olen );
        if( p_out )
            p_out->i_pts = i_pts;
        return p_out;
    }
error:
    block_Release( p_in );
    return NULL;
}

static block_t *
Drain( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->last_soxr && p_sys->i_last_olen )
        return SoXR_Resample( p_filter, p_sys->last_soxr, NULL,
                              p_sys->i_last_olen );
    else
        return NULL;
}

static void
Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->last_soxr )
    {
        soxr_clear( p_sys->last_soxr );
        p_sys->i_last_olen = 0;
        p_sys->last_soxr = NULL;
    }
}
