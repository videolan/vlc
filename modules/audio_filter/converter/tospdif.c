/*****************************************************************************
 * tospdif.c : encapsulates A/52 and DTS frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2002, 2006-2016 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
 *          Rémi Denis-Courmont
 *          Rafaël Carré
 *          Thomas Guillem
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

#include "../../packetizer/a52.h"
#include "../../packetizer/dts_header.h"

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Audio filter for A/52/DTS->S/PDIF encapsulation") )
    set_capability( "audio converter", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

typedef struct
{
    block_t *p_out_buf;
    size_t i_out_offset;

    union
    {
        struct
        {
            unsigned int i_nb_blocks;
        } eac3;
        struct
        {
            unsigned int i_frame_count;
        } truehd;
        struct
        {
            bool b_skip;
        } dtshd;
    };
} filter_sys_t;

#define SPDIF_HEADER_SIZE 8

#define IEC61937_AC3 0x01
#define IEC61937_EAC3 0x15
#define IEC61937_TRUEHD 0x16
#define IEC61937_DTS1 0x0B
#define IEC61937_DTS2 0x0C
#define IEC61937_DTS3 0x0D
#define IEC61937_DTSHD 0x11

#define SPDIF_MORE_DATA 1
#define SPDIF_SUCCESS VLC_SUCCESS
#define SPDIF_ERROR VLC_EGENERIC

static bool is_big_endian( filter_t *p_filter, block_t *p_in_buf )
{
    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_A52:
        case VLC_CODEC_EAC3:
        case VLC_CODEC_MLP:
        case VLC_CODEC_TRUEHD:
            return true;
        case VLC_CODEC_DTS:
        case VLC_CODEC_DTSHD:
            return p_in_buf->p_buffer[0] == 0x1F
                || p_in_buf->p_buffer[0] == 0x7F;
        default:
            vlc_assert_unreachable();
    }
}

static void set_16( filter_t *p_filter, void *p_buf, uint16_t i_val )
{
    if( p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB )
        SetWBE( p_buf, i_val );
    else
        SetWLE( p_buf, i_val );
}

static void write_16( filter_t *p_filter, uint16_t i_val )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    assert( p_sys->p_out_buf != NULL );

    assert( p_sys->p_out_buf->i_buffer - p_sys->i_out_offset
            >= sizeof( uint16_t ) );
    set_16( p_filter, &p_sys->p_out_buf->p_buffer[p_sys->i_out_offset], i_val );
    p_sys->i_out_offset += sizeof( uint16_t );
}

static void write_padding( filter_t *p_filter, size_t i_size )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    assert( p_sys->p_out_buf != NULL );

    assert( p_sys->p_out_buf->i_buffer - p_sys->i_out_offset >= i_size );

    uint8_t *p_out = &p_sys->p_out_buf->p_buffer[p_sys->i_out_offset];
    memset( p_out, 0, i_size );
    p_sys->i_out_offset += i_size;
}

static void write_data( filter_t *p_filter, const void *p_buf, size_t i_size,
                        bool b_input_big_endian )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    assert( p_sys->p_out_buf != NULL );

    bool b_output_big_endian =
        p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB;
    uint8_t *p_out = &p_sys->p_out_buf->p_buffer[p_sys->i_out_offset];
    const uint8_t *p_in = p_buf;

    assert( p_sys->p_out_buf->i_buffer - p_sys->i_out_offset >= i_size );

    if( b_input_big_endian != b_output_big_endian )
        swab( p_in, p_out, i_size & ~1 );
    else
        memcpy( p_out, p_in, i_size & ~1 );
    p_sys->i_out_offset += ( i_size & ~1 );

    if( i_size & 1 )
    {
        assert( p_sys->p_out_buf->i_buffer - p_sys->i_out_offset >= 2 );
        p_out += ( i_size & ~1 );
        set_16( p_filter, p_out, p_in[i_size - 1] << 8 );
        p_sys->i_out_offset += 2;
    }
}

static void write_buffer( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    write_data( p_filter, p_in_buf->p_buffer, p_in_buf->i_buffer,
                is_big_endian( p_filter, p_in_buf ) );
    p_sys->p_out_buf->i_length += p_in_buf->i_length;
}

static int write_init( filter_t *p_filter, block_t *p_in_buf,
                       size_t i_out_size, unsigned i_nb_samples )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    assert( p_sys->p_out_buf == NULL );
    assert( i_out_size > SPDIF_HEADER_SIZE && ( i_out_size & 3 ) == 0 );

    p_sys->p_out_buf = block_Alloc( i_out_size );
    if( !p_sys->p_out_buf )
        return VLC_ENOMEM;
    p_sys->p_out_buf->i_dts = p_in_buf->i_dts;
    p_sys->p_out_buf->i_pts = p_in_buf->i_pts;
    p_sys->p_out_buf->i_nb_samples = i_nb_samples;

    p_sys->i_out_offset = SPDIF_HEADER_SIZE; /* Place for the S/PDIF header */
    return VLC_SUCCESS;
}

