/*****************************************************************************
 * scaletempo.c: Scale audio tempo while maintaining pitch
 *****************************************************************************
 * Copyright © 2008 VLC authors and VideoLAN
 *
 * Authors: Rov Juvano <rovjuvano@users.sourceforge.net>
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
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_modules.h>

#include <stdatomic.h>
#include <string.h> /* for memset */
#include <limits.h> /* form INT_MIN */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( filter_t * );
static block_t *DoWork( filter_t *, block_t * );

#ifdef PITCH_SHIFTER
static int  OpenPitch( vlc_object_t * );
static void ClosePitch( filter_t * );
static block_t *DoPitchWork( filter_t *, block_t * );
# define MODULE_DESC N_("Pitch Shifter")
# define MODULES_SHORTNAME N_("Audio pitch changer")
#else
# define MODULE_DESC N_("Audio tempo scaler synched with rate")
# define MODULES_SHORTNAME N_("Scaletempo")
#endif

vlc_module_begin ()
    set_description( MODULE_DESC )
    set_shortname( MODULES_SHORTNAME )
    set_capability( "audio filter", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )

    add_integer_with_range( "scaletempo-stride", 30, 1, 2000,
        N_("Stride Length"), N_("Length in milliseconds to output each stride") )
    add_float_with_range( "scaletempo-overlap", .20, 0.0, 1.0,
        N_("Overlap Length"), N_("Percentage of stride to overlap") )
    add_integer_with_range( "scaletempo-search", 14, 0, 200,
        N_("Search Length"), N_("Length in milliseconds to search for best overlap position") )
#ifdef PITCH_SHIFTER
    add_float_with_range( "pitch-shift", 0, -12, 12,
        N_("Pitch Shift"), N_("Pitch shift in semitones.") )
    set_callback( OpenPitch )
#else
    set_callback( Open )
#endif

vlc_module_end ()

/*
 * Scaletempo works by producing audio in constant sized chunks (a "stride") but
 * consuming chunks proportional to the playback rate.
 *
 * Scaletempo then smooths the output by blending the end of one stride with
 * the next ("overlap").
 *
 * Scaletempo smooths the overlap further by searching within the input buffer
 * for the best overlap position.  Scaletempo uses a statistical cross correlation
 * (roughly a dot-product).  Scaletempo consumes most of its CPU cycles here.
 *
 * NOTE:
 * sample: a single audio sample for one channel
 * frame: a single set of samples, one for each channel
 * VLC uses these terms differently
 */
typedef struct
{
    /* Filter static config */
    double    scale;
    /* parameters */
    unsigned  ms_stride;
    double    percent_overlap;
    unsigned  ms_search;
    /* audio format */
    unsigned  samples_per_frame;  /* AKA number of channels */
    unsigned  bytes_per_sample;
    unsigned  bytes_per_frame;
    unsigned  sample_rate;
    /* stride */
    double    frames_stride_scaled;
    double    frames_stride_error;
    unsigned  bytes_stride;
    double    bytes_stride_scaled;
    unsigned  bytes_queue_max;
    unsigned  bytes_queued;
    unsigned  bytes_to_slide;
    uint8_t  *buf_queue;
    /* overlap */
    unsigned  samples_overlap;
    unsigned  samples_standing;
    unsigned  bytes_overlap;
    unsigned  bytes_standing;
    void     *buf_overlap;
    void     *table_blend;
    void    (*output_overlap)( filter_t *p_filter, void *p_out_buf, unsigned bytes_off );
    /* best overlap */
    unsigned  frames_search;
    void     *buf_pre_corr;
    void     *table_window;
    unsigned(*best_overlap_offset)( filter_t *p_filter );
#ifdef PITCH_SHIFTER
    /* pitch */
    filter_t * resampler;
    _Atomic float rate_shift;
#endif
} filter_sys_t;

/*****************************************************************************
 * best_overlap_offset: calculate best offset for overlap
 *****************************************************************************/
