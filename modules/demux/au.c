/*****************************************************************************
 * au.c : au file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <limits.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

/* TODO:
 *  - all adpcm things (I _NEED_ samples)
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("AU demuxer") )
    set_capability( "demux", 10 )
    set_callback( Open )
    add_shortcut( "au" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
enum AuType_e
{
    AU_UNKNOWN      =  0,
    AU_MULAW_8      =  1,  /* 8-bit ISDN u-law */
    AU_LINEAR_8     =  2,  /* 8-bit linear PCM */
    AU_LINEAR_16    =  3,  /* 16-bit linear PCM */
    AU_LINEAR_24    =  4,  /* 24-bit linear PCM */
    AU_LINEAR_32    =  5,  /* 32-bit linear PCM */
    AU_FLOAT        =  6,  /* 32-bit IEEE floating point */
    AU_DOUBLE       =  7,  /* 64-bit IEEE floating point */
    AU_ADPCM_G721   =  23, /* 4-bit CCITT g.721 ADPCM */
    AU_ADPCM_G722   =  24, /* CCITT g.722 ADPCM */
    AU_ADPCM_G723_3 =  25, /* CCITT g.723 3-bit ADPCM */
    AU_ADPCM_G723_5 =  26, /* CCITT g.723 5-bit ADPCM */
    AU_ALAW_8       =  27  /* 8-bit ISDN A-law */
};

enum AuCat_e
{
    AU_CAT_UNKNOWN  = 0,
    AU_CAT_PCM      = 1,
    AU_CAT_ADPCM    = 2
};

typedef struct
{
    es_format_t     fmt;
    es_out_id_t     *es;

    vlc_tick_t      i_time;

    int             i_frame_size;
    vlc_tick_t      i_frame_length;

    uint32_t        i_header_size;
} demux_sys_t;

static int Demux( demux_t * );
static int Control ( demux_t *, int i_query, va_list args );

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;

    uint8_t      hdr[20];
    const uint8_t *p_peek;
    int          i_cat;

    if( vlc_stream_Peek( p_demux->s , &p_peek, 4 ) < 4 )
        return VLC_EGENERIC;

    if( memcmp( p_peek, ".snd", 4 ) )
        return VLC_EGENERIC;

    /* skip signature */
    if( vlc_stream_Read( p_demux->s, NULL, 4 ) < 4 )
        return VLC_EGENERIC;

    /* read header */
    if( vlc_stream_Read( p_demux->s, hdr, 20 ) < 20 )
    {
        msg_Err( p_demux, "cannot read" );
        return VLC_EGENERIC;
    }

    if( GetDWBE( &hdr[0]  ) < 24 )
    {
        msg_Err( p_demux, "invalid file" );
        return VLC_EGENERIC;
    }

    demux_sys_t *p_sys = vlc_obj_malloc( p_this, sizeof (*p_sys) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_sys->i_time = 0;
    p_sys->i_header_size = GetDWBE( &hdr[0] );

    /* skip extra header data */
    if( p_sys->i_header_size > 24 )
    {
#if (SSIZE_MAX <= INT32_MAX)
        if( p_sys->i_header_size > SSIZE_MAX )
            return VLC_EGENERIC;
#endif
        size_t skip = p_sys->i_header_size - 24;
        if( vlc_stream_Read( p_demux->s, NULL, skip ) < (ssize_t)skip )
            return VLC_EGENERIC;
    }

    /* init fmt */
    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );
    p_sys->fmt.audio.i_rate     = GetDWBE( &hdr[12] );
    p_sys->fmt.audio.i_channels = GetDWBE( &hdr[16] );

#if 0
    p_sys->au.i_header_size   = GetDWBE( &p_sys->au.i_header_size );
    p_sys->au.i_data_size     = GetDWBE( &p_sys->au.i_data_size );
    p_sys->au.i_encoding      = GetDWBE( &p_sys->au.i_encoding );
    p_sys->au.i_sample_rate   = GetDWBE( &p_sys->au.i_sample_rate );
    p_sys->au.i_channels      = GetDWBE( &p_sys->au.i_channels );
