/*****************************************************************************
 * render.c : Philips OGT and CVD (VCD Subtitle) blending routines.
 *            stuff from here might be pulled out, abstracted or used
 *            by DVD subtitles.
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id: render.c,v 1.28 2004/02/01 05:15:22 rocky Exp $
 *
 * Author: Rocky Bernstein <rocky@panix.com>
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

/*#define TESTING_TRANSPARENCY 1*/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "pixmap.h"
#include "subtitle.h"
#include "render.h"

/* We use 4 bits for an transparency value: 0..15, 15 is completely
   transparent and 0 completely opaque. Note that although SVCD allow
   8-bits, for these routines pixels should previously have been be
   scaled down to 4 bits (the range used by DVDs).
*/
#define TRANS_BITS (4)
#define MAX_TRANS  ((1<<TRANS_BITS) - 1) 
#define TRANS_SCALEDOWN (8-TRANS_BITS)

/* We use a fixed-point arithmetic in scaling ratios so that we
   can use integer arithmetic and still get fairly precise
   results. ASCALE is a left shift amount. 
*/
#define ASCALE 6  /* 2^6 = 32 */

/* Horrible hack to get dbg_print to do the right thing */
#define p_dec p_vout

/**
   Take two 8-bit RGB values and a transparency and do a weighted
   average of them. The "weight" comes from i_trans which is in the
   range 0..MAX_TRANS. To have greater precision using integer
   arithmetic, the RGB values are scaled. The uint16_t cast below is
   to make sure we don't overflow the product in the multiplication

   (MAX_TRANS - i_trans) is the additive "inverse" of i_trans, i.e.
   i_trans + (MAX_TRANS - i_trans) = MAX_TRANS. So the resulting sum
   of rgb1*i_trans + rgb2*(MAX_TRANS-i_trans) will be scaled by
   MAX_TRANS. To reduce the value back down to 8 bits, we shift by
   TRANS_BITS, noting that 1 << TRANS_BITS is roughly
   MAX_TRANS. (Actually it is MAX_TRANS - 1).
*/
#define avg_8bit_rgb(rgb_vout, rgb_sub, i_trans)                   \
{                                                                  \
   int i;                                                          \
   for (i=0; i < RGB_SIZE; i++) {                                  \
   rgb_vout[i] = ( (uint16_t) rgb_vout[i]*(MAX_TRANS-i_trans) +    \
                   (uint16_t) rgb_sub [i]*i_trans ) >> TRANS_BITS; \
   }                                                               \
}


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void BlendI420( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );
static void BlendYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );
static void BlendRV16( vout_thread_t *p_vout, picture_t *p_pic,
                       const subpicture_t *p_spu, vlc_bool_t b_crop,
                       vlc_bool_t b_15bpp );
static void BlendRV24( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );
static void BlendRV32( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );
static void BlendRGB2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop );

/*****************************************************************************
 * BlendSPU: blend a subtitle into a picture
 *****************************************************************************
 
  This blends subtitles (a subpicture) into the underlying
  picture. Subtitle data has been preprocessed as YUV + transparancy
  or 4 bytes per pixel with interleaving of rows in the subtitle
  removed. 

 *****************************************************************************/
void VCDSubBlend( vout_thread_t *p_vout, picture_t *p_pic,
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
            BlendI420( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
            break;

        /* RGB 555 - scaled */
        case VLC_FOURCC('R','V','1','5'):
            BlendRV16( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop, 
                       VLC_TRUE );
            break;
          
        case VLC_FOURCC('R','V','1','6'):
            BlendRV16( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop,
                       /* Not sure under what conditions RV16 is really
                          RV16 and not RV15.
                        */
#if 0
                       VLC_FALSE );
#else
                       VLC_TRUE );
#endif
	    break;

        /* RV24 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
            BlendRV24( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
	    break;

        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','3','2'):
            BlendRV32( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
	    break;

        /* NVidia overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            BlendYUY2( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
	    break;

        /* Palettized 8 bits per pixel (256 colors). Each
           pixel is an uint8_t index in the palette
           Used in ASCII Art. 
        */
        case VLC_FOURCC('R','G','B','2'):
            BlendRGB2( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
          
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

static void BlendI420( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
  /* Common variables */
  uint8_t *p_pixel_base_Y, *p_pixel_base_V, *p_pixel_base_U;
  ogt_yuvt_t *p_source; /* This is the where the subtitle pixels come from */

  int i_x, i_y;
  vlc_bool_t even_scanline = VLC_FALSE;

  /* Crop-specific */
  int i_x_start, i_y_start, i_x_end, i_y_end;
  /* int i=0; */

  const struct subpicture_sys_t *p_sys = p_spu->p_sys;
  
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

      i_x = 0;

      if ( b_crop ) {
        if ( i_y > i_y_end ) break;
        if (i_x_start) {
          i_x = i_x_start;
          p_source += i_x_start;
        }
      }

      even_scanline = !even_scanline;

      /* Draw until we reach the end of the line */
      for( ; i_x < p_spu->i_width; i_x++, p_source++ )
	{

	  if( b_crop ) {

            /* FIXME: y cropping should be dealt with outside of this loop.*/
            if ( i_y < i_y_start) continue;

            if ( i_x > i_x_end )
	    {
	      p_source += p_spu->i_width - i_x;
              break;
	    }
          }

#ifdef TESTING_TRANSPARENCY
          if (p_source->s.t == MAX_TRANS) p_source->s.t >>= 1;
#endif

	  switch( p_source->s.t )
	    {
	    case 0: 
	      /* Completely transparent. Don't change pixel. */
	      break;
	      
	    case MAX_TRANS:
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
		/* Blend in underlying subtitle pixel. */
		
		/* This is the location that's going to get changed. */
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
			       (uint16_t) (MAX_TRANS - p_source->s.t) ) ;
		
		/* Scale the 12-bit result back down to 8 bits. A
		   precise scaling after adding the two components,
		   would divide by one less than a power of 2. However
		   to simplify and speed things we use a power of
		   2. This means the boundaries (either all
		   transparent and all opaque) aren't handled properly.
		   But we deal with them in special cases above. */

		*p_pixel_Y = ( i_sub_color_Y + i_pixel_color_Y ) >> TRANS_BITS;

		if ( even_scanline && i_x % 2 == 0 ) {
		  uint8_t *p_pixel_U = p_pixel_base_U_y + i_x/2
                    - p_pic->p[U_PLANE].i_pitch / 2;
		  uint8_t *p_pixel_V = p_pixel_base_V_y + i_x/2
                    - p_pic->p[V_PLANE].i_pitch / 2;
		  uint16_t i_sub_color_U = 
		    (uint16_t) ( p_source->plane[U_PLANE] *
				 (uint16_t) (p_source->s.t) );
		  
		  uint16_t i_sub_color_V = 
		    (uint16_t) ( p_source->plane[V_PLANE] *
				 (uint16_t) (p_source->s.t) );
		  uint16_t i_pixel_color_U = 
		    (uint16_t) ( *p_pixel_U * 
				 (uint16_t) (MAX_TRANS - p_source->s.t) ) ;
		  uint16_t i_pixel_color_V = 
		    (uint16_t) ( *p_pixel_V * 
				 (uint16_t) (MAX_TRANS - p_source->s.t) ) ;
		  *p_pixel_U = ( i_sub_color_U + i_pixel_color_U )>>TRANS_BITS;
		  *p_pixel_V = ( i_sub_color_V + i_pixel_color_V )>>TRANS_BITS;
		}
		break;
	      }
	      
	    }
	}
    }
}

