/*****************************************************************************
 * video_spu.h : DVD subpicture units functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_spu.h,v 1.9 2001/05/08 20:38:25 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@via.ecp.fr>
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
void   vout_RenderRGBSPU( picture_t *p_pic, const subpicture_t *p_spu,
                          vout_buffer_t *p_buffer,
                          int i_bytes_per_pixel, int i_bytes_per_line );
void   vout_RenderYUVSPU( picture_t *p_pic, const subpicture_t *p_spu );