static void write_finalize( filter_t *p_filter, uint16_t i_data_type,
                            uint8_t i_length_mul )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    assert( p_sys->p_out_buf != NULL );
    assert( i_data_type != 0 );
    uint8_t *p_out = p_sys->p_out_buf->p_buffer;

    /* S/PDIF header */
    assert( p_sys->i_out_offset > SPDIF_HEADER_SIZE );
    assert( i_length_mul == 1 || i_length_mul == 8 );

    set_16( p_filter, &p_out[0], 0xf872 ); /* syncword 1 */
    set_16( p_filter, &p_out[2], 0x4e1f ); /* syncword 2 */
    set_16( p_filter, &p_out[4], i_data_type ); /* data type */
    /* length in bits or bytes */
    set_16( p_filter, &p_out[6],
              ( p_sys->i_out_offset - SPDIF_HEADER_SIZE ) * i_length_mul );

    /* 0 padding */
    if( p_sys->i_out_offset < p_sys->p_out_buf->i_buffer )
        write_padding( p_filter,
                       p_sys->p_out_buf->i_buffer - p_sys->i_out_offset );
}

static int write_buffer_ac3( filter_t *p_filter, block_t *p_in_buf )
{
    static const size_t a52_size = A52_FRAME_NB * 4;

    if( unlikely( p_in_buf->i_buffer < 6
     || p_in_buf->i_buffer + SPDIF_HEADER_SIZE > a52_size
     || p_in_buf->i_nb_samples != A52_FRAME_NB ) )
    {
        /* Input is not correctly packetizer. Try to parse the buffer in order
         * to get the mandatory informations to play AC3 over S/PDIF */
        vlc_a52_header_t a52;
        if( vlc_a52_header_Parse( &a52, p_in_buf->p_buffer, p_in_buf->i_buffer )
            != VLC_SUCCESS || a52.b_eac3 || a52.i_size > p_in_buf->i_buffer )
            return SPDIF_ERROR;
        p_in_buf->i_buffer = a52.i_size;
        p_in_buf->i_nb_samples = a52.i_samples;
    }

    if( p_in_buf->i_buffer + SPDIF_HEADER_SIZE > a52_size
     || write_init( p_filter, p_in_buf, a52_size, A52_FRAME_NB ) )
        return SPDIF_ERROR;
    write_buffer( p_filter, p_in_buf );
    write_finalize( p_filter, IEC61937_AC3 |
                    ( ( p_in_buf->p_buffer[5] & 0x7 ) << 8 ) /* bsmod */,
                    8 /* in bits */ );

    return SPDIF_SUCCESS;
}


