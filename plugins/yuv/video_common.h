/*****************************************************************************
 * video_common.h: YUV transformation functions
 * Provides functions to perform the YUV conversion. The functions provided here
 * are a complete and portable C implementation, and may be replaced in certain
 * case by optimized functions.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_common.h,v 1.3 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Margins and offsets in conversion tables - Margins are used in case a RGB
 * RGB conversion would give a value outside the 0-255 range. Offsets have been
 * calculated to avoid using the same cache line for 2 tables. conversion tables
 * are 2*MARGIN + 256 long and stores pixels.*/
#define RED_MARGIN      178
#define GREEN_MARGIN    135
#define BLUE_MARGIN     224
#define RED_OFFSET      1501                                 /* 1323 to 1935 */
#define GREEN_OFFSET    135                                      /* 0 to 526 */
#define BLUE_OFFSET     818                                   /* 594 to 1298 */
#define RGB_TABLE_SIZE  1935                             /* total table size */

#define GRAY_MARGIN     384
#define GRAY_TABLE_SIZE 1024                             /* total table size */

#define PALETTE_TABLE_SIZE 2176          /* YUV -> 8bpp palette lookup table */

/* macros used for YUV pixel conversions */
#define SHIFT 20
#define U_GREEN_COEF    ((int)(-0.391 * (1<<SHIFT) / 1.164))
#define U_BLUE_COEF     ((int)(2.018 * (1<<SHIFT) / 1.164))
#define V_RED_COEF      ((int)(1.596 * (1<<SHIFT) / 1.164))
#define V_GREEN_COEF    ((int)(-0.813 * (1<<SHIFT) / 1.164))

/* argument lists for YUV functions */
#define YUV_ARGS( word_size ) p_vout_thread_t p_vout, word_size *p_pic, \
yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v, int i_width, int i_height, \
int i_pic_width, int i_pic_height, int i_pic_line_width, \
int i_matrix_coefficients

#define YUV_ARGS_8BPP    YUV_ARGS( u8 )
#define YUV_ARGS_16BPP   YUV_ARGS( u16 )
#define YUV_ARGS_24BPP   YUV_ARGS( u32 )
#define YUV_ARGS_32BPP   YUV_ARGS( u32 )

/*****************************************************************************
 * Extern prototypes
 *****************************************************************************/

void SetOffset( int i_width, int i_height, int i_pic_width, int i_pic_height,
                boolean_t *pb_h_scaling, int *pi_v_scaling,
                int *p_offset, boolean_t b_double );

void ConvertY4Gray8       ( YUV_ARGS_8BPP );
void ConvertYUV420RGB8    ( YUV_ARGS_8BPP );
void ConvertYUV422RGB8    ( YUV_ARGS_8BPP );
void ConvertYUV444RGB8    ( YUV_ARGS_8BPP );

void ConvertY4Gray16      ( YUV_ARGS_16BPP );
void ConvertYUV420RGB16   ( YUV_ARGS_16BPP );
void ConvertYUV422RGB16   ( YUV_ARGS_16BPP );
void ConvertYUV444RGB16   ( YUV_ARGS_16BPP );

void ConvertY4Gray24      ( YUV_ARGS_24BPP );
void ConvertYUV420RGB24   ( YUV_ARGS_24BPP );
void ConvertYUV422RGB24   ( YUV_ARGS_24BPP );
void ConvertYUV444RGB24   ( YUV_ARGS_24BPP );

void ConvertY4Gray32      ( YUV_ARGS_32BPP );
void ConvertYUV420RGB32   ( YUV_ARGS_32BPP );
void ConvertYUV422RGB32   ( YUV_ARGS_32BPP );
void ConvertYUV444RGB32   ( YUV_ARGS_32BPP );

void ConvertYUV420YCbr8    ( YUV_ARGS_8BPP );
void ConvertYUV422YCbr8    ( YUV_ARGS_8BPP );
void ConvertYUV444YCbr8    ( YUV_ARGS_8BPP );

void ConvertYUV420YCbr16    ( YUV_ARGS_16BPP );
void ConvertYUV422YCbr16    ( YUV_ARGS_16BPP );
void ConvertYUV444YCbr16    ( YUV_ARGS_16BPP );

void ConvertYUV420YCbr24    ( YUV_ARGS_24BPP );
void ConvertYUV422YCbr24    ( YUV_ARGS_24BPP );
void ConvertYUV444YCbr24    ( YUV_ARGS_24BPP );

void ConvertYUV420YCbr32    ( YUV_ARGS_32BPP );
void ConvertYUV422YCbr32    ( YUV_ARGS_32BPP );
void ConvertYUV444YCbr32    ( YUV_ARGS_32BPP );


