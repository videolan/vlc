/*****************************************************************************
 * postprocessing.h
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: postprocessing.h,v 1.2 2002/11/10 02:47:27 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#define QT_STORE_T int8_t

/* postprocessing available using to create i_mode */
#define PP_DEBLOCK_Y_H 0x00000001
#define PP_DEBLOCK_Y_V 0x00000002
#define PP_DEBLOCK_C_H 0x00000004
#define PP_DEBLOCK_C_V 0x00000008

#define PP_DERING_Y    0x00000010
#define PP_DERING_C    0x00000020

#define PP_AUTOLEVEL   0x80000000

/* error code, not really used */
#define PP_ERR_OK               0 /* no problem */
#define PP_ERR_INVALID_PICTURE  1 /* wrong picture size or chroma */
#define PP_ERR_INVALID_QP       2 /* need valid QP to make the postprocess */
#define PP_ERR_UNKNOWN        255


typedef struct postprocessing_s
{
    VLC_COMMON_MEMBERS
    
    module_t * p_module;
    
    u32 (*pf_getmode)( int i_quality, int b_autolevel );

    int (*pf_postprocess)( picture_t *p_pic,
                           QT_STORE_T *p_QP_store, unsigned int i_QP_stride,
                           unsigned int i_mode );
} postprocessing_t;

