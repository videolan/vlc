/*****************************************************************************
 * render.c : Philips OGT (SVCD Subtitle) renderer
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: render.c,v 1.5 2003/12/29 04:47:44 rocky Exp $
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

#include "subtitle.h"
#include "render.h"

/* We use 4 bits for an alpha value: 0..15, 15 is completely transparent and
   0 completely opaque. Note that SVCD allow 8-bits, it should be 
   scaled down to use these routines.
*/
#define ALPHA_BITS (4)
#define MAX_ALPHA  ((1<<ALPHA_BITS) - 1) 

/* Horrible hack to get dbg_print to do the right thing */
#define p_dec p_vout

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
void VCDSubRender( vout_thread_t *p_vout, picture_t *p_pic,
		   const subpicture_t *p_spu )
{
    struct subpicture_sys_t *p_sys = p_spu->p_sys;
  
    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	       "chroma %x", p_vout->output.i_chroma );
  
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

  unsigned int i_plane;
  int i_x, i_y;
  uint16_t i_colprecomp, i_destalpha;
  
  /* Crop-specific */
  int i_x_start, i_y_start, i_x_end, i_y_end;
  /* int i=0; */

  struct subpicture_sys_t *p_sys = p_spu->p_sys;
  
  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	     "spu width: %d, height %d, pitch (%d, %d, %d)", 
	     p_spu->i_width,  p_spu->i_height, 
	     p_pic->Y_PITCH, p_pic->U_PITCH, p_pic->V_PITCH );

  
  /*for ( i_plane = 0; i_plane < p_pic->i_planes ; i_plane++ )*/
  for ( i_plane = 0; i_plane < 1 ; i_plane++ )
    {
      
      p_dest = p_pic->p[i_plane].p_pixels + p_spu->i_x + p_spu->i_width
	+ p_pic->p[i_plane].i_pitch * ( p_spu->i_y + p_spu->i_height );
      
      i_x_start = p_spu->i_width - p_sys->i_x_end;
      i_y_start = p_pic->p[i_plane].i_pitch 
	* (p_spu->i_height - p_sys->i_y_end );
      
      i_x_end   = p_spu->i_width - p_sys->i_x_start;
      i_y_end   = p_pic->p[i_plane].i_pitch 
	* (p_spu->i_height - p_sys->i_y_start );
      
      p_source = (ogt_yuvt_t *)p_sys->p_data;
      
      /* Draw until we reach the bottom of the subtitle */
      for( i_y = p_spu->i_height * p_pic->p[i_plane].i_pitch ;
	   i_y ;
	   i_y -= p_pic->p[i_plane].i_pitch )
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
		 p_source->s.t, p_source->y, p_source->u, p_source->v ); */
	      
	      switch( p_source->s.t )
		{
		case 0x00: 
		  /* Completely transparent. Don't change underlying pixel */
		  break;
		  
		case MAX_ALPHA:
		  /* Completely opaque. Completely overwrite underlying
		     pixel with subtitle pixel. */
		  
		  p_destptr = p_dest - i_x - i_y;
		  
		  i_colprecomp = 
		    (uint16_t) ( p_source->plane[i_plane] * MAX_ALPHA );
		  *p_destptr = i_colprecomp >> ALPHA_BITS;
		  
		  break;
		  
		default:
		  /* Blend in underlying pixel subtitle pixel. */
		  
		  /* To be able to ALPHA_BITS, we add 1 to the alpha.
		   * This means Alpha 0 won't be completely transparent, but
		   * that's handled in a special case above anyway. */
		  
		  p_destptr = p_dest - i_x - i_y;
		  
		  i_colprecomp = (uint16_t) (p_source->plane[i_plane] 
			       * (uint16_t) (p_source->s.t+1) );
		  i_destalpha = MAX_ALPHA - p_source->s.t;
		  
		  *p_destptr = ( i_colprecomp +
				 (uint16_t)*p_destptr * i_destalpha ) 
		    >> ALPHA_BITS;
		  break;
		}
	    }
	}
  }
}