/*

  YUY2 Format:

  Data is found in memory as an array of bytes in which the first byte
  contains the first sample of Y, the second byte contains the first
  sample of Cb (=U), the third byte contains the second sample of Y,
  the fourth byte contains the first sample of Cr (=V); and so
  on. Each 32-bit word then contains information for two contiguous
  horizontal pixels, two 8-bit Y values plus a single Cb and Cr which
  spans the two pixels.
*/

#define BYTES_PER_PIXEL 4

static void BlendYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
  /* Common variables */
  uint8_t *p_pixel_base;

  /* This is the where the subtitle pixels come from */
  ogt_yuvt_t *p_source = (ogt_yuvt_t *) p_spu->p_sys->p_data;;

  ogt_yuvt_t *p_source_end = (ogt_yuvt_t *)p_spu->p_sys->p_data + 
    (p_spu->i_width * p_spu->i_height);

  uint16_t i_x, i_y;

  /* Make sure we start on a word (4-byte) boundary. */
  uint16_t i_spu_x = (p_spu->i_x & 0xFFFE) * 2;

  /* Crop-specific */
  int i_x_start, i_y_start, i_x_end, i_y_end;

  const struct subpicture_sys_t *p_sys = p_spu->p_sys;
  
  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	     "spu width x height: (%dx%d), (x,y)=(%d,%d), pitch: %d", 
	     p_spu->i_width,  p_spu->i_height, p_spu->i_x, p_spu->i_y,
	     p_pic->p->i_pitch );

  
  p_pixel_base = p_pic->p->p_pixels + 
    + ( p_spu->i_y * p_pic->p->i_pitch ) + i_spu_x;

  i_x_start = p_sys->i_x_start;
  i_y_start = p_sys->i_y_start * p_pic->p->i_pitch;
  
  i_x_end   = p_sys->i_x_end;
  i_y_end   = p_sys->i_y_end   * p_pic->p->i_pitch;

  /* Draw until we reach the bottom of the subtitle */
  for( i_y = 0; 
       i_y < p_spu->i_height * p_pic->p[Y_PLANE].i_pitch ;
       i_y += p_pic->p[Y_PLANE].i_pitch )
    {
      uint8_t *p_pixel_base_y = p_pixel_base + i_y;

      i_x = 0;

      if ( b_crop ) {
        if ( i_y > i_y_end ) break;
        if (i_x_start) {
          i_x = i_x_start;
          p_source += (i_x_start*2);
        }
      }
  

      /* Draw until we reach the end of the line. Each output pixel
         is a combination of two source pixels. 
       */
      for( i_x = 0;  i_x < p_spu->i_width / 2; i_x++, p_source +=2 )
	{
          uint16_t i_avg_tr; /* transparancy sort of averaged over 2 pixels*/

          if (p_source > p_source_end-1) {
            msg_Err( p_vout, "Trying to access beyond subtitle x: %d y: %d",
                     i_x, i_y);
            return;
          }

	  if( b_crop ) {

            /* FIXME: y cropping should be dealt with outside of this loop.*/
            if ( i_y < i_y_start) continue;

            if ( i_x > i_x_end )
	    {
	      p_source += p_spu->i_width - (i_x*2);
              break;
	    }
          }
  
	  
          /* Favor opaque subtitle pixels. */
	  if ( (p_source->s.t == 0) && (p_source+1)->s.t == MAX_TRANS )
            i_avg_tr = (p_source+1)->s.t;
          else if ( (p_source->s.t == MAX_TRANS) && (p_source+1)->s.t == 0 )
            i_avg_tr = p_source->s.t;
          else 
            i_avg_tr = ( p_source->s.t + (p_source+1)->s.t ) / 2;
          
#ifdef TESTING_TRANSPARENCY 
          if (i_avg_tr == MAX_TRANS) i_avg_tr >>= 1;
#endif

	  switch( i_avg_tr )
	    {
	    case 0: 
	      /* Completely transparent. Don't change pixel. */
	      break;
	      
	    case MAX_TRANS:
	      {
		/* Completely opaque. Completely overwrite underlying
		   pixel with subtitle pixel. */
		
		/* This is the location that's going to get changed. */
		uint8_t *p_pixel = p_pixel_base_y + i_x * BYTES_PER_PIXEL;
		uint8_t i_avg_u;
		uint8_t i_avg_v;

                /* Favor opaque subtitle pixel. */
                if (p_source->s.t == MAX_TRANS ) {
                  i_avg_u = p_source->plane[U_PLANE] ;
                  i_avg_v = p_source->plane[V_PLANE] ;
                } else if ( (p_source+1)->s.t == MAX_TRANS ) {
                  i_avg_u = (p_source+1)->plane[U_PLANE] ;
                  i_avg_v = (p_source+1)->plane[V_PLANE] ;
                } else {
                  i_avg_u = ( p_source->plane[U_PLANE] 
                              + (p_source+1)->plane[U_PLANE] ) / 2;
                  i_avg_v = ( p_source->plane[V_PLANE] 
                              + (p_source+1)->plane[V_PLANE] ) / 2;
                }

		/* draw a two contiguous pixels: 2 Y values, 1 U, and 1 V. */
		*p_pixel++ = p_source->plane[Y_PLANE] ;
                *p_pixel++ = i_avg_u;
		*p_pixel++ = (p_source+1)->plane[Y_PLANE] ;
                *p_pixel++ = i_avg_v;
                
		break;
	      }

	    default:
	      {
		/* Blend in underlying subtitle pixels. */
		
		/* This is the location that's going to get changed. */
		uint8_t *p_pixel = p_pixel_base_y + i_x * BYTES_PER_PIXEL;
		uint8_t i_avg_u = ( p_source->plane[U_PLANE] 
                                    + (p_source+1)->plane[U_PLANE] ) / 2;
		uint8_t i_avg_v = ( p_source->plane[V_PLANE] 
                                    + (p_source+1)->plane[V_PLANE] ) / 2;

		/* This is the weighted part of the two subtitle
		   pixels. The color plane is 8 bits and transparancy
		   is 4 bits so when multiplied we get up to 12 bits.
		 */
		uint16_t i_sub_color_Y1 = 
		  (uint16_t) ( p_source->plane[Y_PLANE] *
			       (uint16_t) (p_source->s.t) );

		uint16_t i_sub_color_Y2 = 
		  (uint16_t) ( (p_source+1)->plane[Y_PLANE] *
			       (uint16_t) ((p_source+1)->s.t) );

		/* This is the weighted part of the underlying pixels.
		   For the same reasons above, the result is up to 12
		   bits.  However since the transparancies are
		   inverses, the sum of i_sub_color and i_pixel_color
		   will not exceed 12 bits.
		*/
		uint16_t i_sub_color_U = 
		  (uint16_t) ( i_avg_u * (uint16_t) i_avg_tr );
		
		uint16_t i_sub_color_V = 
		  (uint16_t) ( i_avg_v * (uint16_t) i_avg_tr );

		uint16_t i_pixel_color_Y1 = 
		  (uint16_t) ( *(p_pixel) * 
			       (uint16_t) (MAX_TRANS - i_avg_tr) ) ;
		uint16_t i_pixel_color_Y2 = 
		  (uint16_t) ( *(p_pixel+2) * 
			       (uint16_t) (MAX_TRANS - i_avg_tr) ) ;
		uint16_t i_pixel_color_U = 
		  (uint16_t) ( *(p_pixel+1) * 
			       (uint16_t) (MAX_TRANS - i_avg_tr) ) ;
		uint16_t i_pixel_color_V = 
		  (uint16_t) ( *(p_pixel+3) * 
			       (uint16_t) (MAX_TRANS - i_avg_tr) ) ;

		/* draw a two contiguous pixels: 2 Y values, 1 U, and 1 V. */

		/* Scale the 12-bit result back down to 8 bits. A
		   precise scaling after adding the two components,
		   would divide by one less than a power of 2. However
		   to simplify and speed things we use a power of
		   2. This means the boundaries (either all
		   transparent and all opaque) aren't handled properly.
		   But we deal with them in special cases above. */

		*p_pixel++ = ( i_sub_color_Y1 + i_pixel_color_Y1 )>>TRANS_BITS;
		*p_pixel++ = ( i_sub_color_U + i_pixel_color_U )  >>TRANS_BITS;
		*p_pixel++ = ( i_sub_color_Y2 + i_pixel_color_Y2 )>>TRANS_BITS;
		*p_pixel++ = ( i_sub_color_V + i_pixel_color_V )  >>TRANS_BITS;
		break;
	      }
	    }
	}

      /* For an odd width source, we'll just have to drop off a pixel. */
      if (p_spu->i_width % 2) p_source++;
    }
}

