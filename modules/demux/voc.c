/*****************************************************************************
 * voc.c : Creative Voice File (.VOC) demux module for vlc
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id: xa.c 10182 2005-03-07 15:04:53Z courmisch $
 *
 * Authors: Remi Denis-Courmont <rem # via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/aout.h>

#include <codecs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("VOC demuxer") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability( "demux2", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

struct demux_sys_t
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    int64_t         i_block_offset;
    int32_t         i_block_size;

    date_t          pts;
};

static const char ct_header[] = "Creative Voice File\x1a";

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    uint8_t     *p_buf, buf[8];
    uint16_t    i_data_offset, i_version;

    if( stream_Peek( p_demux->s, &p_buf, 26 ) < 26 )
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
    if( stream_Read( p_demux->s, NULL, i_data_offset ) < i_data_offset )
        return VLC_EGENERIC;

    /* read first block header */
    /* FIXME: we only support one-block files at the moment */
    if( stream_Read( p_demux->s, buf, 4 ) < 4 )
        return VLC_EGENERIC;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );

    p_sys->i_block_size = GetDWLE( buf ) >> 8;
    msg_Dbg( p_demux, "block size: %u bytes", p_sys->i_block_size );

    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );

    switch( *buf )
    {
        case 1:
            if( p_sys->i_block_size < 2 )
                goto error;
            p_sys->i_block_size -= 2;

            if( stream_Read( p_demux->s, buf, 2 ) < 2 )
                goto error;

            p_sys->fmt.audio.i_rate = 1000000L / (256L - buf[0]);
            if( buf[1] )
            {
                msg_Err( p_demux, "Unsupported compression" );
                goto error;
            }

            p_sys->fmt.i_codec = VLC_FOURCC('u','8',' ',' ');
            p_sys->fmt.audio.i_bytes_per_frame = 1;
            p_sys->fmt.audio.i_frame_length = 1;
            p_sys->fmt.audio.i_channels = 1;
            p_sys->fmt.audio.i_blockalign = 1;
            p_sys->fmt.audio.i_bitspersample = 8;
            p_sys->fmt.i_bitrate = p_sys->fmt.audio.i_rate * 8;
            break;

        case 9:
            if( p_sys->i_block_size < 12 )
                goto error;
            p_sys->i_block_size -= 12;

            if( ( stream_Read( p_demux->s, buf, 8 ) < 8 )
             || ( stream_Read( p_demux->s, NULL, 4 ) < 4 ) )
                goto error;

            p_sys->fmt.audio.i_rate = GetDWLE( buf );
            p_sys->fmt.audio.i_bitspersample = buf[4];
            p_sys->fmt.audio.i_channels = buf[5];

            switch( GetWLE( &buf[6] ) ) /* format */
            {
                case 0x0000: /* PCM */
                    switch( p_sys->fmt.audio.i_bitspersample )
                    {
                        case 8:
                            p_sys->fmt.i_codec = VLC_FOURCC('u','8',' ',' ');
                            break;

                        case 16:
                            p_sys->fmt.i_codec = VLC_FOURCC('u','1','6','l');
                            break;

                        default:
                            msg_Err( p_demux, "Unsupported bit res.: %u bits",
                                     p_sys->fmt.audio.i_bitspersample );
                            return VLC_EGENERIC;
                    }
                    break;

                case 0x0004: /* signed */
                    switch( p_sys->fmt.audio.i_bitspersample )
                    {
                        case 8:
                            p_sys->fmt.i_codec = VLC_FOURCC('s','8',' ',' ');
                            break;

                        case 16:
                            p_sys->fmt.i_codec = VLC_FOURCC('s','1','6','l');
                            break;

                        default:
                            msg_Err( p_demux, "Unsupported bit res.: %u bits",
                                     p_sys->fmt.audio.i_bitspersample );
                            return VLC_EGENERIC;
                    }
                    break;

                default: 
                    msg_Err( p_demux, "Unsupported compression" );
                    goto error;
            }

            p_sys->fmt.audio.i_bytes_per_frame = p_sys->fmt.audio.i_channels
                * (p_sys->fmt.audio.i_bitspersample / 8);
            p_sys->fmt.audio.i_frame_length = 1;
            p_sys->fmt.audio.i_blockalign = p_sys->fmt.audio.i_bytes_per_frame;
            p_sys->fmt.i_bitrate = 8 * p_sys->fmt.audio.i_rate
                                     * p_sys->fmt.audio.i_bytes_per_frame;
            break;

        default:
            msg_Dbg( p_demux, "Unsupported block type %u", (unsigned)*p_buf);
            return VLC_EGENERIC;
    }
    p_sys->i_block_offset = stream_Tell( p_demux->s );

    p_sys->fmt.i_extra = 0;
    p_sys->fmt.p_extra = NULL;

    msg_Dbg( p_demux, "fourcc: %4.4s, channels: %d, "
             "freq: %d Hz, bitrate: %dKo/s, blockalign: %d, bits/samples: %d",
             (char *)&p_sys->fmt.i_codec, p_sys->fmt.audio.i_channels,
             p_sys->fmt.audio.i_rate, p_sys->fmt.i_bitrate / 8192,
             p_sys->fmt.audio.i_blockalign, p_sys->fmt.audio.i_bitspersample
             );

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );

    date_Init( &p_sys->pts, p_sys->fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, 1 );

    return VLC_SUCCESS;

error:
    msg_Err( p_demux, "Unsupported or malformatted file" );
    free( p_sys );
    return VLC_EGENERIC;
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
    int64_t     i_offset;

    i_offset = stream_Tell( p_demux->s );

    if( i_offset >= p_sys->i_block_offset + p_sys->i_block_size )
    {
        /* EOF */
        return 0;
    }

    p_block = stream_Block( p_demux->s, p_sys->fmt.audio.i_bytes_per_frame );
    if( p_block == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }

    p_block->i_dts = p_block->i_pts =
        date_Increment( &p_sys->pts, p_sys->fmt.audio.i_frame_length );

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    return 1;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_sys_t *p_sys  = ((demux_t *)p_this)->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    return demux2_vaControlHelper( p_demux->s, p_sys->i_block_offset,
                                   p_sys->i_block_offset + p_sys->i_block_size,
                                   p_sys->fmt.i_bitrate,
                                   p_sys->fmt.audio.i_blockalign,
                                   i_query, args );
}