static int write_buffer_eac3( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* The input block can contain the following:
     * a/ One EAC3 independent stream (with 1, 2, 3 or 6 audio blocks per
     * syncframe)
     * b/ One AC3 stream followed by one EAC3 dependent stream (with 6 audio
     * blocks per syncframe)
     * c/ One EAC3 independent stream followed by one EAC3 dependent stream
     * (with 1, 2, 3 or 6 audio blocks per syncframe)
     *
     * One IEC61937_EAC3 frame must contain 6 audio blocks per syncframe. This
     * function will gather input blocks until it reaches this amount of audio
     * blocks.
     *
     * Example: for the c/ case with 1 audio blocks per syncframe, a
     * IEC61937_EAC3 frame will contain 12 a52 streams: 6 independent + 6
     * dependent EAC3 streams.
     */

    vlc_a52_header_t a52;
    if( vlc_a52_header_Parse( &a52, p_in_buf->p_buffer, p_in_buf->i_buffer )
        != VLC_SUCCESS || a52.i_size > p_in_buf->i_buffer )
        return SPDIF_ERROR;

    if( p_in_buf->i_buffer > a52.i_size )
    {
        /* Check if the next stream is an eac3 dependent one */
        vlc_a52_header_t a52_dep;
        const uint8_t *dep_buf = &p_in_buf->p_buffer[a52.i_size];
        const size_t dep_size = p_in_buf->i_buffer - a52.i_size;

        if( vlc_a52_header_Parse( &a52_dep, dep_buf, dep_size ) != VLC_SUCCESS
         || a52_dep.i_size > dep_size
         || !a52_dep.b_eac3 || a52_dep.bs.eac3.strmtyp != EAC3_STRMTYP_DEPENDENT
         || p_in_buf->i_buffer > a52.i_size + a52_dep.i_size )
            return SPDIF_ERROR;
    }

    if( !p_sys->p_out_buf
     && write_init( p_filter, p_in_buf, AOUT_SPDIF_SIZE * 4, AOUT_SPDIF_SIZE ) )
        return SPDIF_ERROR;
    if( p_in_buf->i_buffer > p_sys->p_out_buf->i_buffer - p_sys->i_out_offset )
        return SPDIF_ERROR;

    write_buffer( p_filter, p_in_buf );

    /* cf. Annex E 2.3 of AC3 spec */
    p_sys->eac3.i_nb_blocks += a52.i_blocks_per_sync_frame;

    if( p_sys->eac3.i_nb_blocks < 6 )
        return SPDIF_MORE_DATA;
    else if ( p_sys->eac3.i_nb_blocks > 6 )
        return SPDIF_ERROR;

    write_finalize( p_filter, IEC61937_EAC3, 1 /* in bytes */ );
    p_sys->eac3.i_nb_blocks = 0;
    return SPDIF_SUCCESS;
}

/* Adapted from libavformat/spdifenc.c:
 * It seems Dolby TrueHD frames have to be encapsulated in MAT frames before
 * they can be encapsulated in IEC 61937.
 * Here we encapsulate 24 TrueHD frames in a single MAT frame, padding them
 * to achieve constant rate.
 * The actual format of a MAT frame is unknown, but the below seems to work.
 * However, it seems it is not actually necessary for the 24 TrueHD frames to
 * be in an exact alignment with the MAT frame
 */
static int write_buffer_truehd( filter_t *p_filter, block_t *p_in_buf )
{
#define TRUEHD_FRAME_OFFSET     2560

    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_sys->p_out_buf
     && write_init( p_filter, p_in_buf, 61440, 61440 / 16 ) )
        return SPDIF_ERROR;

    int i_padding = 0;
    if( p_sys->truehd.i_frame_count == 0 )
    {
        static const char p_mat_start_code[20] = {
            0x07, 0x9E, 0x00, 0x03, 0x84, 0x01, 0x01, 0x01, 0x80, 0x00,
            0x56, 0xA5, 0x3B, 0xF4, 0x81, 0x83, 0x49, 0x80, 0x77, 0xE0
        };
        write_data( p_filter, p_mat_start_code, 20, true );
        /* We need to include the S/PDIF header in the first MAT frame */
        i_padding = TRUEHD_FRAME_OFFSET - p_in_buf->i_buffer - 20
                  - SPDIF_HEADER_SIZE;
    }
    else if( p_sys->truehd.i_frame_count == 11 )
    {
        /* The middle mat code need to be at the ((2560 * 12) - 4) offset */
        i_padding = TRUEHD_FRAME_OFFSET - p_in_buf->i_buffer - 4;
    }
    else if( p_sys->truehd.i_frame_count == 12 )
    {
        static const char p_mat_middle_code[12] = {
            0xC3, 0xC1, 0x42, 0x49, 0x3B, 0xFA,
            0x82, 0x83, 0x49, 0x80, 0x77, 0xE0
        };
        write_data( p_filter, p_mat_middle_code, 12, true );
        i_padding = TRUEHD_FRAME_OFFSET - p_in_buf->i_buffer - ( 12 - 4 );
    }
    else if( p_sys->truehd.i_frame_count == 23 )
    {
        static const char p_mat_end_code[16] = {
            0xC3, 0xC2, 0xC0, 0xC4, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x11
        };

        /* The end mat code need to be at the ((2560 * 24) - 24) offset */
        i_padding = TRUEHD_FRAME_OFFSET - p_in_buf->i_buffer - 24;

        if( i_padding < 0 || p_in_buf->i_buffer + i_padding >
            p_sys->p_out_buf->i_buffer - p_sys->i_out_offset )
            return SPDIF_ERROR;

        write_buffer( p_filter, p_in_buf );
        write_padding( p_filter, i_padding );
        write_data( p_filter, p_mat_end_code, 16, true );
        write_finalize( p_filter, IEC61937_TRUEHD, 1 /* in bytes */ );
        p_sys->truehd.i_frame_count = 0;
        return SPDIF_SUCCESS;
    }
    else
        i_padding = TRUEHD_FRAME_OFFSET - p_in_buf->i_buffer;

    if( i_padding < 0 || p_in_buf->i_buffer + i_padding >
        p_sys->p_out_buf->i_buffer - p_sys->i_out_offset )
        return SPDIF_ERROR;

    write_buffer( p_filter, p_in_buf );
    write_padding( p_filter, i_padding );
    p_sys->truehd.i_frame_count++;
    return SPDIF_MORE_DATA;
}

