/*****************************************************************************
 * render.c : Philips OGT (SVCD Subtitle) renderer
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id: render.c,v 1.10 2004/01/12 04:03:19 rocky Exp $
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
   0 completely opaque. Note that although SVCD allow 8-bits, pixels 
   previously should be scaled down to 4 bits to use these routines.
*/
#define ALPHA_BITS (4)
#define MAX_ALPHA  ((1<<ALPHA_BITS) - 1) 
#define ALPHA_SCALEDOWN (8-ALPHA_BITS)

/* Horrible hack to get dbg_print to do the right thing */
#define p_dec p_vout

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RenderI420( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );
static void RenderYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );
static void RenderRV16( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );
static void RenderRV32( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );

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
            RenderRV16( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
	    break;

        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
            RenderRV32( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
	    break;

        /* NVidia overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            RenderYUY2( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
	    break;

        /* Used in ASCII art */
        case VLC_FOURCC('R','G','B','2'):
            msg_Err( p_vout, "RGB2 not implimented yet" );
	    break;

        default:
            msg_Err( p_vout, "unknown chroma, can't render SPU" );
            break;
    }
}

/* Following functions are local */

/* 
  YV12 format:  

  All Y samples are found first in memory as an array of bytes
  (possibly with a larger stride for memory alignment), followed
  immediately by all Cr (=U) samples (with half the stride of the Y
  lines, and half the number of lines), then followed immediately by
  all Cb (=V) samples in a similar fashion.
*/

