/*****************************************************************************
 * a52tospdif.c : encapsulates A/52 frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

struct frmsize_s
{
  uint16_t bit_rate;
  uint16_t frm_size[3];
};

static const struct frmsize_s frmsizecod_tbl[64] =
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( _("audio filter for A/52->S/PDIF encapsulation") );
    set_capability( "audio filter", 10 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('a','5','2',' ') ||
         ( p_filter->output.i_format != VLC_FOURCC('s','p','d','b') &&
           p_filter->output.i_format != VLC_FOURCC('s','p','d','i') ) )
    {
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    /* AC3 is natively big endian. Most SPDIF devices have the native endianness of
     * the computersystem. On MAc OS X however, little endian devices are also common.
     */
    uint32_t i_syncword, i_crc1, i_fscod, i_frmsizecod, i_bsid, i_bsmod, i_frame_size;
    static const uint8_t p_sync_le[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x01, 0x00 };
    static const uint8_t p_sync_be[6] = { 0xF8, 0x72, 0x4E, 0x1F, 0x00, 0x01 };
#ifndef HAVE_SWAB
    byte_t * p_tmp;
    uint16_t i;
#endif
    byte_t * p_in = p_in_buf->p_buffer;
    byte_t * p_out = p_out_buf->p_buffer;

    /* AC3 header decode */
    i_syncword = p_in[0] | (p_in[1] << 8);
    i_crc1 = p_in[2] | (p_in[3] << 8);
    i_fscod = (p_in[4] >> 6) & 0x3;
    i_frmsizecod = p_in[4] & 0x3f;
    i_bsid = (p_in[5] >> 3) & 0x1f;
    i_bsmod = p_in[5] & 0x7;
    i_frame_size = frmsizecod_tbl[i_frmsizecod].frm_size[i_fscod];
    
    /* Copy the S/PDIF headers. */
    if( p_filter->output.i_format == VLC_FOURCC('s','p','d','b') )
    {
        p_filter->p_vlc->pf_memcpy( p_out, p_sync_be, 6 );
        p_out[4] = i_bsmod;
        p_out[6] = ((i_frame_size ) >> 4) & 0xff;
        p_out[7] = (i_frame_size << 4) & 0xff;
        p_filter->p_vlc->pf_memcpy( &p_out[8], p_in, i_frame_size * 2 );
    }
    else
    {
        p_filter->p_vlc->pf_memcpy( p_out, p_sync_le, 6 );
        p_out[5] = i_bsmod;
        p_out[6] = (i_frame_size << 4) & 0xff;
        p_out[7] = ((i_frame_size ) >> 4) & 0xff;
#ifdef HAVE_SWAB
        swab( p_in, &p_out[8], i_frame_size * 2 );
#else
        p_tmp = &p_out[8];
        for( i = i_frame_size; i-- ; )
        {
            p_tmp[0] = p_in[1];
            p_tmp[1] = p_in[0];
            p_tmp += 2; p_in += 2;
        }
#endif
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = AOUT_SPDIF_SIZE;
}

