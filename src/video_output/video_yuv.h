/*****************************************************************************
 * video_yuv.h: YUV transformation functions
 * These functions set up YUV tables for colorspace conversion
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_yuv.h,v 1.7 2001/03/21 13:42:35 sam Exp $
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

