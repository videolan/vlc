/*****************************************************************************
 * ac3_iec958.c: ac3 to spdif converter
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ac3_iec958.c,v 1.1 2001/04/29 02:48:51 stef Exp $
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
void ac3_iec958_build_burst( int i_length, u8 * pi_data, u8 * pi_out )
{
    const u8 pi_sync[4] = { 0x72, 0xF8, 0x1F, 0x4E };

    /* add the spdif headers */
    memcpy( pi_out, pi_sync, 4 );
    if( i_length )
        pi_out[4] = 0x01;
    else
        pi_out[4] = 0;
    pi_out[5] = 0x00;
    pi_out[6] = ( i_length *8 ) & 0xFF;
    pi_out[7] = ( ( i_length *8 ) >> 8 ) & 0xFF;

    swab( pi_data, pi_out + 8, i_length );
    /* adds zero to complete the spdif frame
     * they will be ignored by the decoder */
    memset( pi_out + 8 + i_length, 0, SPDIF_FRAME - 8 - i_length );
}

/****************************************************************************
 * ac3_iec958_parse_syncinfo: parse ac3 sync info
 ****************************************************************************/
int ac3_iec958_parse_syncinfo( ac3_spdif_thread_t *p_spdif,
                               ac3_info_t *ac3_info,
                               u8 * pi_ac3 )
{
    int             pi_sample_rates[4] = { 48000, 44100, 32000, -1 };
    int             i_frame_rate_code;
    int             i_frame_size_code;
//    u8 *            pi_tmp;
    sync_frame_t *  p_sync_frame;

    /* find sync word */
    while( ShowBits( &p_spdif->bit_stream, 16 ) != 0xb77 )
    {
        RemoveBits( &p_spdif->bit_stream, 8 );
    }

    /* read sync frame */
    pi_ac3 = malloc( sizeof(sync_frame_t) );
    GetChunk( &p_spdif->bit_stream, pi_ac3, sizeof(sync_frame_t) );
    p_sync_frame = (sync_frame_t*)pi_ac3;

    /* compute frame rate */
    i_frame_rate_code = (p_sync_frame->syncinfo.code >> 6) & 0x03;
    ac3_info->i_sample_rate = pi_sample_rates[i_frame_rate_code];
    if (ac3_info->i_sample_rate == -1)
    {
        return -1;
    }

    /* compute frame size */
    i_frame_size_code = p_sync_frame->syncinfo.code & 0x3f;
    ac3_info->i_frame_size = 2 *
        p_frame_size_code[i_frame_size_code].i_frame_size[i_frame_rate_code];
    ac3_info->i_bit_rate = p_frame_size_code[i_frame_size_code].i_bit_rate;

    if( ( ( p_sync_frame->bsi.bsidmod >> 3 ) & 0x1f ) != 0x08 )
    {
        return -1;
    }

    ac3_info->i_bs_mod = p_sync_frame->bsi.bsidmod & 0x7;

//    free( pi_tmp );

    return 0;
}
