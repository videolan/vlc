/*****************************************************************************
 * video_yuv.h: YUV transformation functions
 * Provides functions prototypes to perform the YUV conversion. The functions
 * may be implemented in one of the video_yuv_* files.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int             vout_InitYUV      ( vout_thread_t *p_vout );
int             vout_ResetYUV     ( vout_thread_t *p_vout );
void            vout_EndYUV       ( vout_thread_t *p_vout );

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
#ifdef HAVE_MMX

/* YUV transformations for MMX - in video_yuv_mmx.S
 *      p_y, p_u, p_v:          Y U and V planes
 *      i_width, i_height:      frames dimensions (pixels)
 *      i_ypitch, i_vpitch:     Y and V lines sizes (bytes)
 *      i_aspect:               vertical aspect factor
 *      p_pic:                  RGB frame
 *      i_dci_offset:           XXX?? x offset for left image border
 *      i_offset_to_line_0:     XXX?? x offset for left image border
 *      i_pitch:                RGB line size (bytes)
 *      i_colortype:            0 for 565, 1 for 555 */
void ConvertYUV420RGB16MMX( u8* p_y, u8* p_u, u8 *p_v,
                            unsigned int i_width, unsigned int i_height,
                            unsigned int i_ypitch, unsigned int i_vpitch,
                            unsigned int i_aspect, u8 *p_pic,
                            u32 i_dci_offset, u32 i_offset_to_line_0,
                            int i_pitch, int i_colortype );
#endif
