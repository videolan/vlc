/*****************************************************************************
 * ts_arib.h : TS demux ARIB specific handling
 *****************************************************************************
 * Copyright (C) 2017 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifndef VLC_TS_ARIB_H
#define VLC_TS_ARIB_H

#define TS_ARIB_CDT_PID                                0x29

#define TS_ARIB_CDT_TABLE_ID                           0xC8

#define TS_ARIB_DR_LOGO_TRANSMISSION                   0xCF

#define TS_ARIB_CDT_DATA_TYPE_LOGO      0x01

#define TS_ARIB_LOGO_TYPE_HD_LARGE      0x05
#define TS_ARIB_LOGO_TYPE_HD_SMALL      0x02
#define TS_ARIB_LOGO_TYPE_SD43_LARGE    0x03
#define TS_ARIB_LOGO_TYPE_SD43_SMALL    0x00
#define TS_ARIB_LOGO_TYPE_SD169_LARGE   0x04
#define TS_ARIB_LOGO_TYPE_SD169_SMALL   0x01

/* logo_transmission_descriptor */
typedef struct
{
    uint8_t  i_transmission_mode;
    uint16_t i_logo_id;
    uint16_t i_logo_version;
    uint16_t i_download_data_id;
    uint8_t *p_logo_char;
    size_t   i_logo_char;
} ts_arib_logo_dr_t;

ts_arib_logo_dr_t * ts_arib_logo_dr_Decode( const uint8_t *, size_t );
void ts_arib_logo_dr_Delete( ts_arib_logo_dr_t * );

bool ts_arib_inject_png_palette( const uint8_t *p_in, size_t i_in,
                                 uint8_t **pp_out, size_t *pi_out );

#endif