/**
   Convert a YUV pixel into a 16-bit RGB 5-5-5 pixel.

   A RGB 5-5-5 pixel looks like this:
   RGB 5-5-5   bit  (MSB) 7  6   5  4  3  2  1  0 (LSB)
                 p 	  ? B4 	B3 B2 B1 B0 R4 	R3
                 q 	 R2 R1  R0 G4 G3 G2 G1 	G0

**/

static inline void
yuv2rgb555(ogt_yuvt_t *p_yuv, uint8_t *p_rgb1, uint8_t *p_rgb2 )
{

  uint8_t rgb[RGB_SIZE];

  yuv2rgb(p_yuv, rgb);
  
  /* Scale RGB from 8 bits down to 5. */
  rgb[RED_PIXEL]   >>= (8-5);
  rgb[GREEN_PIXEL] >>= (8-5);
  rgb[BLUE_PIXEL]  >>= (8-5);
  
  *p_rgb1 = ( (rgb[BLUE_PIXEL] << 2)&0x7c ) | ( (rgb[RED_PIXEL]>>3) & 0x03 );
  *p_rgb2 = ( (rgb[RED_PIXEL]  << 5)&0xe0 ) | ( rgb[GREEN_PIXEL]&0x1f );

#if 0
  printf("Y,Cb,Cr,T=(%02x,%02x,%02x,%02x), r,g,b=(%d,%d,%d), "
         "rgb1: %02x, rgb2 %02x\n",
         p_yuv->s.y, p_yuv->s.u, p_yuv->s.v, p_yuv->s.t,
         rgb[RED_PIXEL], rgb[GREEN_PIXEL], rgb[BLUE_PIXEL],
         *p_rgb1, *p_rgb2);
#endif

}

/**
   Convert a YUV pixel into a 16-bit RGB 5-6-5 pixel.

   A RGB 5-6-5 pixel looks like this:
   RGB 5-6-5   bit  (MSB) 7  6   5  4  3  2  1  0 (LSB)
                 p 	 B4 B3 	B2 B1 B0 R5 R4 	R3
                 q 	 R2 R1  R0 G4 G3 G2 G1 	G0

**/

static inline void
yuv2rgb565(ogt_yuvt_t *p_yuv, uint8_t *p_rgb1, uint8_t *p_rgb2 )
{

  uint8_t rgb[RGB_SIZE];

  yuv2rgb(p_yuv, rgb);
  
  /* Scale RGB from 8 bits down to 5 or 6 bits. */
  rgb[RED_PIXEL]   >>= (8-6);
  rgb[GREEN_PIXEL] >>= (8-5);
  rgb[BLUE_PIXEL]  >>= (8-5);
  
  *p_rgb1 = ( (rgb[BLUE_PIXEL] << 3)&0xF8 ) | ( (rgb[RED_PIXEL]>>3) & 0x07 );
  *p_rgb2 = ( (rgb[RED_PIXEL]  << 5)&0xe0 ) | ( rgb[GREEN_PIXEL]&0x1f );

#if 0
  printf("Y,Cb,Cr,T=(%02x,%02x,%02x,%02x), r,g,b=(%d,%d,%d), "
         "rgb1: %02x, rgb2 %02x\n",
         p_yuv->s.y, p_yuv->s.u, p_yuv->s.v, p_yuv->s.t,
         rgb[RED_PIXEL], rgb[GREEN_PIXEL], rgb[BLUE_PIXEL],
         *p_rgb1, *p_rgb2);
#endif

}

static inline void
rv16_pack_blend(uint8_t *p_pixel, ogt_yuvt_t *p_source,  uint8_t *p_rgb1,
                uint8_t *p_rgb2, vlc_bool_t b_15bpp, uint8_t i_trans, 
                int a_scale_down )
{
  uint8_t rgb_sub[3];
  uint8_t rgb_vout[RGB_SIZE];
  int i;

  yuv2rgb(p_source, rgb_sub);

  /* Scale RGB from 8 bits down to 6 or 5. */
  rgb_sub[GREEN_PIXEL] >>= (8-5);
  rgb_sub[BLUE_PIXEL]  >>= (8-5);

  rgb_vout[GREEN_PIXEL] = *(p_pixel+1)    & 0x1f;
  if (b_15bpp) {
    rgb_sub[RED_PIXEL]   >>= (8-5);
    rgb_vout[BLUE_PIXEL]  = ((*p_pixel)>>2) & 0x1f;
    rgb_vout[RED_PIXEL]   = ((*p_pixel & 0x03) << 3) | ((*(p_pixel+1)&0xe0)>>5);
  } else {
    rgb_sub[RED_PIXEL]   >>= (8-6);
    rgb_vout[BLUE_PIXEL]  = ((*p_pixel)>>3) & 0x1f;
    rgb_vout[RED_PIXEL]   = ((*p_pixel & 0x07) << 3) | ((*(p_pixel+1)&0xe0)>>5);
  }
  

#if 0
  printf("r,g,b=(%d,%d,%d), sub r,g,b=(%d,%d,%d), trans %d, inv_trans %d\n",
         rgb_vout[RED_PIXEL], rgb_vout[GREEN_PIXEL], rgb_vout[BLUE_PIXEL],
         rgb_sub[RED_PIXEL], rgb_sub[GREEN_PIXEL], 
         rgb_sub[BLUE_PIXEL], i_trans, i_inv_trans);
#endif

#ifdef FIXED_RV16_TRANSPARENCY
  avg_8bit_rgb(rgb_vout, rgb_sub, i_trans);
#else 
  for (i=0; i < RGB_SIZE; i++) {
    /* For now the Best we can do is fade the color. Picking up 
       video underneath doesn't work. */
    /* rgb_vout[i] = ( (uint16_t) rgb_vout[i]*i_inv_trans ) >> TRANS_BITS; */
    rgb_vout[i] = ( (uint16_t) rgb_sub[i]*i_trans ) >> TRANS_BITS;
#endif 
  }
  

#if 0
  printf("avg r,g,b=(%d,%d,%d)\n",
         rgb_vout[RED_PIXEL], rgb_vout[GREEN_PIXEL], rgb_vout[BLUE_PIXEL] );
#endif

#if 0
  if (b_15bpp) {
    *p_rgb1 = ( (rgb_vout[BLUE_PIXEL] << 2)&0x7c )|( (rgb_vout[RED_PIXEL]>>3)&0x03 );
  } else {
    *p_rgb1 = ( (rgb_vout[BLUE_PIXEL] << 3)&0xF8 )|( (rgb_vout[RED_PIXEL]>>3)&0x07 );
  }
  *p_rgb2 = ( (rgb_vout[RED_PIXEL]  << 5)&0xe0 ) | ( rgb_vout[GREEN_PIXEL]&0x1f );
#else 
  *p_rgb1 = (*p_rgb1)+1;
  *p_rgb2 = (*p_rgb2)+1;
#endif
  
}