static unsigned best_overlap_offset_float( filter_t *p_filter )
{
    filter_sys_t *p = p_filter->p_sys;
    float *pw, *po, *ppc, *search_start;
    float best_corr = INT_MIN;
    unsigned best_off = 0;
    unsigned i, off;

    pw  = p->table_window;
    po  = p->buf_overlap;
    po += p->samples_per_frame;
    ppc = p->buf_pre_corr;
    for( i = p->samples_per_frame; i < p->samples_overlap; i++ ) {
      *ppc++ = *pw++ * *po++;
    }

    search_start = (float *)p->buf_queue + p->samples_per_frame;
    for( off = 0; off < p->frames_search; off++ ) {
      float corr = 0;
      float *ps = search_start;
      ppc = p->buf_pre_corr;
      for( i = p->samples_per_frame; i < p->samples_overlap; i++ ) {
        corr += *ppc++ * *ps++;
      }
      if( corr > best_corr ) {
        best_corr = corr;
        best_off  = off;
      }
      search_start += p->samples_per_frame;
    }

    return best_off * p->bytes_per_frame;
}

/*****************************************************************************
 * output_overlap: blend end of previous stride with beginning of current stride
 *****************************************************************************/
static void output_overlap_float( filter_t        *p_filter,
                                  void            *buf_out,
                                  unsigned         bytes_off )
{
    filter_sys_t *p = p_filter->p_sys;
    float *pout = buf_out;
    float *pb   = p->table_blend;
    float *po   = p->buf_overlap;
    float *pin  = (float *)( p->buf_queue + bytes_off );
    unsigned i;
    for( i = 0; i < p->samples_overlap; i++ ) {
        *pout++ = *po - *pb++ * ( *po - *pin++ ); po++;
    }
}

/*****************************************************************************
 * fill_queue: fill p_sys->buf_queue as much possible, skipping samples as needed
 *****************************************************************************/
static size_t fill_queue( filter_t      *p_filter,
                          uint8_t       *p_buffer,
                          size_t         i_buffer,
                          size_t         offset )
{
    filter_sys_t *p = p_filter->p_sys;
    unsigned bytes_in = i_buffer - offset;
    size_t offset_unchanged = offset;

    if( p->bytes_to_slide > 0 ) {
        if( p->bytes_to_slide < p->bytes_queued ) {
            unsigned bytes_in_move = p->bytes_queued - p->bytes_to_slide;
            memmove( p->buf_queue,
                     p->buf_queue + p->bytes_to_slide,
                     bytes_in_move );
            p->bytes_to_slide = 0;
            p->bytes_queued   = bytes_in_move;
        } else {
            unsigned bytes_in_skip;
            p->bytes_to_slide -= p->bytes_queued;
            bytes_in_skip      = __MIN( p->bytes_to_slide, bytes_in );
            p->bytes_queued    = 0;
            p->bytes_to_slide -= bytes_in_skip;
            offset            += bytes_in_skip;
            bytes_in          -= bytes_in_skip;
        }
    }

    if( bytes_in > 0 ) {
        unsigned bytes_in_copy = __MIN( p->bytes_queue_max - p->bytes_queued, bytes_in );
        memcpy( p->buf_queue + p->bytes_queued,
                p_buffer + offset,
                bytes_in_copy );
        p->bytes_queued += bytes_in_copy;
        offset          += bytes_in_copy;
    }

    return offset - offset_unchanged;
}

/*****************************************************************************
 * transform_buffer: main filter loop
 *****************************************************************************/
static size_t transform_buffer( filter_t *p_filter,
                                uint8_t *pout, size_t i_max_bytes_out )
{
    filter_sys_t *p = p_filter->p_sys;
    unsigned bytes_out, bytes_off = 0;

    assert( i_max_bytes_out >= p->bytes_stride );
    // output stride
    if( p->output_overlap ) {
        if( p->best_overlap_offset ) {
            bytes_off = p->best_overlap_offset( p_filter );
        }
        p->output_overlap( p_filter, pout, bytes_off );
    }
    memcpy( pout + p->bytes_overlap,
            p->buf_queue + bytes_off + p->bytes_overlap,
            p->bytes_standing );
    bytes_out = p->bytes_stride;

    // input stride
    memcpy( p->buf_overlap,
            p->buf_queue + bytes_off + p->bytes_stride,
            p->bytes_overlap );
    double frames_to_slide = p->frames_stride_scaled + p->frames_stride_error;
    unsigned frames_to_stride_whole = (int)frames_to_slide;
    p->bytes_to_slide       = frames_to_stride_whole * p->bytes_per_frame;
    p->frames_stride_error  = frames_to_slide - frames_to_stride_whole;

    return bytes_out;
}

