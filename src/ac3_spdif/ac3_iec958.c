/*****************************************************************************
 * ac3_iec958.c: ac3 to spdif converter
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ac3_iec958.c,v 1.3 2001/05/06 04:32:02 sam Exp $
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

/****************************************************************************
 * Preamble
 ****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>                                              /* memset() */
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"

#include "ac3_spdif.h"
#include "ac3_iec958.h"

/****************************************************************************
 * Local structures and tables
 ****************************************************************************/
typedef struct frame_size_s
{
    u16     i_bit_rate;
    u16     i_frame_size[3];
} frame_size_t;
                
static const frame_size_t p_frame_size_code[64] =
{
        { 32  ,{64   ,69   ,96   } },
        { 32  ,{64   ,70   ,96   } },
        { 40  ,{80   ,87   ,120  } },
        { 40  ,{80   ,88   ,120  } },
        { 48  ,{96   ,104  ,144  } },
        { 48  ,{96   ,105  ,144  } },
        { 56  ,{112  ,121  ,168  } },
        { 56  ,{112  ,122  ,168  } },
        { 64  ,{128  ,139  ,192  } },
        { 64  ,{128  ,140  ,192  } },
        { 80  ,{160  ,174  ,240  } },
        { 80  ,{160  ,175  ,240  } },
        { 96  ,{192  ,208  ,288  } },
        { 96  ,{192  ,209  ,288  } },
        { 112 ,{224  ,243  ,336  } },
        { 112 ,{224  ,244  ,336  } },
        { 128 ,{256  ,278  ,384  } },
        { 128 ,{256  ,279  ,384  } },
        { 160 ,{320  ,348  ,480  } },
        { 160 ,{320  ,349  ,480  } },
        { 192 ,{384  ,417  ,576  } },
        { 192 ,{384  ,418  ,576  } },
        { 224 ,{448  ,487  ,672  } },
        { 224 ,{448  ,488  ,672  } },
        { 256 ,{512  ,557  ,768  } },
        { 256 ,{512  ,558  ,768  } },
        { 320 ,{640  ,696  ,960  } },
        { 320 ,{640  ,697  ,960  } },
        { 384 ,{768  ,835  ,1152 } },
        { 384 ,{768  ,836  ,1152 } },
        { 448 ,{896  ,975  ,1344 } },
        { 448 ,{896  ,976  ,1344 } },
        { 512 ,{1024 ,1114 ,1536 } },
        { 512 ,{1024 ,1115 ,1536 } },
        { 576 ,{1152 ,1253 ,1728 } },
        { 576 ,{1152 ,1254 ,1728 } },
        { 640 ,{1280 ,1393 ,1920 } },
        { 640 ,{1280 ,1394 ,1920 } }
};

/****************************************************************************
 * ac3_iec958_build_burst: builds an iec958/spdif frame based on an ac3 frame
 ****************************************************************************/
void ac3_iec958_build_burst( ac3_spdif_thread_t *p_spdif )
{
    const u8 p_sync[4] = { 0x72, 0xF8, 0x1F, 0x4E };
    int      i_length  = p_spdif->ac3_info.i_frame_size;
#ifndef HAVE_SWAB
    /* Skip the first byte if i_length is odd */
    u16 * p_in  = (u16 *)( p_spdif->p_ac3 + ( i_length & 0x1 ) );
    u16 * p_out = (u16 *)p_spdif->p_iec;
#endif

    /* Add the spdif headers */
    memcpy( p_spdif->p_iec, p_sync, 4 );
    p_spdif->p_iec[4] = i_length ? 0x01 : 0x00;
    p_spdif->p_iec[5] = 0x00;
    p_spdif->p_iec[6] = ( i_length * 8 ) & 0xFF;
    p_spdif->p_iec[7] = ( ( i_length * 8 ) >> 8 ) & 0xFF;

#ifdef HAVE_SWAB
    swab( p_spdif->p_ac3, p_spdif->p_iec + 8, i_length );
#else
    /* i_length should be even */
    i_length &= ~0x1;

    while( i_length )
    {
        *p_out = ( (*p_in & 0x00ff) << 16 ) | ( (*p_in & 0xff00) >> 16 );
        p_in++;
        p_out++;
        i_length -= 2;
    }
#endif

    /* Add zeroes to complete the spdif frame,
     * they will be ignored by the decoder */
    memset( p_spdif->p_iec + 8 + i_length, 0, SPDIF_FRAME_SIZE - 8 - i_length );
}

/****************************************************************************
 * ac3_iec958_parse_syncinfo: parse ac3 sync info
 ****************************************************************************/
int ac3_iec958_parse_syncinfo( ac3_spdif_thread_t *p_spdif )
{
    int             p_sample_rates[4] = { 48000, 44100, 32000, -1 };
    int             i_frame_rate_code;
    int             i_frame_size_code;
    sync_frame_t *  p_sync_frame;

    /* Find sync word */
    while( ShowBits( &p_spdif->bit_stream, 16 ) != 0xb77 )
    {
        RemoveBits( &p_spdif->bit_stream, 8 );
    }

    /* Read sync frame */
    GetChunk( &p_spdif->bit_stream, p_spdif->p_ac3, sizeof(sync_frame_t) );
    p_sync_frame = (sync_frame_t*)p_spdif->p_ac3;

    /* Compute frame rate */
    i_frame_rate_code = (p_sync_frame->syncinfo.code >> 6) & 0x03;
    p_spdif->ac3_info.i_sample_rate = p_sample_rates[i_frame_rate_code];
    if( p_spdif->ac3_info.i_sample_rate == -1 )
    {
        return -1;
    }

    /* Compute frame size */
    i_frame_size_code = p_sync_frame->syncinfo.code & 0x3f;
    p_spdif->ac3_info.i_frame_size = 2 *
        p_frame_size_code[i_frame_size_code].i_frame_size[i_frame_rate_code];
    p_spdif->ac3_info.i_bit_rate =
        p_frame_size_code[i_frame_size_code].i_bit_rate;

    if( ( ( p_sync_frame->bsi.bsidmod >> 3 ) & 0x1f ) != 0x08 )
    {
        return -1;
    }

    p_spdif->ac3_info.i_bs_mod = p_sync_frame->bsi.bsidmod & 0x7;

    return 0;
}

