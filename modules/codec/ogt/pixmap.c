/*****************************************************************************
 * Common pixel/chroma manipulation routines.
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id: pixmap.c,v 1.1 2004/01/30 13:17:12 rocky Exp $
 *
 * Author: Rocky Bernstein
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

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "pixmap.h"

/* FIXME: This is copied from modules/video_chroma/i420_rgb.h. 
   Include from a more common location.
 */

/*****************************************************************************
 * chroma_sys_t: chroma method descriptor
 *****************************************************************************
 * This structure is part of the chroma transformation descriptor, it
 * describes the yuv2rgb specific properties.
 *****************************************************************************/
struct chroma_sys_t
{
    uint8_t  *p_buffer;
    int *p_offset;

    /* Pre-calculated conversion tables */
    void *p_base;                         /* base for all conversion tables */
    uint8_t   *p_rgb8;                    /* RGB 8 bits table */
    uint16_t  *p_rgb16;                   /* RGB 16 bits table */
    uint32_t  *p_rgb32;                   /* RGB 32 bits table */

    /* To get RGB value for palette entry i, use (p_rgb_r[i], p_rgb_g[i],
       p_rgb_b[i])
     */
    uint8_t  *p_rgb_r;                   /* Red values of palette */
    uint8_t  *p_rgb_g;                   /* Green values of palette */
    uint8_t  *p_rgb_b;                   /* Blue values of palette */
};


/* Number of entries in RGB palette/colormap*/
#define CMAP_SIZE 256

/* 
    From
    http://www.inforamp.net/~poynton/notes/colour_and_gamma/ColorFAQ.html#RTFToC11
    http://people.ee.ethz.ch/~buc/brechbuehler/mirror/color/ColorFAQ.html#RTFToC1
 
    11. What is "luma"?

    It is useful in a video system to convey a component representative of
    luminance and two other components representative of colour. It is
    important to convey the component representative of luminance in such
    a way that noise (or quantization) introduced in transmission,
    processing and storage has a perceptually similar effect across the
    entire tone scale from black to white. The ideal way to accomplish
    these goals would be to form a luminance signal by matrixing RGB, then
    subjecting luminance to a nonlinear transfer function similar to the
    L* function.

    There are practical reasons in video to perform these operations
    in the opposite order. First a nonlinear transfer function - gamma
    correction - is applied to each of the linear R, G and B. Then a
    weighted sum of the nonlinear components is computed to form a
    signal representative of luminance. The resulting component is
    related to brightness but is not CIE luminance. Many video
    engineers call it luma and give it the symbol Y'. It is often
    carelessly called luminance and given the symbol Y. You must be
    careful to determine whether a particular author assigns a linear
    or nonlinear interpretation to the term luminance and the symbol
    Y.

    The coefficients that correspond to the "NTSC" red, green and blue
    CRT phosphors of 1953 are standardized in ITU-R Recommendation BT.
    601-2 (formerly CCIR Rec.  601-2). I call it Rec.  601. To compute
    nonlinear video luma from nonlinear red, green and blue:

    Y'601 = 0.299R' 0.587G' + 0.114B'

    We will use the integer scaled versions of these numbers below
    as RED_COEF, GREEN_COEF and BLUE_COEF.
 */

/* 19 = round(0.299 * 64) */
#define RED_COEF   ((int32_t) 19)

/* 38 = round(0.587 * 64) */
#define GREEN_COEF ((int32_t) 37)

/* 7 = round(0.114 * 64) */
#define BLUE_COEF  ((int32_t)  7)

/** 
   Find the nearest colormap entry in p_vout (assumed to have RGB2
   chroma, i.e. 256 RGB entries) that is closest in color to p_yuv.  Set
   RGB to the color found and return the colormap index. -1 is returned
   if there is some error.

   The closest match is determined by the the Euclidean distance
   using integer-scaled 601-2 coefficients described above.

   Actually, we use the square of the Euclidean distance; but in
   comparisons it amounts to the same thing.
*/

int
find_cmap_rgb8_nearest(const vout_thread_t *p_vout, const ogt_yuvt_t *p_yuv,
		       uint8_t *out_rgb) 
{
  uint8_t *p_cmap_r;
  uint8_t *p_cmap_g;
  uint8_t *p_cmap_b;
  uint8_t rgb[RGB_SIZE];
  
  int i;
  int i_bestmatch=0;
  uint32_t i_mindist = 0xFFFFFFFF; /* The largest number here. */

  /* Check that we really have RGB2. */
  
  if ( !p_vout && p_vout->output.i_chroma  != VLC_FOURCC('R','G','B','2') )
    return -1;

  p_cmap_r=p_vout->chroma.p_sys->p_rgb_r;
  p_cmap_g=p_vout->chroma.p_sys->p_rgb_g;
  p_cmap_b=p_vout->chroma.p_sys->p_rgb_b;

  yuv2rgb(p_yuv, rgb);

  for (i = 0; i < CMAP_SIZE; i++) {
    /* Interval range calculations to show that we don't overflow the
       word sizes below. pixels component values start out 8
       bits. When we subtract two components we get 9 bits, then
       square to 10 bits.  Next we scale by 6 we get 16 bits. XXX_COEF
       all fit into 5 bits, so when we multiply we should have 21 bits
       maximum. So computations can be done using 32-bit
       precision. However before storing back distance components we
       scale back down by 12 bits making the precision 9 bits.

       The squared distance is the sum of three of the 9-bit numbers
       described above. This then uses 21-bits and also fits in a
       32-bit word.
     */

    /* We use in integer fixed-point fractions rather than floating
       point for speed. We multiply by 64 (= 1 << 6) before computing
       the product, and divide the result by 64*64 (= 1 >> (6*2)).
    */

#define SCALEBITS 6 
#define int32_sqr(x) ( ((int32_t) (x)) * ((int32_t) x) )

    uint32_t dr = ( RED_COEF   * ( int32_sqr(rgb[RED_PIXEL]   - p_cmap_r[i])
				 << SCALEBITS ) ) >> (SCALEBITS*2);
    uint32_t dg = ( GREEN_COEF * ( int32_sqr(rgb[GREEN_PIXEL] - p_cmap_g[i])
				 << SCALEBITS ) ) >> (SCALEBITS*2);
    uint32_t db = ( BLUE_COEF  * ( int32_sqr(rgb[BLUE_PIXEL]  - p_cmap_b[i])
				  << SCALEBITS ) ) >> (SCALEBITS*2);

    uint32_t i_dist = dr + dg + db;
    if (i_dist < i_mindist) {
      i_bestmatch = i;
      i_mindist = i_dist;
    }
  }

  out_rgb[RED_PIXEL]   = p_cmap_r[i_bestmatch];
  out_rgb[GREEN_PIXEL] = p_cmap_g[i_bestmatch];
  out_rgb[BLUE_PIXEL]  = p_cmap_b[i_bestmatch];

  return i_bestmatch;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
