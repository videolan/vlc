/*****************************************************************************
 * rtpfmt.c: RTP payload formats
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include "rtp.h"

int
rtp_packetize_h264_nal( sout_stream_id_t *id,
                        const uint8_t *p_data, int i_data, int64_t i_pts,
                        int64_t i_dts, bool b_last, int64_t i_length );

int rtp_packetize_mpa( sout_stream_id_t *id,
                       block_t *in )
{
    int     i_max   = rtp_mtu (id) - 4; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( 16 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0, in->i_pts );
        /* mbz set to 0 */
        SetWBE( out->p_buffer + 12, 0 );
        /* fragment offset in the current frame */
        SetWBE( out->p_buffer + 14, i * i_max );
        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_buffer   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc2250 */
int rtp_packetize_mpv( sout_stream_id_t *id, block_t *in )
{
    int     i_max   = rtp_mtu (id) - 4; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;
    int     b_sequence_start = 0;
    int     i_temporal_ref = 0;
    int     i_picture_coding_type = 0;
    int     i_fbv = 0, i_bfc = 0, i_ffv = 0, i_ffc = 0;
    int     b_start_slice = 0;

    /* preparse this packet to get some info */
    if( in->i_buffer > 4 )
    {
        uint8_t *p = p_data;
        int      i_rest = in->i_buffer;

        for( ;; )
        {
            while( i_rest > 4 &&
                   ( p[0] != 0x00 || p[1] != 0x00 || p[2] != 0x01 ) )
            {
                p++;
                i_rest--;
            }
            if( i_rest <= 4 )
            {
                break;
            }
            p += 3;
            i_rest -= 4;

            if( *p == 0xb3 )
            {
                /* sequence start code */
                b_sequence_start = 1;
            }
            else if( *p == 0x00 && i_rest >= 4 )
            {
                /* picture */
                i_temporal_ref = ( p[1] << 2) |((p[2]>>6)&0x03);
                i_picture_coding_type = (p[2] >> 3)&0x07;

                if( i_rest >= 4 && ( i_picture_coding_type == 2 ||
                                    i_picture_coding_type == 3 ) )
                {
                    i_ffv = (p[3] >> 2)&0x01;
                    i_ffc = ((p[3]&0x03) << 1)|((p[4]>>7)&0x01);
                    if( i_rest > 4 && i_picture_coding_type == 3 )
                    {
                        i_fbv = (p[4]>>6)&0x01;
                        i_bfc = (p[4]>>3)&0x07;
                    }
                }
            }
            else if( *p <= 0xaf )
            {
                b_start_slice = 1;
            }
        }
    }

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( 16 + i_payload );
        /* MBZ:5 T:1 TR:10 AN:1 N:1 S:1 B:1 E:1 P:3 FBV:1 BFC:3 FFV:1 FFC:3 */
        uint32_t      h = ( i_temporal_ref << 16 )|
                          ( b_sequence_start << 13 )|
                          ( b_start_slice << 12 )|
                          ( i == i_count - 1 ? 1 << 11 : 0 )|
                          ( i_picture_coding_type << 8 )|
                          ( i_fbv << 7 )|( i_bfc << 4 )|( i_ffv << 3 )|i_ffc;

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0,
                              in->i_pts > 0 ? in->i_pts : in->i_dts );

        SetDWBE( out->p_buffer + 12, h );

        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_buffer   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

