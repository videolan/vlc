/*****************************************************************************
 * voc.c : Creative Voice File (.VOC) demux module for vlc
 *****************************************************************************
 * Copyright (C) 2005 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("VOC demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 10 )
    set_callback( Open )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

typedef struct
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    int64_t         i_block_start;
    int64_t         i_block_end;

    int64_t         i_loop_offset;
    unsigned        i_loop_count;
    unsigned        i_silence_countdown;

    date_t          pts;
} demux_sys_t;

static const char ct_header[] = "Creative Voice File\x1a";

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    const uint8_t *p_buf;
    uint16_t    i_data_offset, i_version;

    if( vlc_stream_Peek( p_demux->s, &p_buf, 26 ) < 26 )
        return VLC_EGENERIC;

    if( memcmp( p_buf, ct_header, 20 ) )
        return VLC_EGENERIC;
    p_buf += 20;

    i_data_offset = GetWLE( p_buf );
    if ( i_data_offset < 26 /* not enough room for full VOC header */ )
        return VLC_EGENERIC;
    p_buf += 2;

    i_version = GetWLE( p_buf );
    if( ( i_version != 0x10A ) && ( i_version != 0x114 ) )
        return VLC_EGENERIC; /* unknown VOC version */
    p_buf += 2;

    if( GetWLE( p_buf ) != (uint16_t)(0x1234 + ~i_version) )
        return VLC_EGENERIC;

    /* We have a valid VOC header */
    msg_Dbg( p_demux, "CT Voice file v%d.%d", i_version >> 8,
             i_version & 0xff );

    /* skip VOC header */
    if( vlc_stream_Read( p_demux->s, NULL, i_data_offset ) < i_data_offset )
        return VLC_EGENERIC;

    demux_sys_t *p_sys = vlc_obj_malloc( p_this, sizeof (*p_sys) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_sys->i_silence_countdown = p_sys->i_block_start = p_sys->i_block_end =
    p_sys->i_loop_count = 0;
    p_sys->p_es = NULL;

    date_Init( &p_sys->pts, 1, 1 );
    date_Set( &p_sys->pts, VLC_TICK_0 );

    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys;

    return VLC_SUCCESS;
}

static int fmtcmp( es_format_t *ofmt, es_format_t *nfmt )
{
    return (ofmt->audio.i_bitspersample != nfmt->audio.i_bitspersample)
        || (ofmt->audio.i_rate != nfmt->audio.i_rate)
        || (ofmt->audio.i_channels != nfmt->audio.i_channels);
}


/*
 * Converts old-style VOC sample rates to commonly used ones
 * so as not to confuse sound card drivers.
 * (I assume 16k, 24k and 32k are never found in .VOC files)
 */
static unsigned int fix_voc_sr( unsigned int sr )
{
    switch( sr )
    {
        /*case 8000:
            return 8000;*/
        case 11111:
            return 11025;

        case 22222:
            return 22050;

        case 44444:
            return 44100;
    }
    return sr;
}