#undef BYTES_PER_PIXEL
#define BYTES_PER_PIXEL 2

static void 
BlendRV16( vout_thread_t *p_vout, picture_t *p_pic,
           const subpicture_t *p_spu, vlc_bool_t b_crop,
           vlc_bool_t b_15bpp )
{
    /* Common variables */
    uint8_t *p_pixel_base;
    ogt_yuvt_t *p_src_start = (ogt_yuvt_t *)p_spu->p_sys->p_data;
    ogt_yuvt_t *p_src_end   = &p_src_start[p_spu->i_height * p_spu->i_width];
    ogt_yuvt_t *p_source;

    int i_x, i_y;
    int i_y_src;

    /* Chroma specific */
    uint32_t i_xscale;   /* Amount we scale subtitle in the x direction,
                            multiplied by 2**ASCALE. */
    uint32_t i_yscale;   /* Amount we scale subtitle in the y direction.
                            multiplied by 2**ASCALE. */

    int i_width, i_height, i_ytmp, i_ynext;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    struct subpicture_sys_t *p_sys = p_spu->p_sys;

    i_xscale = ( p_vout->output.i_width << ASCALE ) / p_vout->render.i_width;
    i_yscale = ( p_vout->output.i_height << ASCALE ) / p_vout->render.i_height;

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
    p_pixel_base = p_pic->p->p_pixels 
              + ( (p_spu->i_x * i_xscale) >> ASCALE ) * BYTES_PER_PIXEL
              + ( (p_spu->i_y * i_yscale) >> ASCALE ) * p_pic->p->i_pitch;

    i_x_start = p_sys->i_x_start;
    i_y_start = i_yscale * p_sys->i_y_start;
    i_x_end   = p_sys->i_x_end;
    i_y_end   = i_yscale * p_sys->i_y_end;

    p_source = (ogt_yuvt_t *)p_sys->p_data;
  
    /* Draw until we reach the bottom of the subtitle */
    i_y = 0;
    for( i_y_src = 0 ; i_y_src < p_spu->i_height * p_spu->i_width; 
         i_y_src += p_spu->i_width )
    {
	uint8_t *p_pixel_base_y;
        i_ytmp = i_y >> ASCALE;
        i_y += i_yscale;
	p_pixel_base_y = p_pixel_base + (i_ytmp * p_pic->p->i_pitch);
	i_x = 0;

        if ( b_crop ) {
          if ( i_y > i_y_end ) break;
          if (i_x_start) {
            i_x = i_x_start;
            p_source += i_x_start;
          }
        }

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> ASCALE ) )
        {
          /* Draw until we reach the end of the line */
          for( ; i_x < p_spu->i_width;  i_x++, p_source++ )
            {

#if 0              
              uint8_t *p=(uint8_t *) p_source;
              printf("+++ %02x %02x %02x %02x\n", 
                     p[0], p[1], p[2], p[3]);
#endif
    
              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }

	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "Trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
#ifdef TESTING_TRANSPARENCY
              if (p_source->s.t == MAX_TRANS) p_source->s.t >>= 1;
#endif

	      switch( p_source->s.t )
                {
                case 0:
		  /* Completely transparent. Don't change pixel. */
		  break;
		  
                case MAX_TRANS:
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */
		
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;

                    uint8_t i_rgb1;
                    uint8_t i_rgb2;

		    /* This is the location that's going to get changed. */
		    uint8_t *p_dest = p_pixel_base_y + i_x * BYTES_PER_PIXEL;

                    if (b_15bpp) 
                      yuv2rgb555(p_source, &i_rgb1, &i_rgb2);
                    else 
                      yuv2rgb565(p_source, &i_rgb1, &i_rgb2);

                    for ( len = i_xlast - i_xdest; len ; len--) {
                      *p_dest++ = i_rgb1;
                      *p_dest++ = i_rgb2;
                    }
		    break;
		  }

                default:
		  {
		    /* Blend in underlying pixel subtitle pixel. */
		    
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint8_t i_rgb1;
                    uint8_t i_rgb2;
                    uint32_t len     = i_xlast - i_xdest;

		    /* This is the location that's going to get changed. */
		    uint8_t *p_dest = p_pixel_base_y + i_x * BYTES_PER_PIXEL;

                    for ( len = i_xlast - i_xdest; len ; len--) {
                      rv16_pack_blend(p_dest, p_source, &i_rgb1, &i_rgb2, 
                                      b_15bpp, p_source->s.t, TRANS_SCALEDOWN);
                      *p_dest++ = i_rgb1;
                      *p_dest++ = i_rgb2;
                    }
		    break;
		  }
                }
            }
        }
        else
        {
            i_ynext = p_pic->p->i_pitch * i_y >> ASCALE;


            /* Draw until we reach the end of the line */
            for( ; i_x < p_spu->i_width; i_x++, p_source++ )
            {

              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }
	      
	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
	      switch( p_source->s.t )
                {
                case 0:
		    /* Completely transparent. Don't change pixel. */
                    break;

                case MAX_TRANS: 
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */

                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;

		    uint8_t *p_pixel_base_x = p_pixel_base + i_xdest;

                    for(  ; i_ytmp < i_ynext ; i_ytmp += p_pic->p->i_pitch )
                    {
		      /* This is the location that's going to get changed. */
		      uint8_t *p_dest = p_pixel_base_x + i_ytmp;
                      uint8_t i_rgb1, i_rgb2;
                      if (b_15bpp) 
                        yuv2rgb555(p_source, &i_rgb1, &i_rgb2);
                      else 
                        yuv2rgb565(p_source, &i_rgb1, &i_rgb2);

                      for ( len = i_xlast - i_xdest; len ; len--) {
                        *p_dest++ = i_rgb1;
                        *p_dest++ = i_rgb2;
                      }
                    }
                    break;
		  }
                default: 
                  {
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;
                    uint8_t i_rgb1, i_rgb2;
                    for(  ; i_ytmp < i_ynext ; i_ytmp += p_pic->p->i_pitch )
                    {
		      /* Blend in underlying pixel subtitle pixel. */
		      uint8_t *p_dest = p_pixel_base + i_ytmp;
                      for ( len = i_xlast - i_xdest; len ; len--) {
                        rv16_pack_blend(p_dest, p_source, &i_rgb1, &i_rgb2,
                                      b_15bpp, p_source->s.t, TRANS_SCALEDOWN);
                        *p_dest++ = i_rgb1;
                        *p_dest++ = i_rgb2;
                      }
                    }
                    break;
                  }
		}
	    }
	}
    }
}

