/*****************************************************************************
 * mpeg4video.c: mpeg 4 video packetizer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/sout.h>
#include <vlc/input.h>                  /* hmmm, just for INPUT_RATE_DEFAULT */

#include "vlc_bits.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    set_description( _("MPEG4 video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, Close );
vlc_module_end();


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static block_t *Packetize( decoder_t *, block_t ** );

struct decoder_sys_t
{
    /*
     * Common properties
     */
    mtime_t i_interpolated_pts;
    mtime_t i_interpolated_dts;
    mtime_t i_last_ref_pts;
    mtime_t i_last_time_ref;
    mtime_t i_time_ref;
    mtime_t i_last_time;
    mtime_t i_last_timeincr;

    vlc_bool_t  b_vop;
    int         i_buffer;
    int         i_buffer_size;
    uint8_t     *p_buffer;
    unsigned int i_flags;

    int         i_fps_num;
    int         i_fps_den;
    int         i_last_incr;
    int         i_last_incr_diff;

    vlc_bool_t  b_frame;
};

static int m4v_FindStartCode( uint8_t **, uint8_t * );
static int m4v_VOLParse( decoder_t *, es_format_t *, uint8_t *, int );
static int vlc_log2( unsigned int );

#define VIDEO_OBJECT_MASK                       0x01f
#define VIDEO_OBJECT_LAYER_MASK                 0x00f

#define VIDEO_OBJECT_START_CODE                 0x100
#define VIDEO_OBJECT_LAYER_START_CODE           0x120
#define VISUAL_OBJECT_SEQUENCE_START_CODE       0x1b0
#define VISUAL_OBJECT_SEQUENCE_END_CODE         0x1b1
#define USER_DATA_START_CODE                    0x1b2
#define GROUP_OF_VOP_START_CODE                 0x1b3
#define VIDEO_SESSION_ERROR_CODE                0x1b4
#define VISUAL_OBJECT_START_CODE                0x1b5
#define VOP_START_CODE                          0x1b6
#define FACE_OBJECT_START_CODE                  0x1ba
#define FACE_OBJECT_PLANE_START_CODE            0x1bb
#define MESH_OBJECT_START_CODE                  0x1bc
#define MESH_OBJECT_PLANE_START_CODE            0x1bd
#define STILL_TEXTURE_OBJECT_START_CODE         0x1be
#define TEXTURE_SPATIAL_LAYER_START_CODE        0x1bf
#define TEXTURE_SNR_LAYER_START_CODE            0x1c0

/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_FOURCC( 'm', '4', 's', '2'):
        case VLC_FOURCC( 'M', '4', 'S', '2'):
        case VLC_FOURCC( 'm', 'p', '4', 's'):
        case VLC_FOURCC( 'M', 'P', '4', 'S'):
        case VLC_FOURCC( 'm', 'p', '4', 'v'):
        case VLC_FOURCC( 'M', 'P', '4', 'V'):
        case VLC_FOURCC( 'D', 'I', 'V', 'X'):
        case VLC_FOURCC( 'd', 'i', 'v', 'x'):
        case VLC_FOURCC( 'X', 'V', 'I', 'D'):
        case VLC_FOURCC( 'X', 'v', 'i', 'D'):
        case VLC_FOURCC( 'x', 'v', 'i', 'd'):
        case VLC_FOURCC( 'D', 'X', '5', '0'):
        case VLC_FOURCC( 'd', 'x', '5', '0'):
        case VLC_FOURCC( 0x04, 0,   0,   0):
        case VLC_FOURCC( '3', 'I', 'V', '2'):
        case VLC_FOURCC( 'm', '4', 'c', 'c'):
        case VLC_FOURCC( 'M', '4', 'C', 'C'):
            break;

        default:
            return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(decoder_sys_t) ) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    memset( p_sys, 0, sizeof(decoder_sys_t) );

    /* Setup properties */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );

    if( p_dec->fmt_in.i_extra )
    {
        /* We have a vol */
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = malloc( p_dec->fmt_in.i_extra );
        memcpy( p_dec->fmt_out.p_extra, p_dec->fmt_in.p_extra,
                p_dec->fmt_in.i_extra );

        msg_Dbg( p_dec, "opening with vol size: %d", p_dec->fmt_in.i_extra );
        m4v_VOLParse( p_dec, &p_dec->fmt_out,
                      p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
    }
    else
    {
        /* No vol, we'll have to look for one later on */
        p_dec->fmt_out.i_extra = 0;
        p_dec->fmt_out.p_extra = 0;
    }

    /* Set callback */
    p_dec->pf_packetize = Packetize;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the packetizer
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_sys->p_buffer ) free( p_dec->p_sys->p_buffer );
    free( p_dec->p_sys );
}

