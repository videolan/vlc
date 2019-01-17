/*****************************************************************************
 * simple_neon.h : simple channel mixer plug-in using NEON assembly
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2006-2009, 2012, 2015 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          David Geldreich <david.geldreich@free.fr>
 *          Sébastien Toque
 *          Thomas Guillem <thomas@gllm.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_cpu.h>

/* Only conversion to Mono, Stereo and 4.0 right now */
/* Only from 7/7.1/5/5.1/3/3.1/2.0
 * XXX 5.X rear and middle are handled the same way */

#define NEON_WRAPPER(in, out)                                                    \
    void convert_##in##_to_##out##_neon_asm(float *dst, const float *src, int num, bool lfeChannel); \
    static inline void DoWork_##in##_to_##out##_neon( filter_t *p_filter, block_t *p_in_buf, block_t *p_out_buf )  \
    {                                                                            \
        const float *p_src = (const float *)p_in_buf->p_buffer;                  \
        float *p_dest = (float *)p_out_buf->p_buffer;                            \
        convert_##in##_to_##out##_neon_asm( p_dest, p_src, p_in_buf->i_nb_samples, \
                  p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE );  \
    } \
    static inline void (*GET_WORK_##in##_to_##out##_neon())(filter_t*, block_t*, block_t*) \
    { \
        return vlc_CPU_ARM_NEON() ? DoWork_##in##_to_##out##_neon : DoWork_##in##_to_##out; \
    }

NEON_WRAPPER(7_x,2_0)
NEON_WRAPPER(5_x,2_0)
NEON_WRAPPER(4_0,2_0)
NEON_WRAPPER(3_x,2_0)
NEON_WRAPPER(7_x,1_0)
NEON_WRAPPER(5_x,1_0)
NEON_WRAPPER(7_x,4_0)
NEON_WRAPPER(5_x,4_0)

/* TODO: the following conversions are not handled in NEON */

#define C_WRAPPER(in, out) \
    static inline void (*GET_WORK_##in##_to_##out##_neon())(filter_t*, block_t*, block_t*) \
    { \
        return DoWork_##in##_to_##out; \
    }

C_WRAPPER(4_0,1_0)
C_WRAPPER(3_x,1_0)
C_WRAPPER(2_x,1_0)
C_WRAPPER(6_1,2_0)
C_WRAPPER(7_x,5_x)
C_WRAPPER(6_1,5_x)