static int ReadBlockHeader( demux_t *p_demux )
{
    es_format_t     new_fmt;
    uint8_t buf[8];
    int32_t i_block_size;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( vlc_stream_Read( p_demux->s, buf, 4 ) < 4 )
        return VLC_EGENERIC; /* EOF */

    i_block_size = GetDWLE( buf ) >> 8;
    msg_Dbg( p_demux, "new block: type: %u, size: %u",
             (unsigned)*buf, i_block_size );

    es_format_Init( &new_fmt, AUDIO_ES, 0 );

    switch( *buf )
    {
        case 0: /* not possible : caught with earlier vlc_stream_Read */
            goto corrupt;

        case 1:
            if( i_block_size < 2 )
                goto corrupt;
            i_block_size -= 2;

            if( vlc_stream_Read( p_demux->s, buf, 2 ) < 2 )
                goto corrupt;

            switch( buf[1] ) /* codec id */
            {
            case 0x0:
                new_fmt.i_codec = VLC_CODEC_U8;
                new_fmt.audio.i_bytes_per_frame = 1;
                new_fmt.audio.i_bitspersample = 8;
                break;
            case 0x1:
                new_fmt.i_codec = VLC_CODEC_ADPCM_SBPRO_4;
                new_fmt.audio.i_bytes_per_frame = 1;
                new_fmt.audio.i_bitspersample = 4;
                break;
            case 0x2:
                new_fmt.i_codec = VLC_CODEC_ADPCM_SBPRO_3;
                new_fmt.audio.i_bytes_per_frame = 3;
                new_fmt.audio.i_bitspersample = 3;
                break;
            case 0x3:
                new_fmt.i_codec = VLC_CODEC_ADPCM_SBPRO_2;
                new_fmt.audio.i_bytes_per_frame = 1;
                new_fmt.audio.i_bitspersample = 2;
                break;
            case 0x4:
                new_fmt.i_codec = VLC_CODEC_S16L;
                new_fmt.audio.i_bytes_per_frame = 2;
                new_fmt.audio.i_bitspersample = 16;
                break;
            case 0x6:
                new_fmt.i_codec = VLC_CODEC_ALAW;
                new_fmt.audio.i_bytes_per_frame = 1;
                new_fmt.audio.i_bitspersample = 8;
                break;
            case 0x7:
                new_fmt.i_codec = VLC_CODEC_MULAW;
                new_fmt.audio.i_bytes_per_frame = 1;
                new_fmt.audio.i_bitspersample = 8;
                break;
            default:
                msg_Err( p_demux, "unsupported compression 0x%"PRIx8, buf[1] );
                return VLC_EGENERIC;
            }

            new_fmt.audio.i_channels = 1;
            new_fmt.audio.i_bytes_per_frame *= new_fmt.audio.i_channels;
            new_fmt.audio.i_blockalign = new_fmt.audio.i_bytes_per_frame;

            new_fmt.audio.i_frame_length = new_fmt.audio.i_bytes_per_frame * 8
                                         / new_fmt.audio.i_bitspersample;

            new_fmt.audio.i_rate = fix_voc_sr( 1000000L / (256L - buf[0]) );
            new_fmt.i_bitrate = new_fmt.audio.i_rate * new_fmt.audio.i_bitspersample
                              * new_fmt.audio.i_channels;

            break;

        case 2: /* data block with same format as the previous one */
            if( p_sys->p_es == NULL )
                goto corrupt; /* no previous block! */

            memcpy( &new_fmt, &p_sys->fmt, sizeof( new_fmt ) );
            break;

        case 3: /* silence block */
            if( ( i_block_size != 3 )
             || ( vlc_stream_Read( p_demux->s, buf, 3 ) < 3 ) )
                goto corrupt;

            i_block_size = 0;
            p_sys->i_silence_countdown = GetWLE( buf );

            new_fmt.i_codec = VLC_CODEC_U8;
            new_fmt.audio.i_rate = fix_voc_sr( 1000000L / (256L - buf[0]) );
            new_fmt.audio.i_bytes_per_frame = 1;
            new_fmt.audio.i_frame_length = 1;
            new_fmt.audio.i_channels = 1;
            new_fmt.audio.i_blockalign = 1;
            new_fmt.audio.i_bitspersample = 8;
            new_fmt.i_bitrate = new_fmt.audio.i_rate * 8;
            break;

        case 6: /* repeat block */
            if( ( i_block_size != 2 )
             || ( vlc_stream_Read( p_demux->s, buf, 2 ) < 2 ) )
                goto corrupt;

            i_block_size = 0;
            p_sys->i_loop_count = GetWLE( buf );
            p_sys->i_loop_offset = vlc_stream_Tell( p_demux->s );
            break;

        case 7: /* repeat end block */
            if( i_block_size != 0 )
                goto corrupt;

            if( p_sys->i_loop_count > 0 )
            {
                if( vlc_stream_Seek( p_demux->s, p_sys->i_loop_offset ) )
                    msg_Warn( p_demux, "cannot loop: seek failed" );
                else
                    p_sys->i_loop_count--;
            }
            break;

        case 8:
            /*
             * Block 8 is a big kludge to add stereo support to block 1 :
             * A block of type 8 is always followed by a block of type 1
             * and specifies the number of channels in that 1-block
             * (normally block 1 are always mono). In practice, block type 9
             * is used for stereo rather than 8
             */
            if( ( i_block_size != 4 )
             || ( vlc_stream_Read( p_demux->s, buf, 4 ) < 4 ) )
                goto corrupt;

            if( buf[2] )
            {
                msg_Err( p_demux, "unsupported compression" );
                return VLC_EGENERIC;
            }

            new_fmt.i_codec = VLC_CODEC_U8;
            if (buf[3] >= 32)
                goto corrupt;
            new_fmt.audio.i_channels = buf[3] + 1; /* can't be nul */
            new_fmt.audio.i_rate = 256000000L /
                          ((65536L - GetWLE(buf)) * new_fmt.audio.i_channels);
            new_fmt.audio.i_bytes_per_frame = new_fmt.audio.i_channels;
            new_fmt.audio.i_frame_length = 1;
            new_fmt.audio.i_blockalign = new_fmt.audio.i_bytes_per_frame;
            new_fmt.audio.i_bitspersample = 8 * new_fmt.audio.i_bytes_per_frame;
            new_fmt.i_bitrate = new_fmt.audio.i_rate * 8;

            /* read subsequent block 1 */
            if( vlc_stream_Read( p_demux->s, buf, 4 ) < 4 )
                return VLC_EGENERIC; /* EOF */

            i_block_size = GetDWLE( buf ) >> 8;
            msg_Dbg( p_demux, "new block: type: %u, size: %u",
                    (unsigned)*buf, i_block_size );
            if( i_block_size < 2 )
                goto corrupt;
            i_block_size -= 2;

            if( vlc_stream_Read( p_demux->s, buf, 2 ) < 2 )
                goto corrupt;

            if( buf[1] )
            {
                msg_Err( p_demux, "unsupported compression" );
                return VLC_EGENERIC;
            }

            break;

        case 9: /* newer data block with channel number and bits resolution */
            if( i_block_size < 12 )
                goto corrupt;
            i_block_size -= 12;

            if( ( vlc_stream_Read( p_demux->s, buf, 8 ) < 8 )
             || ( vlc_stream_Read( p_demux->s, NULL, 4 ) < 4 ) )
                goto corrupt;

            new_fmt.audio.i_rate = GetDWLE( buf );
            if( !new_fmt.audio.i_rate )
                goto corrupt;
            new_fmt.audio.i_bitspersample = buf[4];
            new_fmt.audio.i_channels = buf[5];

            switch( GetWLE( &buf[6] ) ) /* format */
            {
                case 0x0000: /* PCM */
                    switch( new_fmt.audio.i_bitspersample )
                    {
                        case 8:
                            new_fmt.i_codec = VLC_CODEC_U8;
                            break;

                        case 16:
                            new_fmt.i_codec = VLC_CODEC_U16L;
                            break;

                        default:
                            msg_Err( p_demux, "unsupported bit res.: %u bits",
                                     new_fmt.audio.i_bitspersample );
                            return VLC_EGENERIC;
                    }
                    break;

                case 0x0004: /* signed */
                    switch( new_fmt.audio.i_bitspersample )
                    {
                        case 8:
                            new_fmt.i_codec = VLC_CODEC_S8;
                            break;

                        case 16:
                            new_fmt.i_codec = VLC_CODEC_S16L;
                            break;

                        default:
                            msg_Err( p_demux, "unsupported bit res.: %u bits",
                                     new_fmt.audio.i_bitspersample );
                            return VLC_EGENERIC;
                    }
                    break;

                default:
                    msg_Err( p_demux, "unsupported compression" );
                    return VLC_EGENERIC;
            }

            if( new_fmt.audio.i_channels == 0 )
            {
                msg_Err( p_demux, "0 channels detected" );
                return VLC_EGENERIC;
            }

            new_fmt.audio.i_bytes_per_frame = new_fmt.audio.i_channels
                * (new_fmt.audio.i_bitspersample / 8);
            new_fmt.audio.i_frame_length = 1;
            new_fmt.audio.i_blockalign = new_fmt.audio.i_bytes_per_frame;
            new_fmt.i_bitrate = 8 * new_fmt.audio.i_rate
                                     * new_fmt.audio.i_bytes_per_frame;
            break;

        default:
            msg_Dbg( p_demux, "unknown block type %u - skipping block",
                     (unsigned)*buf);
            /* fall through */
        case 4: /* blocks of non-audio types can be skipped */
        case 5:
            if( vlc_stream_Read( p_demux->s, NULL,
                                 i_block_size ) < i_block_size )
                goto corrupt;
            i_block_size = 0;
            break;
    }

    p_sys->i_block_start = vlc_stream_Tell( p_demux->s );
    p_sys->i_block_end = p_sys->i_block_start + i_block_size;

    if( i_block_size || p_sys->i_silence_countdown )
    {
        /* we've read a block with data in it - update decoder */
        msg_Dbg( p_demux, "fourcc: %4.4s, channels: %d, "
                 "freq: %d Hz, bitrate: %dKo/s, blockalign: %d, "
                 "bits/samples: %d", (char *)&new_fmt.i_codec,
                 new_fmt.audio.i_channels, new_fmt.audio.i_rate,
                 new_fmt.i_bitrate / 8192, new_fmt.audio.i_blockalign,
                 new_fmt.audio.i_bitspersample );

        if( ( p_sys->p_es != NULL ) && fmtcmp( &p_sys->fmt, &new_fmt ) )
        {
            msg_Dbg( p_demux, "codec change needed" );
            es_out_Del( p_demux->out, p_sys->p_es );
            p_sys->p_es = NULL;
        }

        if( p_sys->p_es == NULL )
        {
            memcpy( &p_sys->fmt, &new_fmt, sizeof( p_sys->fmt ) );
            date_Change( &p_sys->pts, p_sys->fmt.audio.i_rate, 1 );
            p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );
            if( unlikely(p_sys->p_es == NULL) )
                return VLC_ENOMEM;
        }
    }

    return VLC_SUCCESS;