/*****************************************************************************
 * calculate_output_buffer_size
 *****************************************************************************/
static size_t calculate_output_buffer_size( filter_t        *p_filter,
                                            size_t           bytes_in )
{
    filter_sys_t *p = p_filter->p_sys;
    size_t bytes_out = 0;
    int bytes_to_out = bytes_in + p->bytes_queued - p->bytes_to_slide;
    if( bytes_to_out >= (int)p->bytes_queue_max ) {
      /* while (total_buffered - stride_length * n >= queue_max) n++ */
      bytes_out = p->bytes_stride * ( (unsigned)(
          ( bytes_to_out - p->bytes_queue_max + /* rounding protection */ p->bytes_per_frame )
          / p->bytes_stride_scaled ) + 1 );
    }
    return bytes_out;
}

/*****************************************************************************
 * reinit_buffers: reinitializes buffers in p_filter->p_sys
 *****************************************************************************/
static int reinit_buffers( filter_t *p_filter )
{
    filter_sys_t *p = p_filter->p_sys;
    unsigned i,j;

    unsigned frames_stride = p->ms_stride * p->sample_rate / 1000.0;
    p->bytes_stride = frames_stride * p->bytes_per_frame;

    /* overlap */
    unsigned frames_overlap = frames_stride * p->percent_overlap;
    if( frames_overlap < 1 )
    { /* if no overlap */
        p->bytes_overlap    = 0;
        p->bytes_standing   = p->bytes_stride;
        p->samples_standing = p->bytes_standing / p->bytes_per_sample;
        p->output_overlap   = NULL;
    }
    else
    {
        unsigned prev_overlap   = p->bytes_overlap;
        p->bytes_overlap    = frames_overlap * p->bytes_per_frame;
        p->samples_overlap  = frames_overlap * p->samples_per_frame;
        p->bytes_standing   = p->bytes_stride - p->bytes_overlap;
        p->samples_standing = p->bytes_standing / p->bytes_per_sample;
        p->buf_overlap      = vlc_alloc( 1, p->bytes_overlap );
        p->table_blend      = vlc_alloc( 4, p->samples_overlap ); /* sizeof (int32|float) */
        if( !p->buf_overlap || !p->table_blend )
            return VLC_ENOMEM;
        if( p->bytes_overlap > prev_overlap )
            memset( (uint8_t *)p->buf_overlap + prev_overlap, 0, p->bytes_overlap - prev_overlap );

        float *pb = p->table_blend;
        float t = (float)frames_overlap;
        for( i = 0; i<frames_overlap; i++ )
        {
            float v = i / t;
            for( j = 0; j < p->samples_per_frame; j++ )
                *pb++ = v;
        }
        p->output_overlap = output_overlap_float;
    }

    /* best overlap */
    p->frames_search = ( frames_overlap <= 1 ) ? 0 : p->ms_search * p->sample_rate / 1000.0;
    if( p->frames_search < 1 )
    { /* if no search */
        p->best_overlap_offset = NULL;
    }
    else
    {
        unsigned bytes_pre_corr = ( p->samples_overlap - p->samples_per_frame ) * 4; /* sizeof (int32|float) */
        p->buf_pre_corr = malloc( bytes_pre_corr );
        p->table_window = malloc( bytes_pre_corr );
        if( ! p->buf_pre_corr || ! p->table_window )
            return VLC_ENOMEM;
        float *pw = p->table_window;
        for( i = 1; i<frames_overlap; i++ )
        {
            float v = i * ( frames_overlap - i );
            for( j = 0; j < p->samples_per_frame; j++ )
                *pw++ = v;
        }
        p->best_overlap_offset = best_overlap_offset_float;
    }

    unsigned new_size = ( p->frames_search + frames_stride + frames_overlap ) * p->bytes_per_frame;
    if( p->bytes_queued > new_size )
    {
        if( p->bytes_to_slide > p->bytes_queued )
        {
          p->bytes_to_slide -= p->bytes_queued;
          p->bytes_queued    = 0;
        }
        else
        {
            unsigned new_queued = __MIN( p->bytes_queued - p->bytes_to_slide, new_size );
            memmove( p->buf_queue,
                     p->buf_queue + p->bytes_queued - new_queued,
                     new_queued );
            p->bytes_to_slide = 0;
            p->bytes_queued   = new_queued;
        }
    }
    p->bytes_queue_max = new_size;
    p->buf_queue = malloc( p->bytes_queue_max );
    if( ! p->buf_queue )
        return VLC_ENOMEM;

    p->bytes_stride_scaled  = p->bytes_stride * p->scale;
    p->frames_stride_scaled = p->bytes_stride_scaled / p->bytes_per_frame;

    msg_Dbg( VLC_OBJECT(p_filter),
             "%.3f scale, %.3f stride_in, %i stride_out, %i standing, %i overlap, %i search, %i queue, %s mode",
             p->scale,
             p->frames_stride_scaled,
             (int)( p->bytes_stride / p->bytes_per_frame ),
             (int)( p->bytes_standing / p->bytes_per_frame ),
             (int)( p->bytes_overlap / p->bytes_per_frame ),
             p->frames_search,
             (int)( p->bytes_queue_max / p->bytes_per_frame ),
             "fl32");

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open: initialize as "audio filter"
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    filter_sys_t *p_sys = p_filter->p_sys = malloc( sizeof(*p_sys) );
    if( ! p_sys )
        return VLC_ENOMEM;

    p_sys->scale             = 1.0;
    p_sys->sample_rate       = p_filter->fmt_in.audio.i_rate;
    p_sys->samples_per_frame = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    p_sys->bytes_per_sample  = 4;
    p_sys->bytes_per_frame   = p_sys->samples_per_frame * p_sys->bytes_per_sample;

    msg_Dbg( p_this, "format: %5i rate, %i nch, %i bps, %s",
             p_sys->sample_rate,
             p_sys->samples_per_frame,
             p_sys->bytes_per_sample,
             "fl32" );

    p_sys->ms_stride       = var_InheritInteger( p_this, "scaletempo-stride" );
    p_sys->percent_overlap = var_InheritFloat( p_this, "scaletempo-overlap" );
    p_sys->ms_search       = var_InheritInteger( p_this, "scaletempo-search" );

    msg_Dbg( p_this, "params: %i stride, %.3f overlap, %i search",
             p_sys->ms_stride, p_sys->percent_overlap, p_sys->ms_search );

    p_sys->buf_queue      = NULL;
    p_sys->buf_overlap    = NULL;
    p_sys->table_blend    = NULL;
    p_sys->buf_pre_corr   = NULL;
    p_sys->table_window   = NULL;
    p_sys->bytes_overlap  = 0;
    p_sys->bytes_queued   = 0;
    p_sys->bytes_to_slide = 0;
    p_sys->frames_stride_error = 0;

    if( reinit_buffers( p_filter ) != VLC_SUCCESS )
    {
        Close( p_filter );
        return VLC_EGENERIC;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&p_filter->fmt_in.audio);
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    static const struct vlc_filter_operations filter_ops =
    {
        .filter_audio = DoWork, .close = Close,
    };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

#ifdef PITCH_SHIFTER
static inline void PitchSetRateShift( filter_sys_t *p_sys, float pitch_shift )
{
    atomic_store( &p_sys->rate_shift,
                  p_sys->sample_rate / powf(2, pitch_shift / 12) );
}

static int PitchCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( p_this );
    VLC_UNUSED( oldval );
    VLC_UNUSED( psz_var );

    PitchSetRateShift( p_data, newval.f_float );

    return VLC_SUCCESS;
}