#undef  BYTES_PER_PIXEL
#define BYTES_PER_PIXEL 4

static inline void
rv24_pack_blend(uint8_t *rgb_vout, const uint8_t *rgb_sub,  uint8_t i_trans,  
                int a_scale_down )
{

#if 0
  printf("r,g,b=(%d,%d,%d), source r,g,b=(%d,%d,%d), trans %d\n",
         rgb_vout[RED_PIXEL], rgb_vout[GREEN_PIXEL], rgb_vout[BLUE_PIXEL],
         rgb_sub[RED_PIXEL], rgb_sub[GREEN_PIXEL], 
         rgb_sub[BLUE_PIXEL], i_trans);
#endif

#ifdef WORDS_BIGENDIAN
  *rgb_vout++;
#endif

  avg_8bit_rgb(rgb_vout, rgb_sub, i_trans);
  
#if 0
  printf("avg r,g,b=(%d,%d,%d)\n",
         rgb_vout[RED_PIXEL], rgb_vout[GREEN_PIXEL], rgb_vout[BLUE_PIXEL] );
#endif

}

/* 
  RV24 format??? Is this just for X11? Or just not for Win32? Is this
  the same as RV32?

  a pixel is represented by 3 bytes containing a red,
  blue and green sample with blue stored at the lowest address, green
  next then red. One padding byte is added between pixels. Although
  this may not be part of a spec, images should be stored with each
  line padded to a u_int32 boundary. 
*/
static void 
BlendRV24( vout_thread_t *p_vout, picture_t *p_pic,
            const subpicture_t *p_spu, vlc_bool_t b_crop )
{
  /* Common variables */
  uint8_t *p_pixel_base;
  ogt_yuvt_t *p_src_start = (ogt_yuvt_t *)p_spu->p_sys->p_data;
  ogt_yuvt_t *p_src_end   = &p_src_start[p_spu->i_height * p_spu->i_width];
  ogt_yuvt_t *p_source; /* This is the where the subtitle pixels come from */
  
  int i_x, i_y;
  int i_y_src;
  
  /* Make sure we start on a word (4-byte) boundary. */
  uint32_t i_spu_x;
  
  /* Chroma specific */
  uint32_t i_xscale;   /* Amount we scale subtitle in the x direction,
                          multiplied by 2**ASCALE. */
  uint32_t i_yscale;   /* Amount we scale subtitle in the y direction.
                          multiplied by 2**ASCALE. */
  
  int i_width, i_height, i_ytmp, i_ynext;
  
  /* Crop-specific */
  int32_t i_x_start, i_y_start, i_x_end, i_y_end;
  
  struct subpicture_sys_t *p_sys = p_spu->p_sys;
  unsigned int i_aspect_x, i_aspect_y;
  
  vout_AspectRatio( p_vout->render.i_aspect, &i_aspect_y, 
                    &i_aspect_x );
  
  i_xscale = (( p_vout->output.i_width << ASCALE ) * i_aspect_x)
    / (i_aspect_y * p_vout->render.i_width);
  i_yscale = ( p_vout->output.i_height << ASCALE ) / p_vout->render.i_height;
  
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
     the picture coordinates subtitle offsets.
  */
  i_spu_x = ((p_spu->i_x * i_xscale) >> ASCALE) * BYTES_PER_PIXEL;
  
  p_pixel_base = p_pic->p->p_pixels + i_spu_x
    + ( (p_spu->i_y * i_yscale) >> ASCALE ) * p_pic->p->i_pitch;
  
  i_x_start = p_sys->i_x_start;
  i_y_start = i_yscale * p_sys->i_y_start;
  i_x_end   = p_sys->i_x_end;
  i_y_end   = i_yscale * p_sys->i_y_end;
  
  p_source = (ogt_yuvt_t *)p_sys->p_data;
  
  /* Draw until we reach the bottom of the subtitle */
  i_y = 0;
  for( i_y_src = 0 ; i_y_src < p_spu->i_height * p_spu->i_width; 
       i_y_src += p_spu->i_width )
    {
      uint8_t *p_pixel_base_y;
      i_ytmp = i_y >> ASCALE;
      i_y += i_yscale;
      p_pixel_base_y = p_pixel_base + (i_ytmp * p_pic->p->i_pitch);
      i_x = 0;
      
      if ( b_crop ) {
        if ( i_y > i_y_end ) break;
        if (i_x_start) {
          i_x = i_x_start;
          p_source += i_x_start;
        }
      }
      
      /* Check whether we need to draw one line or more than one */
      if( i_ytmp + 1 >= ( i_y >> ASCALE ) )
        {
          /* Draw until we reach the end of the line */
          for( ; i_x < p_spu->i_width;  i_x++, p_source++ )
            {
              
              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }
              
	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
#ifdef TESTING_TRANSPARENCY
              if (p_source->s.t == MAX_TRANS) p_source->s.t >>= 2;
#endif

	      switch( p_source->s.t )
                {
                case 0:
		  /* Completely transparent. Don't change pixel. */
		  break;
		  
                case MAX_TRANS:
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */
                    
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;
                    
                    uint8_t rgb[RGB_SIZE];
                    
		    /* This is the location that's going to get changed. */
		    uint8_t *p_dest = p_pixel_base_y + i_xdest;
                    
                    yuv2rgb(p_source, rgb);
                    
                    for ( len = i_xlast - i_xdest; len ; len--) {
                      put_rgb24_pixel(rgb, p_dest);
                      p_dest += BYTES_PER_PIXEL;
                    }
                    
                  default:
                    {
                      /* Blend in underlying pixel subtitle pixel. */
                      
                      uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                           * BYTES_PER_PIXEL );
                      uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                           * BYTES_PER_PIXEL );
                      uint32_t len     = i_xlast - i_xdest * BYTES_PER_PIXEL;
                      
                      /* To be able to scale correctly for full
                         opaqueness, we add 1 to the transparency.
                         This means transparency value 0 won't be
                         completely transparent which is not correct.
                         But that's handled in a special case above
                         anyway. */
                      
                      /* This is the location that's going to get changed. */
                      uint8_t *p_dest = p_pixel_base_y + i_xdest;
                      uint8_t rgb[RGB_SIZE];
                      
                      yuv2rgb(p_source, rgb);

                      for ( len = i_xlast - i_xdest; len ; len--) {
                        rv24_pack_blend(p_dest, rgb, p_source->s.t,
                                        TRANS_SCALEDOWN);
                        p_dest += BYTES_PER_PIXEL;
                      }
                      break;
                    }
                  }
                }
            }
        } 
        else
        {
            i_ynext = p_pic->p->i_pitch * i_y >> ASCALE;


            /* Draw until we reach the end of the line */
            for( ; i_x < p_spu->i_width; i_x++, p_source++ )
            {

              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }
	      
	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
	      switch( p_source->s.t )
                {
                case 0:
		    /* Completely transparent. Don't change pixel. */
                    break;

                case MAX_TRANS: 
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */

		    /* This is the location that's going to get changed. */
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;

                    uint8_t rgb[RGB_SIZE];

                    yuv2rgb(p_source, rgb); 

                    for(  ; i_ytmp < i_ynext ; i_ytmp += p_pic->p->i_pitch )
                    {
                      /* Completely opaque. Completely overwrite underlying
                         pixel with subtitle pixel. */
                      
                      /* This is the location that's going to get changed. */
                      uint8_t *p_dest = p_pixel_base + i_ytmp + i_xdest;
                      
                      for ( len = i_xlast - i_xdest; len ; len--) {
                        put_rgb24_pixel(rgb, p_dest);
                        p_dest += BYTES_PER_PIXEL;
                      }
                    }
                    break;
		  }
                default: 
                  {
                    

                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;
                    uint8_t  rgb[RGB_SIZE];

                    yuv2rgb(p_source, rgb);

                    for(  ; i_ytmp < i_ynext ; i_ytmp += p_pic->p->i_pitch )
                    {
		      /* Blend in underlying pixel subtitle pixel. */
		      
                      /* This is the location that's going to get changed. */
                      uint8_t *p_dest = p_pixel_base + i_ytmp + i_xdest;

		      /* To be able to scale correctly for full opaqueness, we
			 add 1 to the alpha.  This means alpha value 0 won't
			 be completely transparent and is not correct, but
			 that's handled in a special case above anyway. */

                      for ( len = i_xlast - i_xdest; len ; len--) {
                        rv24_pack_blend(p_dest, rgb, p_source->s.t,
                                        TRANS_SCALEDOWN);
                        p_dest += BYTES_PER_PIXEL;
                      }
                    }
                    break;
                  }
                }
            }
	}
    }
}

