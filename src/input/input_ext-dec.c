/*****************************************************************************
 * input_ext-dec.c: services to the decoders
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_ext-dec.c,v 1.39 2002/10/27 16:58:12 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>                                              /* off_t */

#include <vlc/vlc.h>

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * InitBitstream: initialize a bit_stream_t structure and returns VLC_SUCCESS
 *                on success.
 *****************************************************************************/
int InitBitstream( bit_stream_t * p_bit_stream, decoder_fifo_t * p_fifo,
                void (* pf_bitstream_callback)( bit_stream_t *, vlc_bool_t ),
                void * p_callback_arg )
{
    /* Get the first pes packet */
    input_ExtractPES( p_fifo, &p_bit_stream->p_pes );
    if( !p_bit_stream->p_pes )
	return VLC_EGENERIC;

    p_bit_stream->p_decoder_fifo = p_fifo;
    p_bit_stream->pf_bitstream_callback = pf_bitstream_callback;
    p_bit_stream->p_callback_arg = p_callback_arg;

    p_bit_stream->p_data = p_bit_stream->p_pes->p_first;
    p_bit_stream->p_byte = p_bit_stream->p_data->p_payload_start;
    p_bit_stream->p_end  = p_bit_stream->p_data->p_payload_end;
    p_bit_stream->fifo.buffer = 0;
    p_bit_stream->fifo.i_available = 0;
    p_bit_stream->i_pts = p_bit_stream->p_pes->i_pts;
    p_bit_stream->i_dts = p_bit_stream->p_pes->i_dts;
    p_bit_stream->p_pts_validity = p_bit_stream->p_byte;

    /* Call back the decoder. */
    if( p_bit_stream->pf_bitstream_callback != NULL )
    {
        p_bit_stream->pf_bitstream_callback( p_bit_stream, 1 );
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        /* Get aligned on a word boundary.
         * NB : we _will_ get aligned, because we have at most 
         * sizeof(WORD_TYPE) - 1 bytes to store, and at least
         * sizeof(WORD_TYPE) - 1 empty bytes in the bit buffer. */
        AlignWord( p_bit_stream );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseBitstream: free the bitstream structure.
 *****************************************************************************/
void CloseBitstream( bit_stream_t * p_bit_stream )
{
    if( p_bit_stream->p_pes )
        input_DeletePES( p_bit_stream->p_decoder_fifo->p_packets_mgt,
                         p_bit_stream->p_pes );
}

/*****************************************************************************
 * DecoderError : an error occured, use this function to empty the fifo
 *****************************************************************************/
void DecoderError( decoder_fifo_t * p_fifo )
{
    /* No need to take the lock, because input_ExtractPES already takes it
     * and also check for p_fifo->b_die */

    /* Wait until a `die' order is sent */
    while( !p_fifo->b_die )
    {
        /* Trash all received PES packets */
        input_ExtractPES( p_fifo, NULL );
    }
}

/*****************************************************************************
 * NextDataPacket: go to the data packet after *pp_data, return 1 if we
 * changed PES. This function can fail in case of end of stream, you can
 * check p_bit_stream->p_data or p_bit_stream->p_pes to know wether we did get
 * the next data packet.
 *****************************************************************************/
static inline vlc_bool_t _NextDataPacket( decoder_fifo_t * p_fifo,
                                          bit_stream_t * p_bit_stream )
{
    vlc_bool_t b_new_pes;

    /* We are looking for the next data packet that contains real data,
     * and not just a PES header */
    do
    {
        /* We were reading the last data packet of this PES packet... It's
         * time to jump to the next PES packet */
        if( p_bit_stream->p_data->p_next == NULL )
        {
            /* The next packet could be found in the next PES packet */
	    input_DeletePES( p_fifo->p_packets_mgt, p_bit_stream->p_pes );
            input_ExtractPES( p_fifo, &p_bit_stream->p_pes );
	    if( !p_bit_stream->p_pes )
            {
	        /* Couldn't get the next PES, might be an eos */
	        p_bit_stream->p_data = NULL;
	        return 0;
	    }
            p_bit_stream->p_data = p_bit_stream->p_pes->p_first;
            b_new_pes = 1;
        }
        else
        {
            /* Perhaps the next data packet of the current PES packet contains
             * real data (ie its payload's size is greater than 0). */
            p_bit_stream->p_data = p_bit_stream->p_data->p_next;

            b_new_pes = 0;
        }
    } while ( p_bit_stream->p_data->p_payload_start ==
	      p_bit_stream->p_data->p_payload_end );

    return( b_new_pes );
}

vlc_bool_t NextDataPacket( decoder_fifo_t * p_fifo,
			   bit_stream_t * p_bit_stream )
{
    return( _NextDataPacket( p_fifo, p_bit_stream ) );
}

/*****************************************************************************
 * BitstreamNextDataPacket: go to the next data packet, and update bitstream
 * context. This function can fail in case of eos!
 *****************************************************************************/
static inline void _BitstreamNextDataPacket( bit_stream_t * p_bit_stream )
{
    decoder_fifo_t *    p_fifo = p_bit_stream->p_decoder_fifo;
    vlc_bool_t          b_new_pes;

    b_new_pes = _NextDataPacket( p_fifo, p_bit_stream );
    if( !p_bit_stream->p_pes ) return;

    /* We've found a data packet which contains interesting data... */
    p_bit_stream->p_byte = p_bit_stream->p_data->p_payload_start;
    p_bit_stream->p_end  = p_bit_stream->p_data->p_payload_end;

    /* Call back the decoder. */
    if( p_bit_stream->pf_bitstream_callback != NULL )
    {
        p_bit_stream->pf_bitstream_callback( p_bit_stream, b_new_pes );
    }

    /* Discontinuity management. */
    if( p_bit_stream->p_data->b_discard_payload )
    {
        p_bit_stream->i_pts = p_bit_stream->i_dts = 0;
    }

    /* Retrieve the PTS. */
    if( b_new_pes && p_bit_stream->p_pes->i_pts )
    {
        p_bit_stream->i_pts = p_bit_stream->p_pes->i_pts;
        p_bit_stream->i_dts = p_bit_stream->p_pes->i_dts;
        p_bit_stream->p_pts_validity = p_bit_stream->p_byte;
    }
}

void BitstreamNextDataPacket( bit_stream_t * p_bit_stream )
{
    _BitstreamNextDataPacket( p_bit_stream );
}

/*****************************************************************************
 * UnalignedShowBits : places i_bits bits into the bit buffer, even when
 * not aligned on a word boundary
 *****************************************************************************/
u32 UnalignedShowBits( bit_stream_t * p_bit_stream, unsigned int i_bits )
{
    /* We just fill in the bit buffer. */
    while( p_bit_stream->fifo.i_available < i_bits )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            p_bit_stream->fifo.buffer |= *(p_bit_stream->p_byte++)
                                            << (8 * sizeof(WORD_TYPE) - 8
                                            - p_bit_stream->fifo.i_available);
            p_bit_stream->fifo.i_available += 8;
        }
        else
        {
            _BitstreamNextDataPacket( p_bit_stream );

            if( (ptrdiff_t)p_bit_stream->p_byte & (sizeof(WORD_TYPE) - 1) )
            {
                /* We are not aligned anymore. */
                if( ((ptrdiff_t)p_bit_stream->p_byte
                                    & (sizeof(WORD_TYPE) - 1)) * 8
                        < p_bit_stream->fifo.i_available )
                {
                    /* We are not aligned, and won't be. Copy the first word
                     * of the packet in a temporary buffer, and we'll see
                     * later. */
                    int     i;

                    /* sizeof(WORD_TYPE) - number of bytes to trash
                     * from the last payload */
                    int     j;

                    p_bit_stream->i_showbits_buffer = 0;

                    for( j = i = 0 ; i < sizeof(WORD_TYPE) ; i++ )
                    {
                        if( p_bit_stream->p_byte >= p_bit_stream->p_end )
                        {
                            j = i;
                            _BitstreamNextDataPacket( p_bit_stream );
                        }
                        ((byte_t *)&p_bit_stream->i_showbits_buffer)[i] =
                            * p_bit_stream->p_byte;
                        p_bit_stream->p_byte++;
                    }

                    /* This is kind of kludgy. */
                    p_bit_stream->p_data->p_payload_start +=
                                                         sizeof(WORD_TYPE) - j;
                    p_bit_stream->p_byte =
                        (byte_t *)&p_bit_stream->i_showbits_buffer;
                    p_bit_stream->p_end =
                        (byte_t *)&p_bit_stream->i_showbits_buffer
                            + sizeof(WORD_TYPE);
                    p_bit_stream->showbits_data.p_next = p_bit_stream->p_data;
                    p_bit_stream->p_data = &p_bit_stream->showbits_data;
                }
                else
                {
                    /* We are not aligned, but we can be. */
                    AlignWord( p_bit_stream );
                }
            }

            /* We have 32 bits ready for reading, it will be enough. */
            break;
        }
    }

    /* It shouldn't loop :-)) */
    return( ShowBits( p_bit_stream, i_bits ) );
}

