/*****************************************************************************
 * Common SVCD and VCD subtitle routines.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: common.c,v 1.1 2003/12/28 04:51:52 rocky Exp $
 *
 * Author: Rocky Bernstein
 *   based on code from:
 *       Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
 *       Samuel Hocevar <sam@zoy.org>
 *       Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "subtitle.h"
#include "common.h"

/*****************************************************************************
 Free Resources associated with subtitle packet.
 *****************************************************************************/
void VCDSubClose( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

    if( !p_sys->b_packetizer )
    {
        /* FIXME check if it's ok to not lock vout */
        if( p_sys->p_vout != NULL && p_sys->p_vout->p_subpicture != NULL )
        {
            subpicture_t *  p_subpic;
            int             i_subpic;

            for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
            {
                p_subpic = &p_sys->p_vout->p_subpicture[i_subpic];

                if( p_subpic != NULL &&
                    ( ( p_subpic->i_status == RESERVED_SUBPICTURE ) ||
                      ( p_subpic->i_status == READY_SUBPICTURE ) ) )
                {
                    vout_DestroySubPicture( p_sys->p_vout, p_subpic );
                }
            }
        }
    }

    if( p_sys->p_block )
    {
        block_ChainRelease( p_sys->p_block );
    }

    free( p_sys );
}

/*****************************************************************************

Initialize so the next packet will start off a new one.

 *****************************************************************************/
void 
VCDSubInitSubtitleBlock( decoder_sys_t * p_sys ) 
{
  p_sys->i_spu_size = 0;
  p_sys->state      = SUBTITLE_BLOCK_EMPTY;
  p_sys->i_spu      = 0;
  p_sys->p_block    = NULL;
  p_sys->subtitle_data_pos = 0;

}

void 
VCDSubInitSubtitleData(decoder_sys_t *p_sys)
{
  if ( p_sys->subtitle_data ) {
    if ( p_sys->subtitle_data_size < p_sys->i_spu_size ) {
      p_sys->subtitle_data = realloc(p_sys->subtitle_data,
				    p_sys->i_spu_size);
      p_sys->subtitle_data_size = p_sys->i_spu_size;
    }
  } else {
    p_sys->subtitle_data = malloc(p_sys->i_spu_size);
    p_sys->subtitle_data_size = p_sys->i_spu_size;
    /* FIXME: wrong place to get p_sys */
    p_sys->i_image = 0;
  }
  p_sys->subtitle_data_pos = 0;
}

void 
VCDSubAppendData ( decoder_t *p_dec, uint8_t *buffer, uint32_t buf_len )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  int chunk_length = buf_len;

  if ( chunk_length > p_sys->i_spu_size - p_sys->subtitle_data_pos ) {
    msg_Warn( p_dec, "too much data (%d) expecting at most %u",
	      chunk_length, p_sys->i_spu_size - p_sys->subtitle_data_pos );

    chunk_length = p_sys->i_spu_size - p_sys->subtitle_data_pos;
  }

  if ( chunk_length > 0 ) {
    memcpy(p_sys->subtitle_data + p_sys->subtitle_data_pos,
	   buffer, chunk_length);
    p_sys->subtitle_data_pos += chunk_length;
    dbg_print(DECODE_DBG_PACKET, "%d bytes appended, pointer now %d",
	      chunk_length, p_sys->subtitle_data_pos);
  }
}


/*****************************************************************************
 * FindVout: Find a vout or wait for one to be created.
 *****************************************************************************/
