/*****************************************************************************
 * video_yuv.h: MMX YUV transformation functions
 * Provides functions to perform the YUV conversion. The functions provided here
 * are a complete and portable C implementation, and may be replaced in certain
 * case by optimized functions.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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

#define GRAY_MARGIN     384
#define GRAY_TABLE_SIZE 1024                             /* total table size */

#define PALETTE_TABLE_SIZE 2176          /* YUV -> 8bpp palette lookup table */

/* argument lists for YUV functions */
#define YUV_ARGS_8BPP p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, \
yuv_data_t *p_u, yuv_data_t *p_v, int i_width, int i_height, int i_pic_width, \
int i_pic_height, int i_pic_line_width, int i_matrix_coefficients

#define YUV_ARGS_16BPP p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, \
yuv_data_t *p_u, yuv_data_t *p_v, int i_width, int i_height, int i_pic_width, \
int i_pic_height, int i_pic_line_width, int i_matrix_coefficients

#define YUV_ARGS_24BPP p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, \
yuv_data_t *p_u, yuv_data_t *p_v, int i_width, int i_height, int i_pic_width, \
int i_pic_height, int i_pic_line_width, int i_matrix_coefficients

#define YUV_ARGS_32BPP p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, \
yuv_data_t *p_u, yuv_data_t *p_v, int i_width, int i_height, int i_pic_width, \
int i_pic_height, int i_pic_line_width, int i_matrix_coefficients

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
void SetYUV               ( vout_thread_t *p_vout );
void SetOffset            ( int i_width, int i_height, int i_pic_width,
                            int i_pic_height, boolean_t *pb_h_scaling,
                            int *pi_v_scaling, int *p_offset );

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