static void RenderI420( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
  /* Common variables */
  uint8_t *p_pixel_base_Y, *p_pixel_base_V, *p_pixel_base_U;
  ogt_yuvt_t *p_source;

  int i_x, i_y;
  vlc_bool_t even_scanline = VLC_FALSE;

  /* Crop-specific */
  int i_x_start, i_y_start, i_x_end, i_y_end;
  /* int i=0; */

  struct subpicture_sys_t *p_sys = p_spu->p_sys;
  
  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	     "spu width x height: (%dx%d), (x,y)=(%d,%d), yuv pitch (%d,%d,%d)", 
	     p_spu->i_width,  p_spu->i_height, p_spu->i_x, p_spu->i_y,
	     p_pic->Y_PITCH, p_pic->U_PITCH, p_pic->V_PITCH );

  
  p_pixel_base_Y = p_pic->p[Y_PLANE].p_pixels + p_spu->i_x
                 + p_pic->p[Y_PLANE].i_pitch * p_spu->i_y;
  
  p_pixel_base_U = p_pic->p[U_PLANE].p_pixels + p_spu->i_x/2
                 + p_pic->p[U_PLANE].i_pitch * p_spu->i_y/2;
  
  p_pixel_base_V = p_pic->p[V_PLANE].p_pixels + p_spu->i_x/2
                 + p_pic->p[V_PLANE].i_pitch * p_spu->i_y/2;
  
  i_x_start = p_sys->i_x_start;
  i_y_start = p_pic->p[Y_PLANE].i_pitch * p_sys->i_y_start;
  
  i_x_end   = p_sys->i_x_end;
  i_y_end   = p_pic->p[Y_PLANE].i_pitch * p_sys->i_y_end;
  
  p_source = (ogt_yuvt_t *)p_sys->p_data;
  
  /* Draw until we reach the bottom of the subtitle */
  for( i_y = 0; 
       i_y < p_spu->i_height * p_pic->p[Y_PLANE].i_pitch ;
       i_y += p_pic->p[Y_PLANE].i_pitch )
    {
      uint8_t *p_pixel_base_Y_y = p_pixel_base_Y + i_y;
      uint8_t *p_pixel_base_U_y = p_pixel_base_U + i_y/4;
      uint8_t *p_pixel_base_V_y = p_pixel_base_V + i_y/4;

      even_scanline = !even_scanline;

      /* printf("+++begin line: %d,\n", i++); */
      /* Draw until we reach the end of the line */
      for( i_x = 0; i_x < p_spu->i_width; i_x++, p_source++ )
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
	      /* Completely transparent. Don't change pixel. */
	      break;
	      
	    case MAX_ALPHA:
	      {
		/* Completely opaque. Completely overwrite underlying
		   pixel with subtitle pixel. */
		
		/* This is the location that's going to get changed.*/
		uint8_t *p_pixel_Y = p_pixel_base_Y_y + i_x;
		
		*p_pixel_Y = p_source->plane[Y_PLANE];

		if ( even_scanline && i_x % 2 == 0 ) {
		  uint8_t *p_pixel_U = p_pixel_base_U_y + i_x/2;
		  uint8_t *p_pixel_V = p_pixel_base_V_y + i_x/2;
		  *p_pixel_U = p_source->plane[U_PLANE];
		  *p_pixel_V = p_source->plane[V_PLANE];
		}
		
		break;
	      }
	      
	    default:
	      {
		/* Blend in underlying pixel subtitle pixel. */
		
		/* This is the location that's going to get changed.*/
		uint8_t *p_pixel_Y = p_pixel_base_Y_y + i_x;


		/* This is the weighted part of the subtitle. The
		   color plane is 8 bits and transparancy is 4 bits so
		   when multiplied we get up to 12 bits.
		 */
		uint16_t i_sub_color_Y = 
		  (uint16_t) ( p_source->plane[Y_PLANE] *
			       (uint16_t) (p_source->s.t) );

		/* This is the weighted part of the underlying pixel.
		   For the same reasons above, the result is up to 12
		   bits.  However since the transparancies are
		   inverses, the sum of i_sub_color and i_pixel_color
		   will not exceed 12 bits.
		*/
		uint16_t i_pixel_color_Y = 
		  (uint16_t) ( *p_pixel_Y * 
			       (uint16_t) (MAX_ALPHA - p_source->s.t) ) ;
		
		/* Scale the 12-bit result back down to 8 bits. A
		   precise scaling after adding the two components,
		   would divide by one less than a power of 2. However
		   to simplify and speed things we use a power of
		   2. This means the boundaries (either all
		   transparent and all opaque) aren't handled properly.
		   But we deal with them in special cases above. */

		*p_pixel_Y = ( i_sub_color_Y + i_pixel_color_Y ) >> 4;

		if ( even_scanline && i_x % 2 == 0 ) {
		  uint8_t *p_pixel_U = p_pixel_base_U_y + i_x/2;
		  uint8_t *p_pixel_V = p_pixel_base_V_y + i_x/2;
		  uint16_t i_sub_color_U = 
		    (uint16_t) ( p_source->plane[U_PLANE] *
				 (uint16_t) (p_source->s.t) );
		  
		  uint16_t i_sub_color_V = 
		    (uint16_t) ( p_source->plane[V_PLANE] *
				 (uint16_t) (p_source->s.t) );
		  uint16_t i_pixel_color_U = 
		    (uint16_t) ( *p_pixel_U * 
				 (uint16_t) (MAX_ALPHA - p_source->s.t) ) ;
		  uint16_t i_pixel_color_V = 
		    (uint16_t) ( *p_pixel_V * 
				 (uint16_t) (MAX_ALPHA - p_source->s.t) ) ;
		  *p_pixel_U = ( i_sub_color_U + i_pixel_color_U ) >> 4;
		  *p_pixel_V = ( i_sub_color_V + i_pixel_color_V ) >> 4;
		}
		break;
	      }
	      
	    }
	}
    }
}

/*

  YUY2 Format:

  Data is found in memory as an array of bytess in which the first
  byte contains the first sample of Y, the second byte contains the
  first sample of Cb (=U), the third byte contains the second sample
  of Y, the fourth byte contains the first sample of Cr (=V); and so
  on. If data is addressed as an array of two little-endian WORD type
  variables, the first WORD contains Y0 in the least significant bits
  and Cb in the most significant bits, and the second WORD contains Y1
  in the least significant bits and Cr in the most significant bits.
*/