static int write_buffer_dts( filter_t *p_filter, block_t *p_in_buf )
{
    uint16_t i_data_type;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Only send the DTS core part */
    vlc_dts_header_t core;
    if( vlc_dts_header_Parse( &core, p_in_buf->p_buffer,
                              p_in_buf->i_buffer ) != VLC_SUCCESS )
        return SPDIF_ERROR;
    p_in_buf->i_nb_samples = core.i_frame_length;
    p_in_buf->i_buffer = core.i_frame_size;

    switch( p_in_buf->i_nb_samples )
    {
    case  512:
        i_data_type = IEC61937_DTS1;
        break;
    case 1024:
        i_data_type = IEC61937_DTS2;
        break;
    case 2048:
        i_data_type = IEC61937_DTS3;
        break;
    default:
        msg_Err( p_filter, "Frame size %d not supported",
                 p_in_buf->i_nb_samples );
        return SPDIF_ERROR;
    }

    if( core.syncword == DTS_SYNC_CORE_14BITS_BE ||
        core.syncword == DTS_SYNC_CORE_14BITS_LE )
    {
        if( p_in_buf->i_buffer > p_in_buf->i_nb_samples * 4 )
            return SPDIF_ERROR;
        if( write_init( p_filter, p_in_buf, p_in_buf->i_nb_samples * 4,
                        p_in_buf->i_nb_samples ) )
            return SPDIF_ERROR;

        uint8_t *p_out = &p_sys->p_out_buf->p_buffer[p_sys->i_out_offset];
        ssize_t i_size = vlc_dts_header_Convert14b16b( p_out,
                            p_sys->p_out_buf->i_buffer - p_sys->i_out_offset,
                            p_in_buf->p_buffer, p_in_buf->i_buffer,
                            p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFL );
        if( i_size < 0 )
            return SPDIF_ERROR;

        p_sys->i_out_offset += i_size;
        p_sys->p_out_buf->i_length += p_in_buf->i_length;
    }
    else
    {
        if( p_in_buf->i_buffer + SPDIF_HEADER_SIZE > p_in_buf->i_nb_samples * 4 )
            return SPDIF_ERROR;

        if( write_init( p_filter, p_in_buf, p_in_buf->i_nb_samples * 4,
                        p_in_buf->i_nb_samples ) )
            return SPDIF_ERROR;
        write_buffer( p_filter, p_in_buf );
    }

    write_finalize( p_filter, i_data_type, 8 /* in bits */ );
    return SPDIF_SUCCESS;
}

/* Adapted from libavformat/spdifenc.c:
 * DTS type IV (DTS-HD) can be transmitted with various frame repetition
 * periods; longer repetition periods allow for longer packets and therefore
 * higher bitrate. Longer repetition periods mean that the constant bitrate of
 * the output IEC 61937 stream is higher.
 * The repetition period is measured in IEC 60958 frames (4 bytes).
 */
static int dtshd_get_subtype( unsigned i_frame_length )
{
    switch( i_frame_length )
    {
        case 512:   return 0x0;
        case 1024:  return 0x1;
        case 2048:  return 0x2;
        case 4096:  return 0x3;
        case 8192:  return 0x4;
        case 16384: return 0x5;
        default:    return -1;
    }
}