/****************************************************************************
 * Packetize: the whole thing
 ****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t *p_chain_out = NULL;
    block_t *p_block;
    uint8_t *p_vol = NULL;
    uint8_t *p_start;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    /* Append data */
    if( p_sys->i_buffer + p_block->i_buffer > p_sys->i_buffer_size )
    {
        p_sys->i_buffer_size += p_block->i_buffer + 1024;
        p_sys->p_buffer = realloc( p_sys->p_buffer, p_sys->i_buffer_size );
    }
    memcpy( &p_sys->p_buffer[p_sys->i_buffer], p_block->p_buffer,
            p_block->i_buffer );
    p_sys->i_buffer += p_block->i_buffer;

    if( p_sys->i_buffer > 10*1000000 )
    {
        msg_Warn( p_dec, "reseting context" );
        p_sys->i_buffer = 0;
    }

    /* Search vop */
    p_start = &p_sys->p_buffer[p_sys->i_buffer - p_block->i_buffer - 4];
    if( p_start < p_sys->p_buffer )
    {
        p_start = p_sys->p_buffer;
    }
    for( ;; )
    {
        if( m4v_FindStartCode( &p_start, &p_sys->p_buffer[p_sys->i_buffer] ) )
        {
            block_Release( p_block );
            *pp_block = NULL;
            return p_chain_out;
        }
        /* fprintf( stderr, "start code=0x1%2.2x\n", p_start[3] ); */

        if( p_vol )
        {
            if( !p_dec->fmt_out.i_extra )
            {
                /* Copy the complete VOL */
                p_dec->fmt_out.i_extra = p_start - p_vol;
                p_dec->fmt_out.p_extra =
                    realloc( p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
                memcpy( p_dec->fmt_out.p_extra, p_vol,
                        p_dec->fmt_out.i_extra );
                m4v_VOLParse( p_dec, &p_dec->fmt_out,
                              p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
            }

            /* Remove VOL from the original stream */
            memmove( p_vol, p_start,
                     p_sys->i_buffer - (p_start - p_sys->p_buffer) );
            p_sys->i_buffer -= p_dec->fmt_out.i_extra;
            p_start = p_vol;

            p_vol = NULL;
        }
        if( p_sys->b_vop )
        {
            /* Output the complete VOP we have */
            int     i_out = p_start - p_sys->p_buffer;
            block_t *p_out = block_New( p_dec, i_out );

            /* extract data */
            memcpy( p_out->p_buffer, p_sys->p_buffer, i_out );
            if( i_out < p_sys->i_buffer )
            {
                memmove( p_sys->p_buffer, &p_sys->p_buffer[i_out],
                         p_sys->i_buffer - i_out );
            }
            p_sys->i_buffer -= i_out;
            p_start -= i_out;

            p_out->i_flags = p_sys->i_flags;
            p_out->i_pts = p_sys->i_interpolated_pts;
            p_out->i_dts = p_sys->i_interpolated_dts;

            /* FIXME doesn't work when there is multiple VOP in one block */
            if( p_block->i_dts > p_sys->i_interpolated_dts )
            {
                p_out->i_length = p_block->i_dts - p_sys->i_interpolated_dts;
            }

            if( p_dec->fmt_out.i_extra > 0 )
            {
                block_ChainAppend( &p_chain_out, p_out );
            }
            else
            {
                msg_Warn( p_dec, "waiting for VOL" );
                block_Release( p_out );
            }

            p_sys->b_vop = VLC_FALSE;
        }

        if( p_start[3] >= 0x20 && p_start[3] <= 0x2f )
        {
            /* Start of the VOL */
            p_vol = p_start;
        }
        else if( p_start[3] == 0xb3 )
        {
            /* GOP header */
        }
        else if( p_start[3] == 0xb6 )
        {
            /* Parse the VOP */
            bs_t s;
            int i_modulo_time_base = 0;
            int i_time_increment_bits;
            int64_t i_time_increment, i_time_ref;

            /* FIXME: we don't actually check we received enough data to read
             * the VOP time increment. */
            bs_init( &s, &p_start[4],
                     p_sys->i_buffer - (p_start - p_sys->p_buffer) - 4 );

            switch( bs_read( &s, 2 ) )
            {
                case 0:
                    p_sys->i_flags = BLOCK_FLAG_TYPE_I;
                    break;
                case 1:
                    p_sys->i_flags = BLOCK_FLAG_TYPE_P;
                    break;
                case 2:
                    p_sys->i_flags = BLOCK_FLAG_TYPE_B;
                    p_sys->b_frame = VLC_TRUE;
                    break;
                case 3: /* gni ? */
                    p_sys->i_flags = BLOCK_FLAG_TYPE_PB;
                    break;
            }

            while( bs_read( &s, 1 ) ) i_modulo_time_base++;
            if( !bs_read1( &s ) ) continue; /* Marker */

            /* VOP time increment */
            i_time_increment_bits = vlc_log2(p_dec->p_sys->i_fps_num - 1) + 1;
            if( i_time_increment_bits < 1 ) i_time_increment_bits = 1;
            i_time_increment = bs_read( &s, i_time_increment_bits );

            /* Interpolate PTS/DTS */
            if( !(p_sys->i_flags & BLOCK_FLAG_TYPE_B) )
            {
                p_sys->i_last_time_ref = p_sys->i_time_ref;
                p_sys->i_time_ref +=
                    (i_modulo_time_base * p_dec->p_sys->i_fps_num);
                i_time_ref = p_sys->i_time_ref;
            }
            else
            {
                i_time_ref = p_sys->i_last_time_ref +
                    (i_modulo_time_base * p_dec->p_sys->i_fps_num);
            }

            if( p_dec->p_sys->i_fps_num < 5 && /* Work-around buggy streams */
                p_dec->fmt_in.video.i_frame_rate > 0 &&
                p_dec->fmt_in.video.i_frame_rate_base > 0 )
            {
                p_sys->i_interpolated_pts += I64C(1000000) *
                    p_dec->fmt_in.video.i_frame_rate_base *
                    p_block->i_rate / INPUT_RATE_DEFAULT /
                    p_dec->fmt_in.video.i_frame_rate;
            }
            else if( p_dec->p_sys->i_fps_num )
                p_sys->i_interpolated_pts +=
                    ( I64C(1000000) * (i_time_ref + i_time_increment -
                       p_sys->i_last_time - p_sys->i_last_timeincr) *
                      p_block->i_rate / INPUT_RATE_DEFAULT /
                      p_dec->p_sys->i_fps_num );

            p_sys->i_last_time = i_time_ref;
            p_sys->i_last_timeincr = i_time_increment;

            /* Correct interpolated dts when we receive a new pts/dts */
            if( p_block->i_pts > 0 )
                p_sys->i_interpolated_pts = p_block->i_pts;
            if( p_block->i_dts > 0 )
                p_sys->i_interpolated_dts = p_block->i_dts;

            if( (p_sys->i_flags & BLOCK_FLAG_TYPE_B) || !p_sys->b_frame )
            {
                /* Trivial case (DTS == PTS) */

                p_sys->i_interpolated_dts = p_sys->i_interpolated_pts;

                if( p_block->i_pts > 0 )
                    p_sys->i_interpolated_dts = p_block->i_pts;
                if( p_block->i_dts > 0 )
                    p_sys->i_interpolated_dts = p_block->i_dts;

                p_sys->i_interpolated_pts = p_sys->i_interpolated_dts;
            }
            else
            {
                if( p_sys->i_last_ref_pts > 0 )
                    p_sys->i_interpolated_dts = p_sys->i_last_ref_pts;

                p_sys->i_last_ref_pts = p_sys->i_interpolated_pts;
            }

            p_sys->b_vop = VLC_TRUE;

            /* Don't re-use the same PTS/DTS twice */
            p_block->i_pts = p_block->i_dts = 0;
        }

        p_start += 4; /* Next */
    }
}

/****************************************************************************
 * m4v_FindStartCode
 ****************************************************************************/
static int m4v_FindStartCode( uint8_t **pp_start, uint8_t *p_end )
{
    uint8_t *p = *pp_start;

    /* We wait for 4+1 bytes */
    for( p = *pp_start; p < p_end - 5; p++ )
    {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
        {
            *pp_start = p;
            return VLC_SUCCESS;
        }
    }

    *pp_start = p_end;
    return VLC_EGENERIC;
}


/* look at ffmpeg av_log2 ;) */
static int vlc_log2( unsigned int v )
{
    int n = 0;
    static const int vlc_log2_table[16] =
    {
        0,0,1,1,2,2,2,2, 3,3,3,3,3,3,3,3
    };

    if( v&0xffff0000 )
    {
        v >>= 16;
        n += 16;
    }
    if( v&0xff00 )
    {
        v >>= 8;
        n += 8;
    }
    if( v&0xf0 )
    {
        v >>= 4;
        n += 4;
    }
    n += vlc_log2_table[v];

    return n;
}

/* m4v_VOLParse:
 *  TODO:
 *      - support aspect ratio
 */
static int m4v_VOLParse( decoder_t *p_dec, es_format_t *fmt,
                         uint8_t *p_vol, int i_vol )
{
    bs_t s;
    int i_vo_type;
    int i_vo_ver_id;
    int i_ar;
    int i_shape;

    for( ;; )
    {
        if( p_vol[0] == 0x00 && p_vol[1] == 0x00 &&
            p_vol[2] == 0x01 &&
            p_vol[3] >= 0x20 && p_vol[3] <= 0x2f )
        {
            break;
        }
        p_vol++;
        i_vol--;
        if( i_vol <= 4 )
        {
            return VLC_EGENERIC;
        }
    }

    /* parse the vol */
    bs_init( &s, &p_vol[4], i_vol - 4 );

    bs_skip( &s, 1 );   /* random access */
    i_vo_type = bs_read( &s, 8 );
    if( bs_read1( &s ) )
    {
        i_vo_ver_id = bs_read( &s, 4 );
        bs_skip( &s, 3 );
    }
    else
    {
        i_vo_ver_id = 1;
    }
    i_ar = bs_read( &s, 4 );
    if( i_ar == 0xf )
    {
        int i_ar_width, i_ar_height;

        i_ar_width = bs_read( &s, 8 );
        i_ar_height= bs_read( &s, 8 );
    }
    if( bs_read1( &s ) )
    {
        int i_chroma_format;
        int i_low_delay;

        /* vol control parameter */
        i_chroma_format = bs_read( &s, 2 );
        i_low_delay = bs_read1( &s );

        if( bs_read1( &s ) )
        {
            bs_skip( &s, 16 );
            bs_skip( &s, 16 );
            bs_skip( &s, 16 );
            bs_skip( &s, 3 );
            bs_skip( &s, 11 );
            bs_skip( &s, 1 );
            bs_skip( &s, 16 );
        }
    }
    /* shape 0->RECT, 1->BIN, 2->BIN_ONLY, 3->GRAY */
    i_shape = bs_read( &s, 2 );
    if( i_shape == 3 && i_vo_ver_id != 1 )
    {
        bs_skip( &s, 4 );
    }

    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    p_dec->p_sys->i_fps_num = bs_read( &s, 16 ); /* Time increment resolution*/
    if( !p_dec->p_sys->i_fps_num ) p_dec->p_sys->i_fps_num = 1;

    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    if( bs_read1( &s ) )
    {
        int i_time_increment_bits =
            vlc_log2( p_dec->p_sys->i_fps_num - 1 ) + 1;

        if( i_time_increment_bits < 1 ) i_time_increment_bits = 1;

        p_dec->p_sys->i_fps_den = bs_read( &s, i_time_increment_bits );
    }
    if( i_shape == 0 )
    {
        bs_skip( &s, 1 );
        fmt->video.i_width = bs_read( &s, 13 );
        bs_skip( &s, 1 );
        fmt->video.i_height= bs_read( &s, 13 );
        bs_skip( &s, 1 );
    }
    return VLC_SUCCESS;
}