static void RenderYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
  /* Common variables */
  uint8_t *p_pixel_base;
  ogt_yuvt_t *p_source;

  int i_x, i_y;
  vlc_bool_t even_scanline = VLC_FALSE;

  /* Crop-specific */
  int i_x_start, i_y_start, i_x_end, i_y_end;
  /* int i=0; */

  struct subpicture_sys_t *p_sys = p_spu->p_sys;
  
  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	     "spu width x height: (%dx%d), (x,y)=(%d,%d), pitch: %d", 
	     p_spu->i_width,  p_spu->i_height, p_spu->i_x, p_spu->i_y,
	     p_pic->p->i_pitch );

  
  p_pixel_base = p_pic->p->p_pixels +
              + ( p_spu->i_y + p_spu->i_height ) * p_pic->p->i_pitch 
              + ( p_spu->i_x + p_spu->i_width );
  
  i_x_start = p_sys->i_x_start;
  i_y_start = p_sys->i_y_start * p_pic->p->i_pitch;
  
  i_x_end   = p_sys->i_x_end;
  i_y_end   = p_sys->i_y_end   * p_pic->p->i_pitch;
  
  p_source = (ogt_yuvt_t *)p_sys->p_data;
  
  /* Draw until we reach the bottom of the subtitle */
  for( i_y = 0; 
       i_y < p_spu->i_height * p_pic->p->i_pitch;
       i_y += p_pic->p->i_pitch )
    {
      uint8_t *p_pixel_base_y = p_pixel_base + i_y;

      even_scanline = !even_scanline;

      /* printf("+++begin line: %d,\n", i++); */
      /* Draw until we reach the end of the line */
      for( i_x = 0; i_x < p_spu->i_width; i_x++, p_source++ )
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
	      /* Completely transparent. Don't change pixel. */
	      break;
	      
	    case MAX_ALPHA:
	      {
		/* Completely opaque. Completely overwrite underlying
		   pixel with subtitle pixel. */
		
		/* This is the location that's going to get changed.*/
		uint8_t *p_pixel = p_pixel_base_y + i_x;
		
		/* draw a pixel */
                    /* Y */
		*p_pixel++ = p_source->plane[Y_PLANE];

		if ( even_scanline ) {
		  *p_pixel++ = p_source->plane[U_PLANE];
		  *p_pixel++ = p_source->plane[Y_PLANE];
		  *p_pixel++ = p_source->plane[V_PLANE];
		}
		
		break;
	      }
	      
	    default:
	      {
		/* Blend in underlying pixel subtitle pixel. */
		
		/* This is the location that's going to get changed.*/
		uint8_t *p_pixel = p_pixel_base_y + i_x;


		/* This is the weighted part of the subtitle. The
		   color plane is 8 bits and transparancy is 4 bits so
		   when multiplied we get up to 12 bits.
		 */
		uint16_t i_sub_color_Y = 
		  (uint16_t) ( p_source->plane[Y_PLANE] *
			       (uint16_t) (p_source->s.t) );

		/* This is the weighted part of the underlying pixel.
		   For the same reasons above, the result is up to 12
		   bits.  However since the transparancies are
		   inverses, the sum of i_sub_color and i_pixel_color
		   will not exceed 12 bits.
		*/
		uint16_t i_pixel_color_Y = 
		  (uint16_t) ( *p_pixel * 
			       (uint16_t) (MAX_ALPHA - p_source->s.t) ) ;
		
		/* Scale the 12-bit result back down to 8 bits. A
		   precise scaling after adding the two components,
		   would divide by one less than a power of 2. However
		   to simplify and speed things we use a power of
		   2. This means the boundaries (either all
		   transparent and all opaque) aren't handled properly.
		   But we deal with them in special cases above. */

		*p_pixel++ = ( i_sub_color_Y + i_pixel_color_Y ) >> 4;

		if ( even_scanline ) {
		  uint16_t i_sub_color_U = 
		    (uint16_t) ( p_source->plane[U_PLANE] *
				 (uint16_t) (p_source->s.t) );
		  
		  uint16_t i_sub_color_V = 
		    (uint16_t) ( p_source->plane[V_PLANE] *
				 (uint16_t) (p_source->s.t) );
		  uint16_t i_pixel_color_U = 
		    (uint16_t) ( *(p_pixel+1) * 
				 (uint16_t) (MAX_ALPHA - p_source->s.t) ) ;
		  uint16_t i_pixel_color_V = 
		    (uint16_t) ( *(p_pixel+3) * 
				 (uint16_t) (MAX_ALPHA - p_source->s.t) ) ;
		  *p_pixel++ = ( i_sub_color_U + i_pixel_color_U ) >> 4;
		  *p_pixel++ = ( i_sub_color_Y + i_pixel_color_Y ) >> 4;
		  *p_pixel++ = ( i_sub_color_V + i_pixel_color_V ) >> 4;
		}
		break;
	      }
	      
	    }
	}
    }
}

