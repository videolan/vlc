/*****************************************************************************
 * decoder.h : video decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: decoder.h,v 1.3 2002/08/08 00:35:11 sam Exp $
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
 
typedef struct vpar_thread_t vpar_thread_t;
typedef struct vdec_pool_t vdec_pool_t;

/*****************************************************************************
 * vdec_thread_t: video decoder thread descriptor
 *****************************************************************************/
typedef struct vdec_thread_t
{
    VLC_COMMON_MEMBERS

    /* IDCT iformations */
    void *        p_idct_data;

    /* Input properties */
    vdec_pool_t * p_pool;

} vdec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void            vdec_InitThread         ( vdec_thread_t * );
void            vdec_EndThread          ( vdec_thread_t * );
void            vdec_DecodeMacroblockBW ( vdec_thread_t *, macroblock_t * );
void            vdec_DecodeMacroblock420( vdec_thread_t *, macroblock_t * );
void            vdec_DecodeMacroblock422( vdec_thread_t *, macroblock_t * );
void            vdec_DecodeMacroblock444( vdec_thread_t *, macroblock_t * );
vdec_thread_t * vdec_CreateThread       ( vdec_pool_t * );
void            vdec_DestroyThread      ( vdec_thread_t * );

