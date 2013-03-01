/*****************************************************************************
 * remap.c : simple channel remapper plug-in
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Authors: Cheng Sun <chengsun9@gmail.com>
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
#include <vlc_block.h>
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenFilter( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

#define REMAP_CFG "aout-remap-"

/* wg4 channel indices in the order of channel_name */
static const uint8_t channel_wg4idx[] = { 0, 7, 1, 4, 6, 5, 2, 3, 8 };

static const unsigned channel_idx[]    = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

static const char *const channel_name[] =
{
    REMAP_CFG "channel-left", REMAP_CFG "channel-center",
    REMAP_CFG "channel-right", REMAP_CFG "channel-rearleft",
    REMAP_CFG "channel-rearcenter", REMAP_CFG "channel-rearright",
    REMAP_CFG "channel-middleleft", REMAP_CFG "channel-middleright",
    REMAP_CFG "channel-lfe"
};

static const char *const channel_desc[] =
{
    N_( "Left" ), N_( "Center" ), N_( "Right" ),
    N_( "Rear left" ), N_( "Rear center" ), N_( "Rear right" ),
    N_( "Side left" ), N_( "Side right" ), N_( "Low-frequency effects" )
};

static const int channel_flag[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARCENTER, AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_LFE
};

vlc_module_begin ()
    set_description( N_("Audio channel remapper") )
    set_capability( "audio filter", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_callbacks( OpenFilter, CloseFilter )
    set_shortname( "Remap" )

#define CHANNEL( idx ) \
    add_integer( channel_name[idx], idx, channel_desc[idx], \
            channel_desc[idx], false) \
        change_integer_list( channel_idx, channel_desc )
    CHANNEL(0) CHANNEL(1) CHANNEL(2)
    CHANNEL(3) CHANNEL(4) CHANNEL(5)
    CHANNEL(6) CHANNEL(7) CHANNEL(8)
#undef CHANNEL

    add_bool( REMAP_CFG "normalize", true, "Normalize channels",
            "When mapping more than one channel to a single output channel, "
            "normalize the output accordingly.", false )

    set_callbacks( OpenFilter, CloseFilter )

vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static block_t *Remap( filter_t *, block_t * );

typedef void (*remap_fun_t)( filter_t *, const void *, void *,
                             int, unsigned, unsigned);

struct filter_sys_t
{
    remap_fun_t pf_remap;
    int nb_in_ch[AOUT_CHAN_MAX];
    uint8_t map_ch[AOUT_CHAN_MAX];
    bool b_normalize;
};

static const uint32_t valid_channels[] = {
/* list taken from aout_FormatPrintChannels */
    AOUT_CHAN_LEFT,
    AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER
          | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
};

static inline uint32_t CanonicaliseChannels( uint32_t i_physical_channels )
{
    for( unsigned i = 0; i < sizeof( valid_channels )/sizeof( valid_channels[0] ); i++ )
        if( (i_physical_channels & ~valid_channels[i]) == 0 )
            return valid_channels[i];

    assert( false );
    return 0;
}

/*****************************************************************************
 * Remap*: do remapping
 *****************************************************************************/
#define DEFINE_REMAP( name, type ) \
static void RemapCopy##name( filter_t *p_filter, \
                    const void *p_srcorig, void *p_destorig, \
                    int i_nb_samples, \
                    unsigned i_nb_in_channels, unsigned i_nb_out_channels ) \
{ \
    filter_sys_t *p_sys = ( filter_sys_t * )p_filter->p_sys; \
    const type *p_src = p_srcorig; \
    type *p_dest = p_destorig; \
 \
    for( int i = 0; i < i_nb_samples; i++ ) \
    { \
        for( uint8_t in_ch = 0; in_ch < i_nb_in_channels; in_ch++ ) \
        { \
            uint8_t out_ch = p_sys->map_ch[ in_ch ]; \
            memcpy( p_dest + out_ch, \
                    p_src  + in_ch, \
                    sizeof( type ) ); \
        } \
        p_src  += i_nb_in_channels; \
        p_dest += i_nb_out_channels; \
    } \
} \
 \
static void RemapAdd##name( filter_t *p_filter, \
                    const void *p_srcorig, void *p_destorig, \
                    int i_nb_samples, \
                    unsigned i_nb_in_channels, unsigned i_nb_out_channels ) \
{ \
    filter_sys_t *p_sys = ( filter_sys_t * )p_filter->p_sys; \
    const type *p_src = p_srcorig; \
    type *p_dest = p_destorig; \
 \
    for( int i = 0; i < i_nb_samples; i++ ) \
    { \
        for( uint8_t in_ch = 0; in_ch < i_nb_in_channels; in_ch++ ) \
        { \
            uint8_t out_ch = p_sys->map_ch[ in_ch ]; \
            if( p_sys->b_normalize ) \
                p_dest[ out_ch ] += p_src[ in_ch ] / p_sys->nb_in_ch[ out_ch ]; \
            else \
                p_dest[ out_ch ] += p_src[ in_ch ]; \
        } \
        p_src  += i_nb_in_channels; \
        p_dest += i_nb_out_channels; \
    } \
}

DEFINE_REMAP( U8,   uint8_t  )
DEFINE_REMAP( S16N, int16_t  )
DEFINE_REMAP( S32N, int32_t  )
DEFINE_REMAP( FL32, float    )
DEFINE_REMAP( FL64, double   )

#undef DEFINE_REMAP

