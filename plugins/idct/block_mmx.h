/*****************************************************************************
 * block_mmx.h: Macroblock copy functions in MMX assembly
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: block_mmx.h,v 1.2 2002/05/18 17:47:46 sam Exp $
 *
 * Authors: Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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
 * InitBlock: placeholder because we don't need a crop table, MMX does it for us
 *****************************************************************************/
static inline void InitBlock( )
{
    ;
}

/*****************************************************************************
 * AddBlock: add a block
 *****************************************************************************/
#define ADD_MMX(offset,r1,r2,r3,r4)                                         \
    movq_m2r (*(p_data+2*i_incr), r1);                                      \
    packuswb_r2r (r4, r3);                                                  \
    movq_r2r (r1, r2);                                                      \
    p_data += i_incr;                                                       \
    movq_r2m (r3, *p_data);                                                 \
    punpcklbw_r2r (mm0, r1);                                                \
    paddsw_m2r (*(p_block+offset), r1);                                     \
    punpckhbw_r2r (mm0, r2);                                                \
    paddsw_m2r (*(p_block+offset+4), r2);

static inline void AddBlock( dctelem_t * p_block, yuv_data_t * p_data,
                             int i_incr )
{
    movq_m2r (*p_data, mm1);
    pxor_r2r (mm0, mm0);
    movq_m2r (*(p_data + i_incr), mm3);
    movq_r2r (mm1, mm2);
    punpcklbw_r2r (mm0, mm1);
    movq_r2r (mm3, mm4);
    paddsw_m2r (*(p_block+0*8), mm1);
    punpckhbw_r2r (mm0, mm2);
    paddsw_m2r (*(p_block+0*8+4), mm2);
    punpcklbw_r2r (mm0, mm3);
    paddsw_m2r (*(p_block+1*8), mm3);
    packuswb_r2r (mm2, mm1);
    punpckhbw_r2r (mm0, mm4);
    movq_r2m (mm1, *p_data);
    paddsw_m2r (*(p_block+1*8+4), mm4);
    ADD_MMX (2*8, mm1, mm2, mm3, mm4);
    ADD_MMX (3*8, mm3, mm4, mm1, mm2);
    ADD_MMX (4*8, mm1, mm2, mm3, mm4);
    ADD_MMX (5*8, mm3, mm4, mm1, mm2);
    ADD_MMX (6*8, mm1, mm2, mm3, mm4);
    ADD_MMX (7*8, mm3, mm4, mm1, mm2);
    packuswb_r2r (mm4, mm3);
    movq_r2m (mm3, *(p_data + i_incr));
}

/*****************************************************************************
 * CopyBlock: copy a block
 *****************************************************************************/
#define COPY_MMX(offset,r0,r1,r2)                                           \
    movq_m2r (*(p_block+offset), r0);                                       \
    p_data += i_incr;                                                       \
    movq_m2r (*(p_block+offset+4), r1);                                     \
    movq_r2m (r2, *p_data);                                                 \
    packuswb_r2r (r1, r0);

static inline void CopyBlock( dctelem_t * p_block, yuv_data_t * p_data,
                              int i_incr )
{
    movq_m2r (*(p_block+0*8), mm0);
    movq_m2r (*(p_block+0*8+4), mm1);
    movq_m2r (*(p_block+1*8), mm2);
    packuswb_r2r (mm1, mm0);
    movq_m2r (*(p_block+1*8+4), mm3);
    movq_r2m (mm0, *p_data);
    packuswb_r2r (mm3, mm2);
    COPY_MMX (2*8, mm0, mm1, mm2);
    COPY_MMX (3*8, mm2, mm3, mm0);
    COPY_MMX (4*8, mm0, mm1, mm2);
    COPY_MMX (5*8, mm2, mm3, mm0);
    COPY_MMX (6*8, mm0, mm1, mm2);
    COPY_MMX (7*8, mm2, mm3, mm0);
    movq_r2m (mm2, *(p_data + i_incr));
}

