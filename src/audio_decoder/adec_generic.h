/*****************************************************************************
 * audio_decoder.h : audio decoder interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: adec_generic.h,v 1.4 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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

/**** audio decoder API - public audio decoder structures */

typedef struct audiodec_s audiodec_t;

typedef struct adec_sync_info_s {
    int sample_rate;    /* sample rate in Hz */
    int frame_size;     /* frame size in bytes */
    int bit_rate;       /* nominal bit rate in kbps */
} adec_sync_info_t;

typedef struct adec_bank_s
{
    float               v1[512];
    float               v2[512];
    float *             actual;
    int                 pos;
    
} adec_bank_t;