static inline remap_fun_t GetRemapFun( audio_format_t *p_format, bool b_add )
{
    if( b_add )
    {
        switch( p_format->i_format )
        {
            case VLC_CODEC_U8:
                return RemapAddU8;
            case VLC_CODEC_S16N:
                return RemapAddS16N;
            case VLC_CODEC_S32N:
                return RemapAddS32N;
            case VLC_CODEC_FL32:
                return RemapAddFL32;
            case VLC_CODEC_FL64:
                return RemapAddFL64;
        }
    }
    else
    {
        switch( p_format->i_format )
        {
            case VLC_CODEC_U8:
                return RemapCopyU8;
            case VLC_CODEC_S16N:
                return RemapCopyS16N;
            case VLC_CODEC_S32N:
                return RemapCopyS32N;
            case VLC_CODEC_FL32:
                return RemapCopyFL32;
            case VLC_CODEC_FL64:
                return RemapCopyFL64;
        }
    }

    return NULL;
}


/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    audio_format_t *audio_in  = &p_filter->fmt_in.audio;
    audio_format_t *audio_out = &p_filter->fmt_out.audio;

    if( ( audio_in->i_format != audio_out->i_format ) ||
        ( audio_in->i_rate != audio_out->i_rate ) )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    /* get number of and layout of input channels */
    uint32_t i_output_physical = 0;
    uint8_t pi_map_ch[ AOUT_CHAN_MAX ] = { 0 }; /* which out channel each in channel is mapped to */
    p_sys->b_normalize = var_InheritBool( p_this, REMAP_CFG "normalize" );

    for( uint8_t in_ch = 0, wg4_i = 0; in_ch < audio_in->i_channels; in_ch++, wg4_i++ )
    {
        /* explode in_channels in the right order */
        while( ( audio_in->i_physical_channels & pi_vlc_chan_order_wg4[ wg4_i ] ) == 0 )
        {
            wg4_i++;
            assert( wg4_i < sizeof( pi_vlc_chan_order_wg4 )/sizeof( pi_vlc_chan_order_wg4[0] ) );
        }
        unsigned channel_wg4idx_len = sizeof( channel_wg4idx )/sizeof( channel_wg4idx[0] );
        uint8_t *pi_chnidx = memchr( channel_wg4idx, wg4_i, channel_wg4idx_len );
        assert( pi_chnidx != NULL );
        uint8_t chnidx = pi_chnidx - channel_wg4idx;
        uint8_t out_idx = var_InheritInteger( p_this, channel_name[chnidx] );
        pi_map_ch[in_ch] = channel_wg4idx[ out_idx ];

        i_output_physical |= channel_flag[ out_idx ];
    }
    i_output_physical = CanonicaliseChannels( i_output_physical );

    audio_out->i_physical_channels = i_output_physical;
    aout_FormatPrepare( audio_out );

    /* condense out_channels */
    uint8_t out_ch_sorted[ AOUT_CHAN_MAX ];
    for( uint8_t i = 0, wg4_i = 0; i < audio_out->i_channels; i++, wg4_i++ )
    {
        while( ( audio_out->i_physical_channels & pi_vlc_chan_order_wg4[ wg4_i ] ) == 0 )
        {
            wg4_i++;
            assert( wg4_i < sizeof( pi_vlc_chan_order_wg4 )/sizeof( pi_vlc_chan_order_wg4[0] ) );
        }
        out_ch_sorted[ i ] = wg4_i;
    }
    bool b_multiple = false; /* whether we need to add channels (multiple in mapped to an out) */
    memset( p_sys->nb_in_ch, 0, sizeof( p_sys->nb_in_ch ) );
    for( uint8_t i = 0; i < audio_in->i_channels; i++ )
    {
        uint8_t wg4_out_ch = pi_map_ch[i];
        uint8_t *pi_out_ch = memchr( out_ch_sorted, wg4_out_ch, audio_out->i_channels );
        assert( pi_out_ch != NULL );
        p_sys->map_ch[i] = pi_out_ch - out_ch_sorted;
        if( ++p_sys->nb_in_ch[ p_sys->map_ch[i] ] > 1 )
            b_multiple = true;
    }

    msg_Dbg( p_filter, "%s '%4.4s'->'%4.4s' %d Hz->%d Hz %s->%s",
             "Remap filter",
             (char *)&audio_in->i_format, (char *)&audio_out->i_format,
             audio_in->i_rate, audio_out->i_rate,
             aout_FormatPrintChannels( audio_in ),
             aout_FormatPrintChannels( audio_out ) );

    p_sys->pf_remap = GetRemapFun( audio_in, b_multiple );
    if( !p_sys->pf_remap )
    {
        msg_Err( p_filter, "Could not decide on %s remap function", b_multiple ? "an add" : "a copy" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = Remap;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter:
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *) p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    free( p_sys );
}

/*****************************************************************************
 * Remap:
 *****************************************************************************/
static block_t *Remap( filter_t *p_filter, block_t *p_block )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    if( !p_block || !p_block->i_nb_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    size_t i_out_size = p_block->i_nb_samples *
        p_filter->fmt_out.audio.i_bytes_per_frame;

    block_t *p_out = block_Alloc( i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }
    p_out->i_nb_samples = p_block->i_nb_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    memset( p_out->p_buffer, 0, i_out_size );

    p_sys->pf_remap( p_filter,
                (const void *)p_block->p_buffer, (void *)p_out->p_buffer,
                p_block->i_nb_samples,
                p_filter->fmt_in.audio.i_channels,
                p_filter->fmt_out.audio.i_channels );

    block_Release( p_block );

    return p_out;
}
