/*****************************************************************************
 * video_decoder.h : video decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_decoder.h,v 1.5 2001/08/22 17:21:45 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
void            vdec_InitThread         ( struct vdec_thread_s * );
void            vdec_EndThread          ( struct vdec_thread_s * );
void            vdec_DecodeMacroblockC  ( struct vdec_thread_s *,
                                          struct macroblock_s * );
void            vdec_DecodeMacroblockBW ( struct vdec_thread_s *,
                                          struct macroblock_s * );
struct vdec_thread_s * vdec_CreateThread( struct vdec_pool_s * );
void            vdec_DestroyThread      ( struct vdec_thread_s * );