/* Adapted from libavformat/spdifenc.c: */
static int write_buffer_dtshd( filter_t *p_filter, block_t *p_in_buf )
{
    static const char p_dtshd_start_code[10] = {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe
    };
    static const size_t i_dtshd_start_code = sizeof( p_dtshd_start_code );

    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_dts_header_t core;
    if( vlc_dts_header_Parse( &core, p_in_buf->p_buffer,
                              p_in_buf->i_buffer ) != VLC_SUCCESS )
        return SPDIF_ERROR;
    unsigned i_period = p_filter->fmt_out.audio.i_rate
                      * core.i_frame_length / core.i_rate;

    int i_subtype = dtshd_get_subtype( i_period );
    if( i_subtype == -1 )
        return SPDIF_ERROR;

    size_t i_in_size = i_dtshd_start_code + 2 + p_in_buf->i_buffer;
    size_t i_out_size = i_period * 4;
    uint16_t i_data_type = IEC61937_DTSHD | i_subtype << 8;

    if( p_sys->dtshd.b_skip
     || i_in_size + SPDIF_HEADER_SIZE > i_out_size )
    {
        /* The bitrate is too high, pass only the core part */
        p_in_buf->i_buffer = core.i_frame_size;
        i_in_size = i_dtshd_start_code + 2 + p_in_buf->i_buffer;
        if( i_in_size + SPDIF_HEADER_SIZE > i_out_size )
            return SPDIF_ERROR;

        /* Don't try to send substreams anymore. That way, we avoid to switch
         * back and forth between DTD and DTS-HD */
        p_sys->dtshd.b_skip = true;
    }

    if( write_init( p_filter, p_in_buf, i_out_size,
                    i_out_size / p_filter->fmt_out.audio.i_bytes_per_frame ) )
        return SPDIF_ERROR;

    write_data( p_filter, p_dtshd_start_code, i_dtshd_start_code, true );
    write_16( p_filter, p_in_buf->i_buffer );
    write_buffer( p_filter, p_in_buf );

    /* Align so that (length_code & 0xf) == 0x8. This is reportedly needed
     * with some receivers, but the exact requirement is unconfirmed. */
    size_t i_align = vlc_align( i_in_size + 0x8, 0x10 ) - 0x8;

    if( i_align > i_in_size && i_align - i_in_size
        <= p_sys->p_out_buf->i_buffer - p_sys->i_out_offset )
        write_padding( p_filter, i_align - i_in_size );

    write_finalize( p_filter, i_data_type, 1 /* in bytes */ );
    return SPDIF_SUCCESS;
}

static void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_out_buf != NULL )
    {
        block_Release( p_sys->p_out_buf );
        p_sys->p_out_buf = NULL;
    }
    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_TRUEHD:
            p_sys->truehd.i_frame_count = 0;
            break;
        case VLC_CODEC_EAC3:
            p_sys->eac3.i_nb_blocks = 0;
            break;
        default:
            break;
    }
}

static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    block_t *p_out_buf = NULL;

    int i_ret;
    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_A52:
            i_ret = write_buffer_ac3( p_filter, p_in_buf );
            break;
        case VLC_CODEC_EAC3:
            i_ret = write_buffer_eac3( p_filter, p_in_buf );
            break;
        case VLC_CODEC_MLP:
        case VLC_CODEC_TRUEHD:
            i_ret = write_buffer_truehd( p_filter, p_in_buf );
            break;
        case VLC_CODEC_DTSHD:
            i_ret = write_buffer_dtshd( p_filter, p_in_buf );
            break;
        case VLC_CODEC_DTS:
            i_ret = write_buffer_dts( p_filter, p_in_buf );
            break;
        default:
            vlc_assert_unreachable();
    }

    switch( i_ret )
    {
        case SPDIF_SUCCESS:
            assert( p_sys->p_out_buf->i_buffer == p_sys->i_out_offset );
            p_out_buf = p_sys->p_out_buf;
            p_sys->p_out_buf = NULL;
            break;
        case SPDIF_MORE_DATA:
            break;
        case SPDIF_ERROR:
            Flush( p_filter );
            break;
    }

    block_Release( p_in_buf );
    return p_out_buf;
}

static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    if( ( p_filter->fmt_in.audio.i_format != VLC_CODEC_DTS &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_DTSHD &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_A52 &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_EAC3 &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_MLP &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_TRUEHD ) ||
        ( p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFL &&
          p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFB ) )
        return VLC_EGENERIC;

    p_sys = p_filter->p_sys = calloc( 1, sizeof(filter_sys_t) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_filter->pf_audio_filter = DoWork;
    p_filter->pf_flush = Flush;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    Flush( p_filter );
    free( p_filter->p_sys );
}