vout_thread_t *VCDSubFindVout( decoder_t *p_dec )
{
    vout_thread_t *p_vout = NULL;

    /* Find an available video output */
    do
    {
        if( p_dec->b_die || p_dec->b_error )
        {
            break;
        }

        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout )
        {
            break;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    while( 1 );

    return p_vout;
}


/* Scales down (reduces size) of p_dest in the x direction as 
   determined through aspect ratio x_scale by y_scale. Scaling
   is done in place. p_spu->i_width, is updated to new width

   The aspect ratio is assumed to be between 1/2 and 1.
*/
void
VCDSubScaleX( decoder_t *p_dec, subpicture_t *p_spu, 
	      unsigned int i_scale_x, unsigned int i_scale_y )
{
  int i_row, i_col;

  decoder_sys_t *p_sys = p_dec->p_sys;
  uint8_t *p_src1 = p_spu->p_sys->p_data;
  uint8_t *p_src2 = p_src1 + PIXEL_SIZE;
  uint8_t *p_dst  = p_src1;
  unsigned int i_new_width = (p_spu->i_width * i_scale_x) / i_scale_y ;
  unsigned int used=0;  /* Number of bytes used up in p_src1. */

  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_TRANSFORM) , 
	     "Old width: %d, new width: %d", 
	     p_spu->i_width, i_new_width);
  
  for ( i_row=0; i_row <= p_spu->i_height - 1; i_row++ ) {

    if (used != 0) {
      /* Discard the remaining piece of the column of the previous line*/
      used=0;
      p_src1 = p_src2;
      p_src2 += PIXEL_SIZE;
    }
    
    for ( i_col=0; i_col <= p_spu->i_width - 2; i_col++ ) {
      unsigned int i;
      unsigned int w1= i_scale_x - used;
      unsigned int w2= i_scale_y - w1;

      used = w2;
      for (i = 0; i < PIXEL_SIZE; i++ ) {
	*p_dst = ( (*p_src1 * w1) + (*p_src2 * w2) ) / i_scale_y;
	p_src1++; p_src2++; p_dst++;
      }

      if (i_scale_x == used) {
	/* End of last pixel was end of p_src2. */
	p_src1 = p_src2;
	p_src2 += PIXEL_SIZE;
	i_col++;
	used = 0;
      }
    }
  }
  p_spu->i_width = i_new_width;

  if ( p_sys && p_sys->i_debug & DECODE_DBG_TRANSFORM )
  { 
    ogt_yuvt_t *p_source = (ogt_yuvt_t *) p_spu->p_sys->p_data;
    for ( i_row=0; i_row < p_spu->i_height - 1; i_row++ ) {
      for ( i_col=0; i_col < p_spu->i_width - 1; i_col++ ) {
	printf("%1x", p_source->s.t);
	p_source++;
      }
      printf("\n");
    }
  }

}

/*****************************************************************************
 * DestroySPU: subpicture destructor
 *****************************************************************************/
void VCDSubDestroySPU( subpicture_t *p_spu )
{
    if( p_spu->p_sys->p_input )
    {
        /* Detach from our input thread */
        vlc_object_release( p_spu->p_sys->p_input );
    }

    vlc_mutex_destroy( &p_spu->p_sys->lock );
    free( p_spu->p_sys );
}

/*****************************************************************************
  This callback is called from the input thread when we need cropping
 *****************************************************************************/
int VCDSubCropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VCDSubUpdateSPU( (subpicture_t *)p_data, p_object );

    return VLC_SUCCESS;
}


/*****************************************************************************
  update subpicture settings
 *****************************************************************************
  This function is called from CropCallback and at initialization time, to
  retrieve crop information from the input.
 *****************************************************************************/
void VCDSubUpdateSPU( subpicture_t *p_spu, vlc_object_t *p_object )
{
    vlc_value_t val;

    p_spu->p_sys->b_crop = val.b_bool;
    if( !p_spu->p_sys->b_crop )
    {
        return;
    }

    var_Get( p_object, "x-start", &val );
    p_spu->p_sys->i_x_start = val.i_int;
    var_Get( p_object, "y-start", &val );
    p_spu->p_sys->i_y_start = val.i_int;
    var_Get( p_object, "x-end", &val );
    p_spu->p_sys->i_x_end = val.i_int;
    var_Get( p_object, "y-end", &val );
    p_spu->p_sys->i_y_end = val.i_int;

}