#undef  BYTES_PER_PIXEL
#define BYTES_PER_PIXEL 4

/* 
  RV32 format??? Is this just for X11? Or just not for Win32? Is this
  the same as RV24?

  RV32 format: a pixel is represented by 4 bytes containing a red,
  blue and green sample with blue stored at the lowest address, green
  next then red. One padding byte is added between pixels. Although
  this may not be part of a spec, images should be stored with each
  line padded to a u_int32 boundary. 
*/
static void 
BlendRV32( vout_thread_t *p_vout, picture_t *p_pic,
            const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    uint8_t *p_pixel_base;
    ogt_yuvt_t *p_src_start = (ogt_yuvt_t *)p_spu->p_sys->p_data;
    ogt_yuvt_t *p_src_end   = &p_src_start[p_spu->i_height * p_spu->i_width];
    ogt_yuvt_t *p_source; /* This is the where the subtitle pixels come from */

    int i_x, i_y;
    int i_y_src;

    /* Make sure we start on a word (4-byte) boundary. */
    uint32_t i_spu_x;

    /* Chroma specific */
    uint32_t i_xscale;   /* Amount we scale subtitle in the x direction,
                            multiplied by 2**ASCALE. */
    uint32_t i_yscale;   /* Amount we scale subtitle in the y direction.
                            multiplied by 2**ASCALE. */

    int i_width, i_height, i_ytmp, i_ynext;

    /* Crop-specific */
    int32_t i_x_start, i_y_start, i_x_end, i_y_end;

    struct subpicture_sys_t *p_sys = p_spu->p_sys;
    unsigned int i_aspect_x, i_aspect_y;

    vout_AspectRatio( p_vout->render.i_aspect, &i_aspect_y, 
                      &i_aspect_x );

    i_xscale = (( p_vout->output.i_width << ASCALE ) * i_aspect_x)
      / (i_aspect_y * p_vout->render.i_width);
    i_yscale = ( p_vout->output.i_height << ASCALE ) / p_vout->render.i_height;

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
       the picture coordinates subtitle offsets.
    */
    i_spu_x = ((p_spu->i_x * i_xscale) >> ASCALE) * BYTES_PER_PIXEL; 

    p_pixel_base = p_pic->p->p_pixels + i_spu_x
              + ( (p_spu->i_y * i_yscale) >> ASCALE ) * p_pic->p->i_pitch;

    i_x_start = p_sys->i_x_start;
    i_y_start = i_yscale * p_sys->i_y_start;
    i_x_end   = p_sys->i_x_end;
    i_y_end   = i_yscale * p_sys->i_y_end;

    p_source = (ogt_yuvt_t *)p_sys->p_data;
  
    /* Draw until we reach the bottom of the subtitle */
    i_y = 0;
    for( i_y_src = 0 ; i_y_src < p_spu->i_height * p_spu->i_width; 
         i_y_src += p_spu->i_width )
    {
	uint8_t *p_pixel_base_y;
        i_ytmp = i_y >> ASCALE;
        i_y += i_yscale;
	p_pixel_base_y = p_pixel_base + (i_ytmp * p_pic->p->i_pitch);
	i_x = 0;

        if ( b_crop ) {
          if ( i_y > i_y_end ) break;
          if (i_x_start) {
            i_x = i_x_start;
            p_source += i_x_start;
          }
        }

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> ASCALE ) )
        {
          /* Draw until we reach the end of the line */
          for( ; i_x < p_spu->i_width;  i_x++, p_source++ )
            {

              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }

	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
	      switch( p_source->s.t )
                {
                case 0:
		  /* Completely transparent. Don't change pixel. */
		  break;
		  
                default:
                case MAX_TRANS:
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */

                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;

                    uint8_t rgb[RGB_SIZE];

		    /* This is the location that's going to get changed. */
		    uint8_t *p_dest = p_pixel_base_y + i_xdest;

                    yuv2rgb(p_source, rgb);

                    for ( len = i_xlast - i_xdest; len ; len--) {
                      *p_dest++ = rgb[BLUE_PIXEL];
                      *p_dest++ = rgb[GREEN_PIXEL];
                      *p_dest++ = rgb[RED_PIXEL];
                      *p_dest++;
                    }

#ifdef TRANSPARENCY_FINISHED
                  default:
                    {
                      /* Blend in underlying pixel subtitle pixel. */
                      
                      uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                           * BYTES_PER_PIXEL );
                      uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                           * BYTES_PER_PIXEL );
                      uint32_t len     = i_xlast - i_xdest;

                      /* To be able to scale correctly for full opaqueness, we
                         add 1 to the alpha.  This means alpha value 0 won't
                         be completely transparent and is not correct, but
                         that's handled in a special case above anyway. */
                      
                      uint8_t i_destalpha = MAX_TRANS - p_source->s.t;
                      uint8_t rgb[RGB_SIZE];

                      /* This is the location that's going to get changed. */
                      uint8_t *p_dest = p_pixel_base_y + i_xdest;
                      
                      yuv2rgb(p_source, rgb);
                      rv32_pack_blend(p_dest, rgb, dest_alpha, 
                                      TRANS_SCALEDOWN);

                      for ( len = i_xlast - i_xdest; len ; len--) {
                        *p_dest++ = rgb[BLUE_PIXEL];
                        *p_dest++ = rgb[GREEN_PIXEL];
                        *p_dest++ = rgb[RED_PIXEL];
                        *p_dest++;
                      }
                      break;
                    }
