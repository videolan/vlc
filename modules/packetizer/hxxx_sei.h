/*****************************************************************************
 * hxxx_sei.h: AVC/HEVC packetizers SEI handling
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
#ifndef HXXX_SEI_H
#define HXXX_SEI_H

/* Defined in H.264/H.265 annex D */
enum hxxx_sei_type_e
{
    HXXX_SEI_PIC_TIMING = 1,
    HXXX_SEI_USER_DATA_REGISTERED_ITU_T_T35 = 4,
    HXXX_SEI_RECOVERY_POINT = 6
};

typedef struct
{
    unsigned i_type;
    union
    {
        bs_t *p_bs; /* for raw/unhandled in common code callbacks */
        struct
        {
            const uint8_t *p_cc;
            size_t i_cc;
        } itu_t35;
        struct
        {
            int i_frames;
        } recovery;
    };
} hxxx_sei_data_t;

typedef void (*pf_hxxx_sei_callback)(decoder_t *, const hxxx_sei_data_t *);
void HxxxParseSEI(decoder_t *, const uint8_t *, size_t, uint8_t, pf_hxxx_sei_callback);

#endif
