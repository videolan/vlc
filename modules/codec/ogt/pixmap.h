/*****************************************************************************
 * Common pixel/chroma manipulation routines.
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id: pixmap.h,v 1.3 2004/01/29 11:50:22 rocky Exp $
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

#ifndef PIXMAP_H
#define PIXMAP_H 

/* Color and transparency of a pixel or a palette (CLUT) entry */
typedef union {
  uint8_t plane[4];
  struct {
    uint8_t y;
    uint8_t v;
    uint8_t u;
    uint8_t t;
  } s;
} ogt_yuvt_t;

/* Force v in the range 0.255. In video_chroma/i420_rgb.c, this
   is macro is called CLIP. FIXME: Combine with that.
*/
#define clip_8_bit(v) \
  ((v < 0) ? 0 : (v > 255) ? 255 : v)

static inline void
yuv2rgb(ogt_yuvt_t *p_yuv, uint8_t *p_rgb_out )
{
  
  int i_Y  = p_yuv->s.y - 16;
  int i_Cb = p_yuv->s.v - 128;
  int i_Cr = p_yuv->s.u - 128;
  
  int i_red   = (1.1644 * i_Y) + (1.5960 * i_Cr);
  int i_green = (1.1644 * i_Y) - (0.3918 * i_Cb) - (0.8130 * i_Cr);
  int i_blue  = (1.1644 * i_Y) + (2.0172 * i_Cb);
  
  i_red   = clip_8_bit( i_red );
  i_green = clip_8_bit( i_green );
  i_blue  = clip_8_bit( i_blue );
  
#ifdef WORDS_BIGENDIAN
  *p_rgb_out++ = i_red;
  *p_rgb_out++ = i_green;
  *p_rgb_out++ = i_blue;
#else
  *p_rgb_out++ = i_blue;
  *p_rgb_out++ = i_green;
  *p_rgb_out++ = i_red;
#endif
  
}

#define GREEN_PIXEL 1
#ifdef WORDS_BIGENDIAN
#define RED_PIXEL   0
#define BLUE_PIXEL  2
#else
#define BLUE_PIXEL  2
#define RED_PIXEL   0
#endif

static inline void
put_rgb24_pixel(uint8_t *rgb, uint8_t *p_pixel)
{
#ifdef WORDS_BIGENDIAN
  *p_pixel++;
#endif
  *p_pixel++ = rgb[RED_PIXEL];
  *p_pixel++ = rgb[GREEN_PIXEL];
  *p_pixel++ = rgb[BLUE_PIXEL];
}


#endif /* PIXMAP_H */