#endif /*TRANSPARENCY_FINISHED*/
                  }
                }
            }
        } 
        else
        {
            i_ynext = p_pic->p->i_pitch * i_y >> ASCALE;


            /* Draw until we reach the end of the line */
            for( ; i_x < p_spu->i_width; i_x++, p_source++ )
            {

              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }
	      
	      if (p_source >= p_src_end) {
		msg_Err( p_vout, "Trying to access beyond subtitle %dx%d %d",
			 i_x, i_y / i_yscale, i_height);
		return;
	      }
	      
	      switch( p_source->s.t )
                {
                case 0:
		    /* Completely transparent. Don't change pixel. */
                    break;

                default:
                case MAX_TRANS: 
		  {
		    /* Completely opaque. Completely overwrite underlying
		       pixel with subtitle pixel. */

		    /* This is the location that's going to get changed. */
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;

                    uint8_t rgb[RGB_SIZE];

                    yuv2rgb(p_source, rgb); 

                    for(  ; i_ytmp < i_ynext ; i_ytmp += p_pic->p->i_pitch )
                    {
                      /* Completely opaque. Completely overwrite underlying
                         pixel with subtitle pixel. */
                      
                      /* This is the location that's going to get changed. */
                      uint8_t *p_dest = p_pixel_base + i_ytmp + i_xdest;
                      
                      for ( len = i_xlast - i_xdest; len ; len--) {
                        put_rgb24_pixel(rgb, p_dest);
                      }
                    }
                    break;
		  }
#ifdef TRANSPARENCY_FINISHED
                default: 
                  {
                    

                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;
                    uint8_t rgb[RGB_SIZE];

                    yuv2rgb(p_source, rgb);

                    for(  ; i_ytmp < i_ynext ; y_ytmp += p_pic->p->i_pitch )
                    {
		      /* Blend in underlying pixel subtitle pixel. */
		      
                      /* This is the location that's going to get changed. */
                      uint8_t *p_dest = p_pixel_base + i_ytmp + i_xdest;

		      /* To be able to scale correctly for full opaqueness, we
			 add 1 to the alpha.  This means alpha value 0 won't
			 be completely transparent and is not correct, but
			 that's handled in a special case above anyway. */

                      uint8_t i_destalpha = MAX_TRANS - p_source->s.t;
                      rv32_pack_blend(p_dest, rgb, dest_alpha,
                                      TRANS_SCALEDOWN);
                    }
                    break;
#endif /*TRANSPARENCY_FINISHED*/
		}
	    }
	}
    }
}

/* 4-entry array of colormap indices */
static  uint8_t cmap[NUM_SUBTITLE_COLORS];

/* Actual RGB values for above; this is used in blending.*/
static uint8_t cmap_rgb[NUM_SUBTITLE_COLORS][RGB_SIZE];

/*
  Return the colormap index for the average of i_pixel and a subtitle
  pixel whose subtitle palette entry is i_cmap. 
 */
static inline cmap_t
avg_rgb2(const vout_thread_t *p_vout, uint8_t i_pixel, uint8_t i_trans, 
         int i_cmap, mtime_t i_pts)
{
  uint8_t rgb_vout[RGB_SIZE];

  /* Cache the average of a vout colormap entry and a subtitle palette
     entry. There are not too many of these 256*4 = 1024.
   */
  static cmap_t avg_cache[CMAP_RGB2_SIZE][NUM_SUBTITLE_COLORS];

  /* subtitle palettes might change between two subtitles. i_last_pts
     will be used to figure out if the subtitle has changed, and
     we have to invalidate the cache. */
  static mtime_t i_last_pts = -1;

  if (i_pts != i_last_pts) 
    {
      /* Hack: We rely on the fact that INVALID_CMAP_ENTRY is repeated
         0xFF.
       */
      memset(avg_cache, 0xFF, sizeof(avg_cache));
      i_last_pts = i_pts;
    }

  if ( avg_cache[i_pixel][i_cmap] != INVALID_CMAP_ENTRY ) 
    return avg_cache[i_pixel][i_cmap];

  if ( !query_color(p_vout, i_pixel, rgb_vout) ) return INVALID_CMAP_ENTRY;

  avg_8bit_rgb(rgb_vout, cmap_rgb[i_cmap], i_trans);

#if 0
 {
  uint8_t rgb_approx[RGB_SIZE];

  avg_cache[i_pixel][i_cmap] = 
    find_cmap_rgb8_nearest(p_vout, rgb_vout, rgb_approx);
  printf(
         "cmap old %0x avg approx 0%x sub: %d sub=(%0x, %0x, %0x) "
         "approx=(%0x, %0x, %0x) avg vout=(%0x, %0x, %0x)\n", 
         i_pixel, avg_cache[i_pixel][i_cmap], i_cmap,
         cmap_rgb[i_cmap][RED_PIXEL], cmap_rgb[i_cmap][GREEN_PIXEL], 
         cmap_rgb[i_cmap][BLUE_PIXEL],
         rgb_approx[RED_PIXEL], rgb_approx[GREEN_PIXEL], rgb_approx[BLUE_PIXEL],
         rgb_vout[RED_PIXEL], rgb_vout[GREEN_PIXEL], rgb_vout[BLUE_PIXEL]);
 }  
#else 
  avg_cache[i_pixel][i_cmap] = 
    find_cmap_rgb8_nearest(p_vout, rgb_vout, NULL);
#endif
  return avg_cache[i_pixel][i_cmap];
}

#undef  BYTES_PER_PIXEL
#define BYTES_PER_PIXEL 1