static filter_t *ResamplerCreate(filter_t *p_filter)
{
    filter_t *p_resampler = vlc_object_create( p_filter, sizeof (filter_t) );
    if( unlikely( p_resampler == NULL ) )
        return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;
    p_resampler->p_cfg = NULL;
    p_resampler->fmt_in = p_filter->fmt_in;
    p_resampler->fmt_out = p_filter->fmt_in;
    p_resampler->fmt_out.audio.i_rate = atomic_load( &p_sys->rate_shift );
    aout_FormatPrepare( &p_resampler->fmt_out.audio );
    p_resampler->p_module = module_need( p_resampler, "audio resampler", NULL,
                                         false );

    if( p_resampler->p_module == NULL )
    {
        msg_Err( p_filter, "Could not load resampler" );
        vlc_object_delete(p_resampler);
        return NULL;
    }
    assert( p_resampler->ops != NULL );
    return p_resampler;
}

static int OpenPitch( vlc_object_t *p_this )
{
    int err = Open( p_this );
    if( err )
        return err;

    filter_t     *p_filter = (filter_t *)p_this;
    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    filter_sys_t *p_sys = p_filter->p_sys;

    float pitch_shift  = var_CreateGetFloat( p_aout, "pitch-shift" );
    var_AddCallback( p_aout, "pitch-shift", PitchCallback, p_sys );
    PitchSetRateShift( p_sys, pitch_shift );

    p_sys->resampler = ResamplerCreate(p_filter);
    if( !p_sys->resampler )
        return VLC_EGENERIC;

    static const struct vlc_filter_operations filter_ops =
    {
        .filter_audio = DoPitchWork, .close = ClosePitch,
    };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}
