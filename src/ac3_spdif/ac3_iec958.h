/*****************************************************************************
 * ac3_iec958.h: ac3 to spdif converter headers
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ac3_iec958.h,v 1.1 2001/04/29 02:48:51 stef Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Juha Yrjola <jyrjola@cc.hut.fi>
 *          German Gomez Garcia <german@piraos.com>
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
 ****************************************************************************/

#ifndef _AC3_IEC958_H
#define _AC3_IEC958_H

/****************************************************************************
 * information about ac3 frame
 ****************************************************************************/
typedef struct ac3_info_s
{
    int i_bit_rate;
    int i_frame_size;
    int i_sample_rate;
    int i_bs_mod;
} ac3_info_t;

typedef struct sync_frame_s
{
    struct syncinfo
    {
        u8      syncword[2];
        u8      crc1[2];
        u8      code;
    } syncinfo;

    struct bsi
    {
        u8      bsidmod;
        u8      acmod;
    } bsi;
} sync_frame_t;

/****************************************************************************
 * Prototypes
 ****************************************************************************/
void    ac3_iec958_build_burst      ( int, u8 *, u8 * );
int     ac3_iec958_parse_syncinfo   ( struct ac3_spdif_thread_s *,
                                      struct ac3_info_s *, u8 * );
#endif