/*****************************************************************************
 * UnalignedGetBits : returns i_bits bits from the bit stream and removes
 * them from the buffer, even when the bit stream is not aligned on a word
 * boundary
 *****************************************************************************/
u32 UnalignedGetBits( bit_stream_t * p_bit_stream, unsigned int i_bits )
{
    u32         i_result;

    i_result = p_bit_stream->fifo.buffer
                    >> (8 * sizeof(WORD_TYPE) - i_bits);
    i_bits = -p_bit_stream->fifo.i_available;

    /* Gather missing bytes. */
    while( i_bits >= 8 )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            i_result |= *(p_bit_stream->p_byte++) << (i_bits - 8);
            i_bits -= 8;
        }
        else
        {
            _BitstreamNextDataPacket( p_bit_stream );
            i_result |= *(p_bit_stream->p_byte++) << (i_bits - 8);
            i_bits -= 8;
        }
    }

    /* Gather missing bits. */
    if( i_bits > 0 )
    {
        unsigned int    i_tmp = 8 - i_bits;

        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            i_result |= *p_bit_stream->p_byte >> i_tmp;
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - i_tmp );
            p_bit_stream->fifo.i_available = i_tmp;
        }
        else
        {
            _BitstreamNextDataPacket( p_bit_stream );
            i_result |= *p_bit_stream->p_byte >> i_tmp;
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - i_tmp );
            p_bit_stream->fifo.i_available = i_tmp;
        }
    }
    else
    {
        p_bit_stream->fifo.i_available = 0;
        p_bit_stream->fifo.buffer = 0;
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        /* Get aligned on a word boundary. Otherwise it is safer
         * to do it the next time.
         * NB : we _will_ get aligned, because we have at most
         * sizeof(WORD_TYPE) - 1 bytes to store, and at least
         * sizeof(WORD_TYPE) - 1 empty bytes in the bit buffer. */
        AlignWord( p_bit_stream );
    }

    return( i_result );
}