#endif

static void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    free( p_sys->buf_queue );
    free( p_sys->buf_overlap );
    free( p_sys->table_blend );
    free( p_sys->buf_pre_corr );
    free( p_sys->table_window );
    free( p_sys );
}

#ifdef PITCH_SHIFTER
static void ClosePitch( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    var_DelCallback( p_aout, "pitch-shift", PitchCallback, p_sys );
    var_Destroy( p_aout, "pitch-shift" );
    filter_Close( p_sys->resampler );
    module_unneed( p_sys->resampler, p_sys->resampler->p_module );
    vlc_object_delete(p_sys->resampler);
    Close( p_filter );
}
#endif

/*****************************************************************************
 * DoWork: filter wrapper for transform_buffer
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t * p_in_buf )
{
    filter_sys_t *p = p_filter->p_sys;

    if( p_filter->fmt_in.audio.i_rate == p->sample_rate )
        return p_in_buf;

    double scale = p_filter->fmt_in.audio.i_rate / (double)p->sample_rate;
    if( scale != p->scale ) {
        p->scale = scale;
        p->bytes_stride_scaled  = p->bytes_stride * p->scale;
        p->frames_stride_scaled = p->bytes_stride_scaled / p->bytes_per_frame;
        p->bytes_to_slide = 0;
        msg_Dbg( p_filter, "%.3f scale, %.3f stride_in, %i stride_out rate: %u",
                 p->scale, p->frames_stride_scaled,
                 (int)( p->bytes_stride / p->bytes_per_frame ), p->sample_rate );
    }

    block_t *p_out_buf = NULL;
    size_t i_outsize = calculate_output_buffer_size ( p_filter, p_in_buf->i_buffer );

    size_t offset_in = fill_queue( p_filter, p_in_buf->p_buffer,
                                   p_in_buf->i_buffer, 0 );
    if( i_outsize > 0 )
    {
        p_out_buf = block_Alloc( i_outsize );
        if( p_out_buf == NULL )
        {
            block_Release( p_in_buf );
            return NULL;
        }
        size_t bytes_out = 0;
        while( p->bytes_queued >= p->bytes_queue_max )
        {
            bytes_out += transform_buffer( p_filter,
                                           &p_out_buf->p_buffer[bytes_out],
                                           p_out_buf->i_buffer - bytes_out );
            offset_in += fill_queue( p_filter, p_in_buf->p_buffer,
                                     p_in_buf->i_buffer, offset_in );
        }
        p_out_buf->i_buffer     = bytes_out;
        p_out_buf->i_nb_samples = bytes_out / p->bytes_per_frame;
        p_out_buf->i_dts        = p_in_buf->i_dts;
        p_out_buf->i_pts        = p_in_buf->i_pts;
        p_out_buf->i_length = vlc_tick_from_samples(p_out_buf->i_nb_samples,
                                                    p_filter->fmt_out.audio.i_rate);
    }

    block_Release( p_in_buf );
    return p_out_buf;
}

#ifdef PITCH_SHIFTER
static block_t *DoPitchWork( filter_t * p_filter, block_t * p_in_buf )
{
    filter_sys_t *p = p_filter->p_sys;

    float rate_shift = atomic_load( &p->rate_shift );

    /* Set matching rates for resampler's output and scaletempo's input */
    p->resampler->fmt_out.audio.i_rate = rate_shift;
    p_filter->fmt_in.audio.i_rate = rate_shift;

    /* Change rate, thus changing pitch */
    p_in_buf = p->resampler->ops->filter_audio( p->resampler, p_in_buf );

    /* Change tempo while preserving shifted pitch */
    return DoWork( p_filter, p_in_buf );
}
#endif