corrupt:
    msg_Err( p_demux, "corrupted file - halting demux" );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
#define MAX_READ_FRAME 1000
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;
    int64_t     i_read_frames;

    if( p_sys->i_silence_countdown == 0 )
    {
        int64_t i_offset = vlc_stream_Tell( p_demux->s );
        if( i_offset >= p_sys->i_block_end )
        {
            if( ReadBlockHeader( p_demux ) != VLC_SUCCESS )
                return VLC_DEMUXER_EOF;
            return VLC_DEMUXER_SUCCESS;
        }

        i_read_frames = ( p_sys->i_block_end - i_offset )
                      / p_sys->fmt.audio.i_bytes_per_frame;

        if( i_read_frames > MAX_READ_FRAME )
            i_read_frames = MAX_READ_FRAME;

        p_block = vlc_stream_Block( p_demux->s,
                                    p_sys->fmt.audio.i_bytes_per_frame
                                    * i_read_frames );
        if( p_block == NULL )
        {
            msg_Warn( p_demux, "cannot read data" );
            return VLC_DEMUXER_EOF;
        }
    }
    else
    {   /* emulates silence from the stream */
        i_read_frames = p_sys->i_silence_countdown;
        if( i_read_frames > MAX_READ_FRAME )
            i_read_frames = MAX_READ_FRAME;

        p_block = block_Alloc( i_read_frames );
        if( p_block == NULL )
            return VLC_ENOMEM;

        memset( p_block->p_buffer, 0, i_read_frames );
        p_sys->i_silence_countdown -= i_read_frames;
    }

    p_block->i_dts = p_block->i_pts = VLC_TICK_0 + date_Get( &p_sys->pts );
    p_block->i_nb_samples = i_read_frames * p_sys->fmt.audio.i_frame_length;
    date_Increment( &p_sys->pts, p_block->i_nb_samples );
    es_out_SetPCR( p_demux->out, p_block->i_pts );
    assert(p_sys->p_es != NULL);
    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    return demux_vaControlHelper( p_demux->s, p_sys->i_block_start,
                                   p_sys->i_block_end,
                                   p_sys->fmt.i_bitrate,
                                   p_sys->fmt.audio.i_blockalign,
                                   i_query, args );
}
