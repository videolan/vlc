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
    HXXX_SEI_RECOVERY_POINT = 6,
    HXXX_SEI_FRAME_PACKING_ARRANGEMENT = 45,
    HXXX_SEI_MASTERING_DISPLAY_COLOUR_VOLUME = 137, /* SMPTE ST 2086 */
    HXXX_SEI_CONTENT_LIGHT_LEVEL = 144,
};

enum hxxx_sei_t35_type_e
{
    HXXX_ITU_T35_TYPE_CC,
};

typedef struct
{
    unsigned i_type;
    union
    {
        bs_t *p_bs; /* for raw/unhandled in common code callbacks */
        struct
        {
            enum hxxx_sei_t35_type_e type;
            union
            {
                struct
                {
                    const uint8_t *p_data;
                    size_t i_data;
                } cc;
            } u;
        } itu_t35;
        struct
        {
            enum
            {
                FRAME_PACKING_CANCEL = -1,
                FRAME_PACKING_INTERLEAVED_CHECKERBOARD = 0,
                FRAME_PACKING_INTERLEAVED_COLUMN,
                FRAME_PACKING_INTERLEAVED_ROW,
                FRAME_PACKING_SIDE_BY_SIDE,
                FRAME_PACKING_TOP_BOTTOM,
                FRAME_PACKING_TEMPORAL,
                FRAME_PACKING_NONE_2D,
                FRAME_PACKING_TILED,
            } type;
            bool b_flipped;
            bool b_left_first;
            bool b_fields;
            bool b_frame0;
        } frame_packing;
        struct
        {
            int i_frames;
        } recovery;
        struct
        {
            uint16_t primaries[3*2]; /* G,B,R / x,y */
            uint16_t white_point[2]; /* x,y */
            uint32_t max_luminance;
            uint32_t min_luminance;
        } colour_volume; /* SMPTE ST 2086 */
        struct
        {
            uint16_t MaxCLL;
            uint16_t MaxFALL;
        } content_light_lvl; /* CTA-861.3 */
    };
} hxxx_sei_data_t;

typedef bool (*pf_hxxx_sei_callback)(const hxxx_sei_data_t *, void *);
void HxxxParseSEI(const uint8_t *, size_t, uint8_t, pf_hxxx_sei_callback, void *);
void HxxxParse_AnnexB_SEI(const uint8_t *, size_t, uint8_t, pf_hxxx_sei_callback, void *);

#endif
