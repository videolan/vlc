/*****************************************************************************
 * render.c : Philips OGT (SVCD Subtitle) renderer
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: render.c,v 1.2 2003/12/27 01:49:59 rocky Exp $
 *
 * Author: Rocky Bernstein 
 *   based on code from: 
 *          Sam Hocevar <sam@zoy.org>
 *          Rudolf Cornelissen <rag.cornelissen@inter.nl.net>
 *          Roine Gustafsson <roine@popstar.com>
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

#include "ogt.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RenderI420( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );

/*****************************************************************************
 * RenderSPU: draw an SPU on a picture
 *****************************************************************************
 
  This is a fast implementation of the subpicture drawing code. The
  data has been preprocessed. Each byte has a run-length 1 in the upper
  nibble and a color in the lower nibble. The interleaving of rows has
  been done. Most sanity checks are already done so that this
  routine can be as fast as possible.

 *****************************************************************************/
void E_(RenderSPU)( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_spu )
{

    /*
      printf("+++%x\n", p_vout->output.i_chroma);
    */
  
    switch( p_vout->output.i_chroma )
    {
        /* I420 target, no scaling */
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
            RenderI420( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
            break;

        /* RV16 target, scaling */
        case VLC_FOURCC('R','V','1','6'):
            msg_Err( p_vout, "RV16 not implimented yet" );
	    break;

        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
            msg_Err( p_vout, "RV24/VF32 not implimented yet" );
	    break;

        /* NVidia overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            msg_Err( p_vout, "YUV2 not implimented yet" );
	    break;

        default:
            msg_Err( p_vout, "unknown chroma, can't render SPU" );
            break;
    }
}

/* Following functions are local */

static void RenderI420( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
  /* Common variables */
  uint8_t *p_dest;
  uint8_t *p_destptr;
  ogt_yuvt_t *p_source;
  
  int i_x, i_y;
  uint16_t i_colprecomp, i_destalpha;
  
  /* Crop-specific */
  int i_x_start, i_y_start, i_x_end, i_y_end;
  /* int i=0; */
  
  p_dest = p_pic->Y_PIXELS + p_spu->i_x + p_spu->i_width
    + p_pic->Y_PITCH * ( p_spu->i_y + p_spu->i_height );
  
  i_x_start = p_spu->i_width - p_spu->p_sys->i_x_end;
  i_y_start = p_pic->Y_PITCH * (p_spu->i_height - p_spu->p_sys->i_y_end );
  i_x_end   = p_spu->i_width - p_spu->p_sys->i_x_start;
  i_y_end   = p_pic->Y_PITCH * (p_spu->i_height - p_spu->p_sys->i_y_start );
  
  p_source = (ogt_yuvt_t *)p_spu->p_sys->p_data;

  /* printf("+++spu width: %d, height %d\n", p_spu->i_width, 
     p_spu->i_height); */
  
  /* Draw until we reach the bottom of the subtitle */
  for( i_y = p_spu->i_height * p_pic->Y_PITCH ;
       i_y ;
       i_y -= p_pic->Y_PITCH )
    {
      /* printf("+++begin line: %d,\n", i++); */
      /* Draw until we reach the end of the line */
      for( i_x = p_spu->i_width ; i_x ; i_x--, p_source++ )
        {
	  
	  if( b_crop
	      && ( i_x < i_x_start || i_x > i_x_end
		   || i_y < i_y_start || i_y > i_y_end ) )
            {
	      continue;
            }

	  /* printf( "t: %x, y: %x, u: %x, v: %x\n", 
	     p_source->t, p_source->y, p_source->u, p_source->v ); */
	  
	  switch( p_source->t )
            {
	    case 0x00: 
	      /* Completely transparent. Don't change underlying pixel */
	      break;
	      
	    case 0x0f:
	      /* Completely opaque. Completely overwrite underlying
		 pixel with subtitle pixel. */

	      p_destptr = p_dest - i_x - i_y;

	      i_colprecomp = (uint16_t) ( p_source->y * 15 );
	      *p_destptr = i_colprecomp >> 4;

	      break;
	      
	    default:
	      /* Blend in underlying pixel subtitle pixel. */

	      /* To be able to divide by 16 (>>4) we add 1 to the alpha.
	       * This means Alpha 0 won't be completely transparent, but
	       * that's handled in a special case above anyway. */

	      p_destptr = p_dest - i_x - i_y;

	      i_colprecomp = (uint16_t) ( (p_source->y 
					   * (uint16_t)(p_source->t + 1) ) );
	      i_destalpha = 15 - p_source->t;

	      *p_destptr = ( i_colprecomp +
			       (uint16_t)*p_destptr * i_destalpha ) >> 4;
	      break;
            }
        }
    }
}

