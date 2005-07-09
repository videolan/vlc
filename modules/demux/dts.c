/*****************************************************************************
 * dts.c : raw DTS stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc/input.h>
#include <vlc_codec.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("Raw DTS demuxer") );
    set_capability( "demux2", 155 );
    set_callbacks( Open, Close );
    add_shortcut( "dts" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    vlc_bool_t  b_start;
    es_out_id_t *p_es;

    /* Packetizer */
    decoder_t *p_packetizer;

    int i_mux_rate;
};

static int CheckSync( uint8_t *p_peek );

#define DTS_PACKET_SIZE 16384
#define DTS_PROBE_SIZE (DTS_PACKET_SIZE * 4)
#define DTS_MAX_HEADER_SIZE 11

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    byte_t *     p_peek;
    int          i_peek = 0;

    /* Check if we are dealing with a WAV file */
    if( stream_Peek( p_demux->s, &p_peek, 20 ) == 20 &&
        !strncmp( p_peek, "RIFF", 4 ) && !strncmp( &p_peek[8], "WAVE", 4 ) )
    {
        int i_size;

        /* Find the wave format header */
        i_peek = 20;
        while( strncmp( p_peek + i_peek - 8, "fmt ", 4 ) )
        {
            i_size = GetDWLE( p_peek + i_peek - 4 );
            if( i_size + i_peek > DTS_PROBE_SIZE ) return VLC_EGENERIC;
            i_peek += i_size + 8;

            if( stream_Peek( p_demux->s, &p_peek, i_peek ) != i_peek )
                return VLC_EGENERIC;
        }

        /* Sanity check the wave format header */
        i_size = GetDWLE( p_peek + i_peek - 4 );
        if( i_size + i_peek > DTS_PROBE_SIZE ) return VLC_EGENERIC;
        i_peek += i_size + 8;
        if( stream_Peek( p_demux->s, &p_peek, i_peek ) != i_peek )
            return VLC_EGENERIC;
        if( GetWLE( p_peek + i_peek - i_size - 8 /* wFormatTag */ ) !=
            1 /* WAVE_FORMAT_PCM */ )
            return VLC_EGENERIC;
        if( GetWLE( p_peek + i_peek - i_size - 6 /* nChannels */ ) != 2 )
            return VLC_EGENERIC;
        if( GetDWLE( p_peek + i_peek - i_size - 4 /* nSamplesPerSec */ ) !=
            44100 )
            return VLC_EGENERIC;

        /* Skip the wave header */
        while( strncmp( p_peek + i_peek - 8, "data", 4 ) )
        {
            i_size = GetDWLE( p_peek + i_peek - 4 );
            if( i_size + i_peek > DTS_PROBE_SIZE ) return VLC_EGENERIC;
            i_peek += i_size + 8;

            if( stream_Peek( p_demux->s, &p_peek, i_peek ) != i_peek )
                return VLC_EGENERIC;
        }

        /* Some DTS wav files don't begin with a sync code so we do a more
         * extensive search */
        i_size = stream_Peek( p_demux->s, &p_peek, DTS_PROBE_SIZE );
        i_size -= DTS_MAX_HEADER_SIZE;

        while( i_peek < i_size )
        {
            if( CheckSync( p_peek + i_peek ) != VLC_SUCCESS )
                /* The data is stored in 16 bits words */
                i_peek += 2;
            else
                break;
        }
    }

    /* Have a peep at the show. */
    if( stream_Peek( p_demux->s, &p_peek, i_peek + DTS_MAX_HEADER_SIZE * 2 ) <
        i_peek + DTS_MAX_HEADER_SIZE * 2 )
    {
        /* Stream too short */
        msg_Warn( p_demux, "cannot peek()" );
        return VLC_EGENERIC;
    }

    if( CheckSync( p_peek + i_peek ) != VLC_SUCCESS )
    {
        if( strncmp( p_demux->psz_demux, "dts", 3 ) )
        {
            return VLC_EGENERIC;
        }
        /* User forced */
        msg_Err( p_demux, "this doesn't look like a DTS audio stream, "
                 "continuing anyway" );
    }

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->b_start = VLC_TRUE;
    p_sys->i_mux_rate = 0;

    /*
     * Load the DTS packetizer
     */
    p_sys->p_packetizer = vlc_object_create( p_demux, VLC_OBJECT_DECODER );
    p_sys->p_packetizer->pf_decode_audio = 0;
    p_sys->p_packetizer->pf_decode_video = 0;
    p_sys->p_packetizer->pf_decode_sub = 0;
    p_sys->p_packetizer->pf_packetize = 0;

    /* Initialization of decoder structure */
    es_format_Init( &p_sys->p_packetizer->fmt_in, AUDIO_ES,
                    VLC_FOURCC( 'd', 't', 's', ' ' ) );

    p_sys->p_packetizer->p_module =
        module_Need( p_sys->p_packetizer, "packetizer", NULL, 0 );
    if( !p_sys->p_packetizer->p_module )
    {
        msg_Err( p_demux, "cannot find DTS packetizer" );
        return VLC_EGENERIC;
    }

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_in );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Unneed module */
    module_Unneed( p_sys->p_packetizer, p_sys->p_packetizer->p_module );

    /* Delete the decoder */
    vlc_object_destroy( p_sys->p_packetizer );

    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    if( !( p_block_in = stream_Block( p_demux->s, DTS_PACKET_SIZE ) ) )
    {
        return 0;
    }

    if( p_sys->b_start )
        p_block_in->i_pts = p_block_in->i_dts = 1;
    else
        p_block_in->i_pts = p_block_in->i_dts = 0;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                p_sys->p_packetizer, &p_block_in )) )
    {
        p_sys->b_start = VLC_FALSE;

        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            /* We assume a constant bitrate */
            if( p_block_out->i_length )
            {
                p_sys->i_mux_rate =
                    p_block_out->i_buffer * I64C(1000000) / p_block_out->i_length;
            }

            /* set PCR */
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    if( i_query == DEMUX_SET_TIME )
        return VLC_EGENERIC;
    else
        return demux2_vaControlHelper( p_demux->s,
                                       0, -1,
                                       8*p_sys->i_mux_rate, 1, i_query, args );
}

/*****************************************************************************
 * CheckSync: Check if buffer starts with a DTS sync code
 *****************************************************************************/
static int CheckSync( uint8_t *p_peek )
{
    /* 14 bits, little endian version of the bitstream */
    if( p_peek[0] == 0xff && p_peek[1] == 0x1f &&
        p_peek[2] == 0x00 && p_peek[3] == 0xe8 &&
        (p_peek[4] & 0xf0) == 0xf0 && p_peek[5] == 0x07 )
    {
        return VLC_SUCCESS;
    }
    /* 14 bits, big endian version of the bitstream */
    else if( p_peek[0] == 0x1f && p_peek[1] == 0xff &&
             p_peek[2] == 0xe8 && p_peek[3] == 0x00 &&
             p_peek[4] == 0x07 && (p_peek[5] & 0xf0) == 0xf0)
    {
        return VLC_SUCCESS;
    }
    /* 16 bits, big endian version of the bitstream */
    else if( p_peek[0] == 0x7f && p_peek[1] == 0xfe &&
             p_peek[2] == 0x80 && p_peek[3] == 0x01 )
    {
        return VLC_SUCCESS;
    }
    /* 16 bits, little endian version of the bitstream */
    else if( p_peek[0] == 0xfe && p_peek[1] == 0x7f &&
             p_peek[2] == 0x01 && p_peek[3] == 0x80 )
    {
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

