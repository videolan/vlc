/*****************************************************************************
 * a52.c : raw A/52 stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
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

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("Raw A/52 demuxer") );
    set_capability( "demux2", 145 );
    set_callbacks( Open, Close );
    add_shortcut( "a52" );
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
    vlc_bool_t b_big_endian;
};

static int CheckSync( uint8_t *p_peek, vlc_bool_t *p_big_endian );

#define PCM_FRAME_SIZE (1536 * 4)
#define A52_PACKET_SIZE (4 * PCM_FRAME_SIZE)
#define A52_MAX_HEADER_SIZE 10


/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    byte_t      *p_peek;
    int         i_peek = 0;
    vlc_bool_t  b_big_endian;

    /* Check if we are dealing with a WAV file */
    if( stream_Peek( p_demux->s, &p_peek, 12 ) == 12 &&
        !strncmp( p_peek, "RIFF", 4 ) && !strncmp( &p_peek[8], "WAVE", 4 ) )
    {
        int i_size;

        /* Skip the wave header */
        i_peek = 12 + 8;
        while( stream_Peek( p_demux->s, &p_peek, i_peek ) == i_peek &&
               strncmp( p_peek + i_peek - 8, "data", 4 ) )
        {
            i_peek += GetDWLE( p_peek + i_peek - 4 ) + 8;
        }

        /* TODO: should check wave format and sample_rate */

        /* Some A52 wav files don't begin with a sync code so we do a more
         * extensive search */
        i_size = stream_Peek( p_demux->s, &p_peek, i_peek + A52_PACKET_SIZE * 2);
        i_size -= (PCM_FRAME_SIZE + A52_MAX_HEADER_SIZE);

        while( i_peek < i_size )
        {
            if( CheckSync( p_peek + i_peek, &b_big_endian ) != VLC_SUCCESS )
                /* The data is stored in 16 bits words */
                i_peek += 2;
            else
            {
                /* Check following sync code */
                if( CheckSync( p_peek + i_peek + PCM_FRAME_SIZE,
                               &b_big_endian ) != VLC_SUCCESS )
                {
                    i_peek += 2;
                    continue;
                }

                break;
            }
        }
    }

    /* Have a peep at the show. */
    if( stream_Peek( p_demux->s, &p_peek, i_peek + A52_MAX_HEADER_SIZE * 2 ) <
        i_peek + A52_MAX_HEADER_SIZE * 2 )
    {
        /* Stream too short */
        msg_Warn( p_demux, "cannot peek()" );
        return VLC_EGENERIC;
    }

    if( CheckSync( p_peek + i_peek, &b_big_endian ) != VLC_SUCCESS )
    {
        if( strncmp( p_demux->psz_demux, "a52", 3 ) )
        {
            return VLC_EGENERIC;
        }

        /* User forced */
        msg_Err( p_demux, "this doesn't look like a A52 audio stream, "
                 "continuing anyway" );
    }

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->b_start = VLC_TRUE;
    p_sys->i_mux_rate = 0;
    p_sys->b_big_endian = b_big_endian;

    /*
     * Load the A52 packetizer
     */
    p_sys->p_packetizer = vlc_object_create( p_demux, VLC_OBJECT_DECODER );
    p_sys->p_packetizer->pf_decode_audio = 0;
    p_sys->p_packetizer->pf_decode_video = 0;
    p_sys->p_packetizer->pf_decode_sub = 0;
    p_sys->p_packetizer->pf_packetize = 0;

    /* Initialization of decoder structure */
    es_format_Init( &p_sys->p_packetizer->fmt_in, AUDIO_ES,
                    VLC_FOURCC( 'a', '5', '2', ' ' ) );

    p_sys->p_packetizer->p_module =
        module_Need( p_sys->p_packetizer, "packetizer", NULL, 0 );
    if( !p_sys->p_packetizer->p_module )
    {
        msg_Err( p_demux, "cannot find A52 packetizer" );
        return VLC_EGENERIC;
    }

    /* Create one program */
    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_in );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

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
    block_t     *p_block_in, *p_block_out;

     /* Align stream */
    int64_t i_pos = stream_Tell( p_demux->s );
    if( i_pos % 2 ) stream_Read( p_demux->s, NULL, 1 );

    if( !( p_block_in = stream_Block( p_demux->s, A52_PACKET_SIZE ) ) )
    {
        return 0;
    }

    if( !p_sys->b_big_endian && p_block_in->i_buffer )
    {
        /* Convert to big endian */

#ifdef HAVE_SWAB
        swab(p_block_in->p_buffer, p_block_in->p_buffer, p_block_in->i_buffer);

#else
        int i;
        byte_t *p_tmp, tmp;
        p_tmp = p_block_in->p_buffer;
        for( i = p_block_in->i_buffer / 2 ; i-- ; )
        {
            tmp = p_tmp[0];
            p_tmp[0] = p_tmp[1];
            p_tmp[1] = tmp;
            p_tmp += 2;
        }
#endif
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
                    p_block_out->i_buffer * I64C(1000000)/p_block_out->i_length;
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
 * CheckSync: Check if buffer starts with an A52 sync code
 *****************************************************************************/
static int CheckSync( uint8_t *p_peek, vlc_bool_t *p_big_endian )
{
    /* Little endian version of the bitstream */
    if( p_peek[0] == 0x77 && p_peek[1] == 0x0b &&
        p_peek[4] < 0x60 /* bsid < 12 */ )
    {
        *p_big_endian = VLC_FALSE;
        return VLC_SUCCESS;
    }
    /* Big endian version of the bitstream */
    else if( p_peek[0] == 0x0b && p_peek[1] == 0x77 &&
             p_peek[5] < 0x60 /* bsid < 12 */ )
    {
        *p_big_endian = VLC_TRUE;
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}