int rtp_packetize_ac3( sout_stream_id_t *id, block_t *in )
{
    int     i_max   = rtp_mtu (id) - 2; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( 14 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0, in->i_pts );
        /* unit count */
        out->p_buffer[12] = 1;
        /* unit header */
        out->p_buffer[13] = 0x00;
        /* data */
        memcpy( &out->p_buffer[14], p_data, i_payload );

        out->i_buffer   = 14 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

int rtp_packetize_split( sout_stream_id_t *id, block_t *in )
{
    int     i_max   = rtp_mtu (id); /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_buffer   = 12 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc3016 */
int rtp_packetize_mp4a_latm( sout_stream_id_t *id, block_t *in )
{
    int     i_max   = rtp_mtu (id) - 2;              /* payload max in one packet */
    int     latmhdrsize = in->i_buffer / 0xff + 1;
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer, *p_header = NULL;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int     i_payload = __MIN( i_max, i_data );
        block_t *out;

        if( i != 0 )
            latmhdrsize = 0;
        out = block_Alloc( 12 + latmhdrsize + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1) ? 1 : 0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );

        if( i == 0 )
        {
            int tmp = in->i_buffer;

            p_header=out->p_buffer+12;
            while( tmp > 0xfe )
            {
                *p_header = 0xff;
                p_header++;
                tmp -= 0xff;
            }
            *p_header = tmp;
        }

        memcpy( &out->p_buffer[12+latmhdrsize], p_data, i_payload );

        out->i_buffer   = 12 + latmhdrsize + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

int rtp_packetize_mp4a( sout_stream_id_t *id, block_t *in )
{
    int     i_max   = rtp_mtu (id) - 4; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( 16 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        /* AU headers */
        /* AU headers length (bits) */
        out->p_buffer[12] = 0;
        out->p_buffer[13] = 2*8;
        /* for each AU length 13 bits + idx 3bits, */
        SetWBE( out->p_buffer + 14, (in->i_buffer << 3) | 0 );

        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_buffer   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}


/* rfc2429 */
#define RTP_H263_HEADER_SIZE (2)  // plen = 0
#define RTP_H263_PAYLOAD_START (14)  // plen = 0
int rtp_packetize_h263( sout_stream_id_t *id, block_t *in )
{
    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;
    int     i_max   = rtp_mtu (id) - RTP_H263_HEADER_SIZE; /* payload max in one packet */
    int     i_count;
    int     b_p_bit;
    int     b_v_bit = 0; // no pesky error resilience
    int     i_plen = 0; // normally plen=0 for PSC packet
    int     i_pebit = 0; // because plen=0
    uint16_t h;

    if( i_data < 2 )
    {
        return VLC_EGENERIC;
    }
    if( p_data[0] || p_data[1] )
    {
        return VLC_EGENERIC;
    }
    /* remove 2 leading 0 bytes */
    p_data += 2;
    i_data -= 2;
    i_count = ( i_data + i_max - 1 ) / i_max;

    for( i = 0; i < i_count; i++ )
    {
        int      i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( RTP_H263_PAYLOAD_START + i_payload );
        b_p_bit = (i == 0) ? 1 : 0;
        h = ( b_p_bit << 10 )|
            ( b_v_bit << 9  )|
            ( i_plen  << 3  )|
              i_pebit;

        /* rtp common header */
        //b_m_bit = 1; // always contains end of frame
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0,
                              in->i_pts > 0 ? in->i_pts : in->i_dts );

        /* h263 header */
        SetWBE( out->p_buffer + 12, h );
        memcpy( &out->p_buffer[RTP_H263_PAYLOAD_START], p_data, i_payload );

        out->i_buffer = RTP_H263_PAYLOAD_START + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc3984 */
int
rtp_packetize_h264_nal( sout_stream_id_t *id,
                        const uint8_t *p_data, int i_data, int64_t i_pts,
                        int64_t i_dts, bool b_last, int64_t i_length )
{
    const int i_max = rtp_mtu (id); /* payload max in one packet */
    int i_nal_hdr;
    int i_nal_type;

    if( i_data < 5 )
        return VLC_SUCCESS;

    i_nal_hdr = p_data[3];
    i_nal_type = i_nal_hdr&0x1f;

    /* Skip start code */
    p_data += 3;
    i_data -= 3;

    /* */
    if( i_data <= i_max )
    {
        /* Single NAL unit packet */
        block_t *out = block_Alloc( 12 + i_data );
        out->i_dts    = i_dts;
        out->i_length = i_length;

        /* */
        rtp_packetize_common( id, out, b_last, i_pts );
        out->i_buffer = 12 + i_data;

        memcpy( &out->p_buffer[12], p_data, i_data );

        rtp_packetize_send( id, out );
    }
    else
    {
        /* FU-A Fragmentation Unit without interleaving */
        const int i_count = ( i_data-1 + i_max-2 - 1 ) / (i_max-2);
        int i;

        p_data++;
        i_data--;

        for( i = 0; i < i_count; i++ )
        {
            const int i_payload = __MIN( i_data, i_max-2 );
            block_t *out = block_Alloc( 12 + 2 + i_payload );
            out->i_dts    = i_dts + i * i_length / i_count;
            out->i_length = i_length / i_count;

            /* */
            rtp_packetize_common( id, out, (b_last && i_payload == i_data), i_pts );
            out->i_buffer = 14 + i_payload;

            /* FU indicator */
            out->p_buffer[12] = 0x00 | (i_nal_hdr & 0x60) | 28;
            /* FU header */
            out->p_buffer[13] = ( i == 0 ? 0x80 : 0x00 ) | ( (i == i_count-1) ? 0x40 : 0x00 )  | i_nal_type;
            memcpy( &out->p_buffer[14], p_data, i_payload );

            rtp_packetize_send( id, out );

            i_data -= i_payload;
            p_data += i_payload;
        }
    }
    return VLC_SUCCESS;
}

int rtp_packetize_h264( sout_stream_id_t *id, block_t *in )
{
    const uint8_t *p_buffer = in->p_buffer;
    int i_buffer = in->i_buffer;

    while( i_buffer > 4 && ( p_buffer[0] != 0 || p_buffer[1] != 0 || p_buffer[2] != 1 ) )
    {
        i_buffer--;
        p_buffer++;
    }

    /* Split nal units */
    while( i_buffer > 4 )
    {
        int i_offset;
        int i_size = i_buffer;
        int i_skip = i_buffer;

        /* search nal end */
        for( i_offset = 4; i_offset+2 < i_buffer ; i_offset++)
        {
            if( p_buffer[i_offset] == 0 && p_buffer[i_offset+1] == 0 && p_buffer[i_offset+2] == 1 )
            {
                /* we found another startcode */
                i_size = i_offset - ( p_buffer[i_offset-1] == 0 ? 1 : 0);
                i_skip = i_offset;
                break;
            }
        }
        /* TODO add STAP-A to remove a lot of overhead with small slice/sei/... */
        rtp_packetize_h264_nal( id, p_buffer, i_size,
                                (in->i_pts > 0 ? in->i_pts : in->i_dts), in->i_dts,
                                (i_size >= i_buffer), in->i_length * i_size / in->i_buffer );

        i_buffer -= i_skip;
        p_buffer += i_skip;
    }
    return VLC_SUCCESS;
}

int rtp_packetize_amr( sout_stream_id_t *id, block_t *in )
{
    int     i_max   = rtp_mtu (id) - 2; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    /* Only supports octet-aligned mode */
    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_Alloc( 14 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        /* Payload header */
        out->p_buffer[12] = 0xF0; /* CMR */
        out->p_buffer[13] = p_data[0]&0x7C; /* ToC */ /* FIXME: frame type */

        /* FIXME: are we fed multiple frames ? */
        memcpy( &out->p_buffer[14], p_data+1, i_payload-1 );

        out->i_buffer   = 14 + i_payload-1;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

int rtp_packetize_t140( sout_stream_id_t *id, block_t *in )
{
    const size_t   i_max  = rtp_mtu (id);
    const uint8_t *p_data = in->p_buffer;
    size_t         i_data = in->i_buffer;

    for( unsigned i_packet = 0; i_data > 0; i_packet++ )
    {
        size_t i_payload = i_data;

        /* Make sure we stop on an UTF-8 character boundary
         * (assuming the input is valid UTF-8) */
        if( i_data > i_max )
        {
            i_payload = i_max;

            while( ( p_data[i_payload] & 0xC0 ) == 0x80 )
            {
                if( i_payload == 0 )
                    return VLC_SUCCESS; /* fishy input! */

                i_payload--;
            }
        }

        block_t *out = block_Alloc( 12 + i_payload );
        if( out == NULL )
            return VLC_SUCCESS;

        rtp_packetize_common( id, out, 0, in->i_pts + i_packet );
        memcpy( out->p_buffer + 12, p_data, i_payload );

        out->i_buffer = 12 + i_payload;
        out->i_dts    = out->i_pts;
        out->i_length = 0;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}


int rtp_packetize_spx( sout_stream_id_t *id, block_t *in )
{
    uint8_t *p_buffer = in->p_buffer;
    int i_data_size, i_payload_size, i_payload_padding;
    i_data_size = i_payload_size = in->i_buffer;
    i_payload_padding = 0;
    block_t *p_out;

    if ( in->i_buffer > rtp_mtu (id) )
        return VLC_SUCCESS;

    /*
      RFC for Speex in RTP says that each packet must end on an octet 
      boundary. So, we check to see if the number of bytes % 4 is zero.
      If not, we have to add some padding. 

      This MAY be overkill since packetization is handled elsewhere and 
      appears to ensure the octet boundary. However, better safe than
      sorry.
    */
    if ( i_payload_size % 4 )
    {
        i_payload_padding = 4 - ( i_payload_size % 4 );
        i_payload_size += i_payload_padding;
    }

    /*
      Allocate a new RTP p_output block of the appropriate size. 
      Allow for 12 extra bytes of RTP header. 
    */
    p_out = block_Alloc( 12 + i_payload_size );

    if ( i_payload_padding )
    {
    /*
      The padding is required to be a zero followed by all 1s.
    */
        char c_first_pad, c_remaining_pad;
        c_first_pad = 0x7F;
        c_remaining_pad = 0xFF;

        /*
          Allow for 12 bytes before the i_data_size because
          of the expected RTP header added during
          rtp_packetize_common.
        */
        p_out->p_buffer[12 + i_data_size] = c_first_pad; 
        switch (i_payload_padding)
        {
          case 2:
            p_out->p_buffer[12 + i_data_size + 1] = c_remaining_pad; 
            break;
          case 3:
            p_out->p_buffer[12 + i_data_size + 1] = c_remaining_pad; 
            p_out->p_buffer[12 + i_data_size + 2] = c_remaining_pad; 
            break;
        }
    }

    /* Add the RTP header to our p_output buffer. */
    rtp_packetize_common( id, p_out, 0, (in->i_pts > 0 ? in->i_pts : in->i_dts) );
    /* Copy the Speex payload to the p_output buffer */
    memcpy( &p_out->p_buffer[12], p_buffer, i_data_size );

    p_out->i_buffer = 12 + i_payload_size;
    p_out->i_dts = in->i_dts;
    p_out->i_length = in->i_length;

    /* Queue the buffer for actual transmission. */
    rtp_packetize_send( id, p_out );
    return VLC_SUCCESS;
}

static int rtp_packetize_g726( sout_stream_id_t *id, block_t *in, int i_pad )
{
    int     i_max   = (rtp_mtu( id )- 12 + i_pad - 1) & ~i_pad;
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i_packet = 0;

    while( i_data > 0 )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, 0,
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );

        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_buffer   = 12 + i_payload;
        out->i_dts    = in->i_dts + i_packet++ * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }
    return VLC_SUCCESS;
}

int rtp_packetize_g726_16( sout_stream_id_t *id, block_t *in )
{
    return rtp_packetize_g726( id, in, 4 );
}

int rtp_packetize_g726_24( sout_stream_id_t *id, block_t *in )
{
    return rtp_packetize_g726( id, in, 8 );
}

int rtp_packetize_g726_32( sout_stream_id_t *id, block_t *in )
{
    return rtp_packetize_g726( id, in, 2 );
}

int rtp_packetize_g726_40( sout_stream_id_t *id, block_t *in )
{
    return rtp_packetize_g726( id, in, 8 );
}