/*****************************************************************************
 * UnalignedRemoveBits : removes i_bits (== -i_available) from the bit
 * buffer, even when the bit stream is not aligned on a word boundary
 *****************************************************************************/
void UnalignedRemoveBits( bit_stream_t * p_bit_stream )
{
    /* First remove all unnecessary bytes. */
    while( p_bit_stream->fifo.i_available <= -8 )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            p_bit_stream->p_byte++;
            p_bit_stream->fifo.i_available += 8;
        }
        else
        {
            _BitstreamNextDataPacket( p_bit_stream );
            p_bit_stream->p_byte++;
            p_bit_stream->fifo.i_available += 8;
        }
    }

    /* Remove unnecessary bits. */
    if( p_bit_stream->fifo.i_available < 0 )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - 8
                         - p_bit_stream->fifo.i_available );
            p_bit_stream->fifo.i_available += 8;
        }
        else
        {
            _BitstreamNextDataPacket( p_bit_stream );
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - 8
                         - p_bit_stream->fifo.i_available );
            p_bit_stream->fifo.i_available += 8;
        }
    }
    else
    {
        p_bit_stream->fifo.buffer = 0;
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        /* Get aligned on a word boundary. Otherwise it is safer
         * to do it the next time.
         * NB : we _will_ get aligned, because we have at most 
         * sizeof(WORD_TYPE) - 1 bytes to store, and at least
         * sizeof(WORD_TYPE) - 1 empty bytes in the bit buffer. */
        AlignWord( p_bit_stream );
    }
}

/*****************************************************************************
 * CurrentPTS: returns the current PTS and DTS
 *****************************************************************************/
void CurrentPTS( bit_stream_t * p_bit_stream, mtime_t * pi_pts,
                 mtime_t * pi_dts )
{
    /* Check if the current PTS is already valid (ie. if the first byte
     * of the packet has already been used in the decoder). */
    ptrdiff_t p_diff = p_bit_stream->p_pts_validity - p_bit_stream->p_byte;
    if( p_diff < 0 || p_diff > 4 /* We are far away so it is valid */
         || (p_diff * 8) >= p_bit_stream->fifo.i_available
            /* We have buffered less bytes than actually read */ )
    {
        *pi_pts = p_bit_stream->i_pts;
        if( pi_dts != NULL ) *pi_dts = p_bit_stream->i_dts;
        p_bit_stream->i_pts = 0;
        p_bit_stream->i_dts = 0;
    }
    else
    {
        *pi_pts = 0;
        if( pi_dts != NULL) *pi_dts = 0;
    }
}

/*****************************************************************************
 * NextPTS: returns the PTS and DTS for the next starting byte
 *****************************************************************************/
void NextPTS( bit_stream_t * p_bit_stream, mtime_t * pi_pts,
              mtime_t * pi_dts )
{
    /* Check if the current PTS is already valid (ie. if the first byte
     * of the packet has already been used in the decoder). */
    ptrdiff_t p_diff = p_bit_stream->p_pts_validity - p_bit_stream->p_byte - 1;
    if( p_diff < 0 || p_diff > 4 /* We are far away so it is valid */
         || (p_diff * 8) >= p_bit_stream->fifo.i_available
            /* We have buffered less bytes than actually read */ )
    {
        *pi_pts = p_bit_stream->i_pts;
        if( pi_dts != NULL ) *pi_dts = p_bit_stream->i_dts;
        p_bit_stream->i_pts = 0;
        p_bit_stream->i_dts = 0;
    }
    else
    {
        *pi_pts = 0;
        if( pi_dts != NULL) *pi_dts = 0;
    }
}
