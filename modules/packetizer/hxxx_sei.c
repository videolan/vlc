/*****************************************************************************
 * hxxx_sei.c: AVC/HEVC packetizers SEI handling
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_bits.h>
#include <vlc_block.h>

#include "hxxx_sei.h"
#include "hxxx_nal.h"

void HxxxParse_AnnexB_SEI(decoder_t *p_dec, const uint8_t *p_buf, size_t i_buf,
                          uint8_t i_header, pf_hxxx_sei_callback cb)
{
    if( hxxx_strip_AnnexB_startcode( &p_buf, &i_buf ) )
        HxxxParseSEI(p_dec, p_buf, i_buf, i_header, cb);
}

void HxxxParseSEI(decoder_t *p_dec, const uint8_t *p_buf, size_t i_buf,
                  uint8_t i_header, pf_hxxx_sei_callback pf_callback)
{
    bs_t s;
    unsigned i_bitflow = 0;

    if( i_buf < 2 )
        return;

    bs_init( &s, p_buf, i_buf );
    s.p_fwpriv = &i_bitflow;
    s.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */

    bs_skip( &s, i_header * 8 ); /* nal unit header */

    while( bs_remain( &s ) >= 8 && bs_aligned( &s ) )
    {
        /* Read type */
        unsigned i_type = 0;
        while( bs_remain( &s ) >= 8 )
        {
            const uint8_t i_byte = bs_read( &s, 8 );
            i_type += i_byte;
            if( i_byte != 0xff )
                break;
        }

        /* Read size */
        unsigned i_size = 0;
        while( bs_remain( &s ) >= 8 )
        {
            const uint8_t i_byte = bs_read( &s, 8 );
            i_size += i_byte;
            if( i_byte != 0xff )
                break;
        }

        /* Check room */
        if( bs_remain( &s ) < 8 )
            break;

        hxxx_sei_data_t sei_data;
        sei_data.i_type = i_type;

        /* Save start offset */
        const unsigned i_start_bit_pos = bs_pos( &s );
        switch( i_type )
        {
            /* Look for pic timing, do not decode locally */
            case HXXX_SEI_PIC_TIMING:
            {
                sei_data.p_bs = &s;
                pf_callback( p_dec, &sei_data );
            } break;

            /* Look for user_data_registered_itu_t_t35 */
            case HXXX_SEI_USER_DATA_REGISTERED_ITU_T_T35:
            {
                /* TS 101 154 Auxiliary Data and H264/AVC video */
                static const uint8_t p_DVB1_data_start_code[] = {
                    0xb5, /* United States */
                    0x00, 0x31, /* US provider code */
                    0x47, 0x41, 0x39, 0x34 /* user identifier */
                };

                static const uint8_t p_DIRECTV_data_start_code[] = {
                    0xb5, /* United States */
                    0x00, 0x2f, /* US provider code */
                    0x03  /* Captions */
                };

                unsigned i_t35;
                uint8_t *p_t35 = malloc( i_size );
                if( !p_t35 )
                    break;

                for( i_t35 = 0; i_t35<i_size && bs_remain( &s ) >= 8; i_t35++ )
                    p_t35[i_t35] = bs_read( &s, 8 );

                /* Check for we have DVB1_data() */
                if( ( i_t35 >= sizeof(p_DVB1_data_start_code) &&
                         !memcmp( p_t35, p_DVB1_data_start_code, sizeof(p_DVB1_data_start_code) ) ) ||
                    ( i_t35 >= sizeof(p_DIRECTV_data_start_code) &&
                         !memcmp( p_t35, p_DIRECTV_data_start_code, sizeof(p_DIRECTV_data_start_code) ) ) )
                {
                    sei_data.itu_t35.i_cc = i_t35 - 3;
                    sei_data.itu_t35.p_cc = &p_t35[3];
                    pf_callback( p_dec, &sei_data );
                }

                free( p_t35 );
            } break;

            /* Look for SEI recovery point */
            case HXXX_SEI_RECOVERY_POINT:
            {
                sei_data.recovery.i_frames = bs_read_ue( &s );
                //bool b_exact_match = bs_read( &s, 1 );
                //bool b_broken_link = bs_read( &s, 1 );
                //int i_changing_slice_group = bs_read( &s, 2 );
                pf_callback( p_dec, &sei_data );
            } break;

            default:
                /* Will skip */
                break;
        }
        const unsigned i_end_bit_pos = bs_pos( &s );

        /* Skip unsparsed content */
        if( i_end_bit_pos - i_start_bit_pos > i_size * 8 ) /* Something went wrong with _ue reads */
            break;
        bs_skip( &s, i_size * 8 - ( i_end_bit_pos - i_start_bit_pos ) );
    }
}
