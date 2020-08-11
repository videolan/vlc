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
#include <vlc_block.h>
#include <vlc_bits.h>

#include "hxxx_sei.h"
#include "hxxx_nal.h"
#include "hxxx_ep3b.h"

void HxxxParse_AnnexB_SEI(const uint8_t *p_buf, size_t i_buf,
                          uint8_t i_header, pf_hxxx_sei_callback cb, void *cbdata)
{
    if( hxxx_strip_AnnexB_startcode( &p_buf, &i_buf ) )
        HxxxParseSEI(p_buf, i_buf, i_header, cb, cbdata);
}

void HxxxParseSEI(const uint8_t *p_buf, size_t i_buf,
                  uint8_t i_header, pf_hxxx_sei_callback pf_callback, void *cbdata)
{
    bs_t s;
    bool b_continue = true;

    if( i_buf <= i_header )
        return;

    struct hxxx_bsfw_ep3b_ctx_s bsctx;
    hxxx_bsfw_ep3b_ctx_init( &bsctx );
    bs_init_custom( &s, &p_buf[i_header], i_buf - i_header, /* skip nal unit header */
                    &hxxx_bsfw_ep3b_callbacks, &bsctx );


    while( !bs_eof( &s ) && bs_aligned( &s ) && b_continue )
    {
        /* Read type */
        unsigned i_type = 0;
        while( !bs_eof( &s ) )
        {
            const uint8_t i_byte = bs_read( &s, 8 );
            i_type += i_byte;
            if( i_byte != 0xff )
                break;
        }

        if( bs_error( &s ) )
            return;

        /* Read size */
        unsigned i_size = 0;
        while( !bs_eof( &s ) )
        {
            const uint8_t i_byte = bs_read( &s, 8 );
            i_size += i_byte;
            if( i_byte != 0xff )
                break;
        }

        if( bs_error( &s ) )
            return;

        /* Check room */
        if( bs_eof( &s ) )
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
                b_continue = pf_callback( &sei_data, cbdata );
            } break;

            /* Look for user_data_registered_itu_t_t35 */
            case HXXX_SEI_USER_DATA_REGISTERED_ITU_T_T35:
            {
                size_t i_t35;
                uint8_t *p_t35 = malloc( i_size );
                if( !p_t35 )
                    break;

                for( i_t35 = 0; i_t35<i_size && !bs_eof( &s ); i_t35++ )
                    p_t35[i_t35] = bs_read( &s, 8 );

                if( bs_error( &s ) )
                    break;

                /* TS 101 154 Auxiliary Data and H264/AVC video */
                if( i_t35 > 4 && p_t35[0] == 0xb5 /* United States */ )
                {
                    if( p_t35[1] == 0x00 && p_t35[2] == 0x31 && /* US provider code for ATSC / DVB1 */
                        i_t35 > 7 )
                    {
                        switch( VLC_FOURCC(p_t35[3],p_t35[4],p_t35[5],p_t35[6]) )
                        {
                            case VLC_FOURCC('G', 'A', '9', '4'):
                                if( p_t35[7] == 0x03 )
                                {
                                    sei_data.itu_t35.type = HXXX_ITU_T35_TYPE_CC;
                                    sei_data.itu_t35.u.cc.i_data = i_t35 - 8;
                                    sei_data.itu_t35.u.cc.p_data = &p_t35[8];
                                    b_continue = pf_callback( &sei_data, cbdata );
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    else if( p_t35[1] == 0x00 && p_t35[2] == 0x2f && /* US provider code for DirecTV */
                             p_t35[3] == 0x03 && i_t35 > 5 )
                    {
                        /* DirecTV does not use GA94 user_data identifier */
                        sei_data.itu_t35.type = HXXX_ITU_T35_TYPE_CC;
                        sei_data.itu_t35.u.cc.i_data = i_t35 - 5;
                        sei_data.itu_t35.u.cc.p_data = &p_t35[5];
                        b_continue = pf_callback( &sei_data, cbdata );
                    }
                }

                free( p_t35 );
            } break;

            case HXXX_SEI_FRAME_PACKING_ARRANGEMENT:
            {
                bs_read_ue( &s );
                if ( !bs_read1( &s ) )
                {
                    sei_data.frame_packing.type = bs_read( &s, 7 );
                    bs_read( &s, 1 );
                    if( bs_read( &s, 6 ) == 2 ) /*intpr type*/
                        sei_data.frame_packing.b_left_first = false;
                    else
                        sei_data.frame_packing.b_left_first = true;
                    sei_data.frame_packing.b_flipped = bs_read1( &s );
                    sei_data.frame_packing.b_fields = bs_read1( &s );
                    sei_data.frame_packing.b_frame0 = bs_read1( &s );
                }
                else sei_data.frame_packing.type = FRAME_PACKING_CANCEL;

            } break;

            /* Look for SEI recovery point */
            case HXXX_SEI_RECOVERY_POINT:
            {
                sei_data.recovery.i_frames = bs_read_ue( &s );
                //bool b_exact_match = bs_read( &s, 1 );
                //bool b_broken_link = bs_read( &s, 1 );
                //int i_changing_slice_group = bs_read( &s, 2 );
                b_continue = pf_callback( &sei_data, cbdata );
            } break;

            case HXXX_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
            {
                for ( size_t i = 0; i < 6 ; ++i)
                    sei_data.colour_volume.primaries[i] = bs_read( &s, 16 );
                for ( size_t i = 0; i < 2 ; ++i)
                    sei_data.colour_volume.white_point[i] = bs_read( &s, 16 );
                sei_data.colour_volume.max_luminance = bs_read( &s, 32 );
                sei_data.colour_volume.min_luminance = bs_read( &s, 32 );
                if( bs_error( &s ) ) /* not enough data */
                    break;
                b_continue = pf_callback( &sei_data, cbdata );
            } break;

            case HXXX_SEI_CONTENT_LIGHT_LEVEL:
            {
                sei_data.content_light_lvl.MaxCLL = bs_read( &s, 16 );
                sei_data.content_light_lvl.MaxFALL = bs_read( &s, 16 );
                if( bs_error( &s ) ) /* not enough data */
                    break;
                b_continue = pf_callback( &sei_data, cbdata );
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