#define Y2RV16(val) ((uint16_t) (0x1111 * ( (uint16_t) (val) >> 4 )))

static void RenderRV16( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    uint8_t *p_pixel_base;
    ogt_yuvt_t *p_src_start = (ogt_yuvt_t *)p_spu->p_sys->p_data;
    ogt_yuvt_t *p_src_end   = &p_src_start[p_spu->i_height * p_spu->i_width];

    int i_x, i_y;
    int i_x_src, i_y_src;

    /* RGB-specific */
    int i_xscale, i_yscale, i_width, i_height, i_ytmp, i_ynext;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    struct subpicture_sys_t *p_sys = p_spu->p_sys;

    i_xscale = ( p_vout->output.i_width << 6 ) / p_vout->render.i_width;
    i_yscale = ( p_vout->output.i_height << 6 ) / p_vout->render.i_height;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	       "spu: %dx%d, scaled: %dx%d, vout render: %dx%d, scale %dx%d", 
	       p_spu->i_width,  p_spu->i_height, 
	       p_vout->output.i_width, p_vout->output.i_height,
	       p_vout->render.i_width, p_vout->render.i_height,
	       i_xscale, i_yscale
	       );

    i_width  = p_spu->i_width  * i_xscale;
    i_height = p_spu->i_height * i_yscale;

    /* Set where we will start blending subtitle from using
       the picture coordinates subtitle offsets
    */
    p_pixel_base = p_pic->p->p_pixels + ( (p_spu->i_x * i_xscale) >> 6 ) * 2
              + ( (p_spu->i_y * i_yscale) >> 6 ) * p_pic->p->i_pitch;

    i_x_start = i_xscale * p_spu->p_sys->i_x_start;
    i_y_start = i_yscale * p_spu->p_sys->i_y_start;
    i_x_end   = i_xscale * p_spu->p_sys->i_x_end;
    i_y_end   = i_yscale * p_spu->p_sys->i_y_end;

    /* Draw until we reach the bottom of the subtitle */
    i_y = 0;
    for( i_y_src = 0 ; i_y_src < p_spu->i_height ; i_y_src++ )
    {
	uint8_t *p_pixel_base_y;
        i_ytmp = i_y >> 6;
        i_y += i_yscale;
	p_pixel_base_y = p_pixel_base + (i_ytmp * p_pic->p->i_pitch);
	i_x = 0;

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> 6 ) )
        {
            /* Draw until we reach the end of the line */
            for( i_x_src = 0; i_x_src < p_spu->i_width;  i_x_src++ )
            {
	      ogt_yuvt_t *p_source;

	      i_x += (1<<6);
	      
	      if( b_crop
		  && ( i_x < i_x_start || i_x > i_x_end
		       || i_y < i_y_start || i_y > i_y_end ) )
                {
		  continue;
                }

	      p_source = &p_src_start[i_y_src + i_x_src];

	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "Trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
	      switch( p_source->s.t )
                {
                case 0x00:
		  /* Completely transparent. Don't change pixel. */
		  break;
		  
                default:
                case MAX_ALPHA:
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */
		
		    uint16_t i_colprecomp = Y2RV16(p_source->plane[Y_PLANE]);

		    /* This is the location that's going to get changed.
		     */
		    uint8_t *p_dest = p_pixel_base_y + 2 * (i_x >> 6 );
#if 1
                    memset( p_dest, i_colprecomp, 2*i_xscale );
#else 
                    memset( p_dest, 0xFFFF, 2*i_xscale );
#endif
		    break;
		  }

#if FINISHED
                default:
		  {
		    /* Blend in underlying pixel subtitle pixel. */
		    
		    /* To be able to scale correctly for full opaqueness, we
		       add 1 to the alpha.  This means alpha value 0 won't
		       be completely transparent and is not correct, but
		       that's handled in a special case above anyway. */
		
		    uint8_t *p_pixel = p_pixel_base + 2*( i_x >> 6 );
		    uint16_t i_colprecomp = Y2RV16(p_source->plane[Y_PLANE])
		      * ( (uint16_t) (p_source->s.t+1) );
		    uint16_t i_destalpha = MAX_ALPHA - p_source->s.t;
		    *p_pixel++ = ( i_colprecomp + 
				     (uint16_t) (*p_pixel) * i_destalpha ) 
		      >> ALPHA_SCALEDOWN;
		    *p_pixel    = ( i_colprecomp + 
				     (uint16_t) (*p_pixel) * i_destalpha ) 
		      >> ALPHA_SCALEDOWN;
		    break;
		  }
#endif
                }
            }
        }
        else
        {
            i_ynext = p_pic->p->i_pitch * i_y >> 6;


            /* Draw until we reach the end of the line */
            for( i_x_src = 0; i_x_src < p_spu->i_width; i_x_src++ )
            {
	      ogt_yuvt_t *p_source;

	      i_x += (1<<6);
	      if( b_crop
		  && ( i_x < i_x_start || i_x > i_x_end
		       || i_y < i_y_start || i_y > i_y_end ) )
                {
		  continue;
                }
	      
	      p_source = &p_src_start[i_y_src + i_x_src];

	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "Trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
	      switch( p_source->s.t )
                {
                case 0x00:
		    /* Completely transparent. Don't change pixel. */
                    break;

                default:
                case MAX_ALPHA: 
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */
		    uint16_t i_colprecomp = Y2RV16(p_source->plane[Y_PLANE]);
		    uint8_t *p_pixel_base_x = p_pixel_base + 2 * ( i_x >> 6 );
		    
		    printf("++multiline\n");

                    for(  ; i_ytmp < i_ynext ; i_ytmp += p_pic->p->i_pitch )
                    {
		      /* This is the location that's going to get changed.  */
		      uint8_t *p_dest = p_pixel_base_x + i_ytmp;
#if 1
		      memset( p_dest, i_colprecomp, 2*i_xscale );
#else 
		      memset( p_dest, 0xFFFF, 2*i_xscale );
#endif
                    }
                    break;
		  }
#ifdef FINISHED
                default:
                    for(  ; i_ytmp < i_ynext ; y_ytmp += p_pic->p->i_pitch )
                    {
		      /* Blend in underlying pixel subtitle pixel. */
		      
		      /* To be able to scale correctly for full opaqueness, we
			 add 1 to the alpha.  This means alpha value 0 won't
			 be completely transparent and is not correct, but
			 that's handled in a special case above anyway. */
		      
		      uint8_t *p_pixel = p_pixel_base + i_ytmp;
		      uint16_t i_colprecomp = Y2RV16(p_source->plane[Y_PLANE])
			* ( (uint16_t) (p_source->s.t+1) );
		      uint16_t i_destalpha = MAX_ALPHA - p_source->s.t;

		      *p_pixel = ( i_colprecomp + 
				   (uint16_t) (*p_pixel) * i_destalpha ) 
			>> ALPHA_SCALEDOWN;
                    }
                    break;
#endif
		}
	    }
	}
    }
}

#define Y2RV32(val) ((uint32_t) (0x11111111 * ( (uint32_t)  (val) >> 4 )))

static void RenderRV32( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
  msg_Err( p_vout, "RV32 not implemented yet" );
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