static void 
BlendRGB2( vout_thread_t *p_vout, picture_t *p_pic,
            const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    uint8_t *p_pixel_base;
    uint8_t *p_src_start = (uint8_t *)p_spu->p_sys->p_data;
    uint8_t *p_src_end   = &p_src_start[p_spu->i_height * p_spu->i_width];
    uint8_t *p_source; /* This is the where the subtitle pixels come from */
    int i;

    int i_x, i_y;
    int i_y_src;

    /* Chroma specific */
    uint32_t i_xscale;   /* Amount we scale subtitle in the x direction,
                            multiplied by 2**ASCALE. */
    uint32_t i_yscale;   /* Amount we scale subtitle in the y direction.
                            multiplied by 2**ASCALE. */

    int i_width, i_height, i_ytmp;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    struct subpicture_sys_t *p_sys = p_spu->p_sys;
    unsigned int i_aspect_x, i_aspect_y;

    vout_AspectRatio( p_vout->render.i_aspect, &i_aspect_y, 
                      &i_aspect_x );
    
    i_xscale = (( p_vout->output.i_width << ASCALE ) * i_aspect_x)
      / (i_aspect_y * p_vout->render.i_width);
    i_yscale = ( p_vout->output.i_height << ASCALE ) / p_vout->render.i_height;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_RENDER), 
	       "spu: %dx%d, scaled: %dx%d, vout render: %dx%d, scale %dx%d", 
	       p_spu->i_width,  p_spu->i_height, 
	       p_vout->output.i_width, p_vout->output.i_height,
	       p_vout->render.i_width, p_vout->render.i_height,
	       i_xscale, i_yscale
	       );

    i_width  = p_spu->i_width  * i_xscale;
    i_height = p_spu->i_height * i_yscale;


    /** FIXME: do once per subtitle in subtitle processing, not here
        each time we render.  */
    /* Find a corresponding colormap entries for our palette entries. */
    for( i = 0; i < NUM_SUBTITLE_COLORS; i++ )
    {

      if ( p_sys->p_palette[i].s.t != 0 ) {
        uint8_t rgb[RGB_SIZE]; 
        yuv2rgb(&(p_sys->p_palette[i]), rgb);
        cmap[i] = 
          find_cmap_rgb8_nearest(p_vout, rgb, cmap_rgb[i]);
        dbg_print( (DECODE_DBG_RENDER), 
                   "palette %d RGB=(%0x, %0x, %0x)\n", i,
                   rgb[RED_PIXEL], rgb[GREEN_PIXEL], rgb[BLUE_PIXEL]);
      }
    }

    /* Set where we will start blending subtitle from using
       the picture coordinates subtitle offsets
    */
    p_pixel_base = p_pic->p->p_pixels 
              + ( (p_spu->i_x * i_xscale) >> ASCALE ) * BYTES_PER_PIXEL
              + ( (p_spu->i_y * i_yscale) >> ASCALE ) * p_pic->p->i_pitch;

    i_x_start = p_sys->i_x_start;
    i_y_start = i_yscale * p_sys->i_y_start;
    i_x_end   = p_sys->i_x_end;
    i_y_end   = i_yscale * p_sys->i_y_end;

    p_source = (uint8_t *)p_sys->p_data;
  
    /* Draw until we reach the bottom of the subtitle */
    i_y = 0;
    for( i_y_src = 0 ; i_y_src < p_spu->i_height * p_spu->i_width; 
         i_y_src += p_spu->i_width )
      {
	uint8_t *p_pixel_base_y;
        i_ytmp = i_y >> ASCALE;
        i_y += i_yscale;
	p_pixel_base_y = p_pixel_base + (i_ytmp * p_pic->p->i_pitch);
	i_x = 0;

        if ( b_crop ) {
          if ( i_y > i_y_end ) break;
          if (i_x_start) {
            i_x = i_x_start;
            p_source += i_x_start;
          }
        }
        
        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> ASCALE ) )
        {

          /* Draw until we reach the end of the line */
          for( ; i_x < p_spu->i_width; i_x ++, p_source++ )
            {
              ogt_yuvt_t p_yuvt;

              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }
              
              if (p_source >= p_src_end) {
                msg_Err( p_vout, "trying to access beyond subtitle %dx%d %d",
                         i_x, i_y / i_yscale, i_height);
                return;
              }
              
              p_yuvt = p_sys->p_palette[*p_source & 0x3];

#ifdef TESTING_TRANSPARENCY
              if (p_yuvt.s.t == MAX_TRANS) p_yuvt.s.t >>= 1;
#endif

              switch ( p_yuvt.s.t ) 
                {
                case 0:
		  /* Completely transparent. Don't change pixel. */
#if 0
                  printf(" "); /*++++*/
#endif
                  break;
                case MAX_TRANS: 
                  {
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    /* This is the pixel that's going to change;*/
                    uint8_t *p_dest = p_pixel_base_y + i_xdest;
                    memset( p_dest, cmap[*p_source & 0x3], i_xlast - i_xdest );
#if 0
                    printf("%1d", *p_source); /*++++*/
#endif
                    break;
                  }
                default:
                  {
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    /* This is the pixel that's going to change;*/
                    uint8_t *p_pixel = p_pixel_base_y + i_xdest;
                    uint32_t len     = i_xlast - i_xdest;

#if FULL_TRANSPARENCY
                    /* This is what should be done, but it may be too
		       slow.  */
                    for ( len = i_xlast - i_xdest -1; len >= 0; len-- ) 
                      {
                        cmap_t i_cmap  = avg_rgb2(p_vout, *p_pixel, p_yuvt.s.t,
                                                  *p_source, p_sys->i_pts);
                                                  
                        if (i_cmap != INVALID_CMAP_ENTRY) 
                          *p_pixel= (uint8_t) i_cmap;
                        p_pixel++;
                      }
#else
                    cmap_t i_cmap  = avg_rgb2(p_vout, *p_pixel, p_yuvt.s.t,
                                              *p_source, p_sys->i_pts);
                    if (i_cmap != INVALID_CMAP_ENTRY) 
                      memset(p_pixel, i_cmap, len);
#endif 

#if 0
                    printf("%1d", *p_source); /*++++*/
#endif
                  }
                }
            }
#if 0
          printf("\n"); /*++++*/
#endif
        } else {
          /* Have to scale over many lines. */
          int i_yreal = p_pic->p->i_pitch * i_ytmp;
          int i_ynext = p_pic->p->i_pitch * i_y >> ASCALE;

           /* Draw until we reach the end of the line */
           for( ; i_x < p_spu->i_width; i_x ++, p_source++ )
             {
              ogt_yuvt_t p_yuvt = p_sys->p_palette[*p_source & 0x3];

              if( b_crop ) {
                
                /* FIXME: y cropping should be dealt with outside of this 
                   loop.*/
                if ( i_y < i_y_start) continue;
                
                if ( i_x > i_x_end )
                  {
                    p_source += p_spu->i_width - i_x;
                    break;
                  }
              }
              
              if (p_source >= p_src_end) {
                msg_Err( p_vout, "trying to access beyond subtitle %dx%d %d",
                         i_x, i_y / i_yscale, i_height);
                return;
              }
              
#ifdef TESTING_TRANSPARENCY
              if (p_yuvt.s.t == MAX_TRANS) p_yuvt.s.t >>= 1;
#endif
              switch ( p_yuvt.s.t ) 
                {
                case 0:
		  /* Completely transparent. Don't change pixel. */
#if 0
                  printf(" "); /*++++*/
#endif
                  break;
                case MAX_TRANS: 
                  {
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    uint32_t len     = i_xlast - i_xdest;
#if 0
                    printf("%1d", *p_source); /*++++*/
#endif
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch ) {
                      uint8_t *p_dest = p_pixel_base + i_ytmp + i_xdest;
                      memset( p_dest, cmap[*p_source & 0x3], len );
                    }
                    break;
                  }
                default:
                  {
                    uint32_t i_xdest = ( ((i_x*i_xscale) >> ASCALE) 
                                         * BYTES_PER_PIXEL );
                    uint32_t i_xlast = ( (((i_x+1)*i_xscale) >> ASCALE)
                                         * BYTES_PER_PIXEL );
                    int32_t len = i_xlast - i_xdest;
#if 0
                    printf("%1d", *p_source); /*++++*/
#endif

                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch ) {
                      /* This is the pixel that's going to change;*/
                      uint8_t *p_pixel = p_pixel_base + i_ytmp + i_xdest;

#if FULL_TRANSPARENCY
                    /* This is what should be done, but it may be too
		       slow.  */
                      for ( len = i_xlast - i_xdest -1; len >= 0; len-- ) 
                        {
                          cmap_t i_cmap  = avg_rgb2(p_vout, *p_pixel, 
                                                    p_yuvt.s.t, *p_source, 
                                                    p_sys->i_pts);
                          if (i_cmap != INVALID_CMAP_ENTRY) 
                            *p_pixel= (uint8_t) i_cmap;
                          p_pixel++;
                        }
#else
                    cmap_t i_cmap  = avg_rgb2(p_vout, *p_pixel, p_yuvt.s.t, 
                                              *p_source, p_sys->i_pts);
                    if (i_cmap != INVALID_CMAP_ENTRY) 
                      memset(p_pixel, i_cmap, len);
#endif 

                    }
                  }
                }
             }
        }
      }
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