#endif
    switch( GetDWBE( &hdr[8] ) )
    {
        case AU_ALAW_8:        /* 8-bit ISDN A-law */
            p_sys->fmt.i_codec               = VLC_CODEC_ALAW;
            p_sys->fmt.audio.i_bitspersample = 8;
            p_sys->fmt.audio.i_blockalign    = 1 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_MULAW_8:       /* 8-bit ISDN u-law */
            p_sys->fmt.i_codec               = VLC_CODEC_MULAW;
            p_sys->fmt.audio.i_bitspersample = 8;
            p_sys->fmt.audio.i_blockalign    = 1 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_8:      /* 8-bit linear PCM */
            p_sys->fmt.i_codec               = VLC_CODEC_S8;
            p_sys->fmt.audio.i_bitspersample = 8;
            p_sys->fmt.audio.i_blockalign    = 1 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_16:     /* 16-bit linear PCM */
            p_sys->fmt.i_codec               = VLC_CODEC_S16B;
            p_sys->fmt.audio.i_bitspersample = 16;
            p_sys->fmt.audio.i_blockalign    = 2 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_24:     /* 24-bit linear PCM */
            p_sys->fmt.i_codec               = VLC_CODEC_S24B;
            p_sys->fmt.audio.i_bitspersample = 24;
            p_sys->fmt.audio.i_blockalign    = 3 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_32:     /* 32-bit linear PCM */
            p_sys->fmt.i_codec               = VLC_CODEC_S32B;
            p_sys->fmt.audio.i_bitspersample = 32;
            p_sys->fmt.audio.i_blockalign    = 4 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_FLOAT:         /* 32-bit IEEE floating point */
            p_sys->fmt.i_codec               = VLC_FOURCC( 'a', 'u', 0, AU_FLOAT );
            p_sys->fmt.audio.i_bitspersample = 32;
            p_sys->fmt.audio.i_blockalign    = 4 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_DOUBLE:        /* 64-bit IEEE floating point */
            p_sys->fmt.i_codec               = VLC_FOURCC( 'a', 'u', 0, AU_DOUBLE );
            p_sys->fmt.audio.i_bitspersample = 64;
            p_sys->fmt.audio.i_blockalign    = 8 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_ADPCM_G721:    /* 4-bit CCITT g.721 ADPCM */
            p_sys->fmt.i_codec               = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G721 );
            p_sys->fmt.audio.i_bitspersample = 0;
            p_sys->fmt.audio.i_blockalign    = 0 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_ADPCM;
            break;

        case AU_ADPCM_G722:    /* CCITT g.722 ADPCM */
            p_sys->fmt.i_codec               = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G722 );
            p_sys->fmt.audio.i_bitspersample = 0;
            p_sys->fmt.audio.i_blockalign    = 0 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_ADPCM;
            break;

        case AU_ADPCM_G723_3:  /* CCITT g.723 3-bit ADPCM */
            p_sys->fmt.i_codec               = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G723_3 );
            p_sys->fmt.audio.i_bitspersample = 0;
            p_sys->fmt.audio.i_blockalign    = 0 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_ADPCM;
            break;

        case AU_ADPCM_G723_5:  /* CCITT g.723 5-bit ADPCM */
            p_sys->fmt.i_codec               = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G723_5 );
            p_sys->fmt.audio.i_bitspersample = 0;
            p_sys->fmt.audio.i_blockalign    = 0 * p_sys->fmt.audio.i_channels;
            i_cat                    = AU_CAT_ADPCM;
            break;

        default:
            msg_Warn( p_demux, "unknown encoding=0x%x", GetDWBE( &hdr[8] ) );
            p_sys->fmt.audio.i_bitspersample = 0;
            p_sys->fmt.audio.i_blockalign    = 0;
            i_cat                    = AU_CAT_UNKNOWN;
            break;
    }

    p_sys->fmt.i_bitrate = p_sys->fmt.audio.i_rate *
                           p_sys->fmt.audio.i_channels *
                           p_sys->fmt.audio.i_bitspersample;

    if( i_cat == AU_CAT_UNKNOWN || i_cat == AU_CAT_ADPCM )
    {
        msg_Err( p_demux, "unsupported codec/type (Please report it)" );
        return VLC_EGENERIC;
    }

    if( p_sys->fmt.audio.i_rate == 0 )
    {
        msg_Err( p_demux, "invalid samplerate: 0" );
        return VLC_EGENERIC;
    }

    /* add the es */
    p_sys->fmt.i_id = 0;
    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );
    if( unlikely(p_sys->es == NULL) )
        return VLC_ENOMEM;

    /* calculate 50ms frame size/time */
    unsigned i_samples = __MAX( p_sys->fmt.audio.i_rate / 20, 1 );
    p_sys->i_frame_size = i_samples * p_sys->fmt.audio.i_channels *
                          ( (p_sys->fmt.audio.i_bitspersample + 7) / 8 );
    if( p_sys->fmt.audio.i_blockalign > 0 )
    {
        unsigned mod = p_sys->i_frame_size % p_sys->fmt.audio.i_blockalign;
        if( mod != 0 )
        {
            p_sys->i_frame_size += p_sys->fmt.audio.i_blockalign - mod;
        }
    }
    p_sys->i_frame_length = vlc_tick_from_samples( i_samples,
                                                   p_sys->fmt.audio.i_rate );

    p_demux->p_sys = p_sys;
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    /* set PCR */
    es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time );

    p_block = vlc_stream_Block( p_demux->s, p_sys->i_frame_size );
    if( p_block == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return VLC_DEMUXER_EOF;
    }

    p_block->i_dts =
    p_block->i_pts = VLC_TICK_0 + p_sys->i_time;
    es_out_Send( p_demux->out, p_sys->es, p_block );

    p_sys->i_time += p_sys->i_frame_length;

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    return demux_vaControlHelper( p_demux->s, p_sys->i_header_size, -1,
                                   p_sys->fmt.i_bitrate, p_sys->fmt.audio.i_blockalign,
                                   i_query, args );
}

