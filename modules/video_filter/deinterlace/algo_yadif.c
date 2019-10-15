/*****************************************************************************
 * algo_yadif.c : Wrapper for FFmpeg's Yadif algorithm
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 *
 * Author: Laurent Aimar <fenrir@videolan.org>
 *         Juha Jeronen  <juha.jeronen@jyu.fi> (soft field repeat hack)
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
#   include "config.h"
#endif

#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_cpu.h>
#include <vlc_picture.h>
#include <vlc_filter.h>

#include "deinterlace.h" /* filter_sys_t  */
#include "common.h"      /* FFMIN3 et al. */

#include "algo_yadif.h"

/*****************************************************************************
 * Yadif (Yet Another DeInterlacing Filter).
 *****************************************************************************/

/* yadif.h comes from yadif.c of FFmpeg project.
   Necessary preprocessor macros are defined in common.h. */
#include "yadif.h"

int RenderYadifSingle( filter_t *p_filter, picture_t *p_dst, picture_t *p_src )
{
    return RenderYadif( p_filter, p_dst, p_src, 0, 0 );
}

int RenderYadif( filter_t *p_filter, picture_t *p_dst, picture_t *p_src,
                 int i_order, int i_field )
{
    VLC_UNUSED(p_src);

    filter_sys_t *p_sys = p_filter->p_sys;

    /* */
    assert( i_order >= 0 && i_order <= 2 ); /* 2 = soft field repeat */
    assert( i_field == 0 || i_field == 1 );

    /* As the pitches must match, use ONLY pictures coming from picture_New()! */
    picture_t *p_prev = p_sys->context.pp_history[0];
    picture_t *p_cur  = p_sys->context.pp_history[1];
    picture_t *p_next = p_sys->context.pp_history[2];

    /* Account for soft field repeat.

       The "parity" parameter affects the algorithm like this (from yadif.h):
       uint8_t *prev2= parity ? prev : cur ;
       uint8_t *next2= parity ? cur  : next;

       The original parity expression that was used here is:
       (i_field ^ (i_order == i_field)) & 1

       Truth table:
       i_field = 0, i_order = 0  => 1
       i_field = 1, i_order = 1  => 0
       i_field = 1, i_order = 0  => 1
       i_field = 0, i_order = 1  => 0

       => equivalent with e.g.  (1 - i_order)  or  (i_order + 1) % 2

       Thus, in a normal two-field frame,
             parity 1 = first field  (i_order == 0)
             parity 0 = second field (i_order == 1)

       Now, with three fields, where the third is a copy of the first,
             i_order = 0  =>  parity 1 (as usual)
             i_order = 1  =>  due to the repeat, prev = cur, but also next = cur.
                              Because in such a case there is no motion
                              (otherwise field repeat makes no sense),
                              we don't actually need to invoke Yadif's filter().
                              Thus, set "parity" to 2, and use this to bypass
                              the filter.
             i_order = 2  =>  parity 0 (as usual)
    */
    int yadif_parity;
    if( p_cur  &&  p_cur->i_nb_fields > 2 )
        yadif_parity = (i_order + 1) % 3; /* 1, *2*, 0; where 2 is a special
                                             value meaning "bypass filter". */
    else
        yadif_parity = (i_order + 1) % 2; /* 1, 0 */

    /* Filter if we have all the pictures we need */
    if( p_prev && p_cur && p_next )
    {
        /* */
        void (*filter)(uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next,
                       int w, int prefs, int mrefs, int parity, int mode);

#if defined(HAVE_X86ASM)
        if( vlc_CPU_SSSE3() )
            filter = vlcpriv_yadif_filter_line_ssse3;
        else
        if( vlc_CPU_SSE2() )
            filter = vlcpriv_yadif_filter_line_sse2;
        else
#if defined(__i386__)
        if( vlc_CPU_MMXEXT() )
            filter = vlcpriv_yadif_filter_line_mmxext;
        else
#endif
#endif
            filter = yadif_filter_line_c;

        if( p_sys->chroma->pixel_size == 2 )
            filter = yadif_filter_line_c_16bit;

        for( int n = 0; n < p_dst->i_planes; n++ )
        {
            const plane_t *prevp = &p_prev->p[n];
            const plane_t *curp  = &p_cur->p[n];
            const plane_t *nextp = &p_next->p[n];
            plane_t *dstp        = &p_dst->p[n];

            for( int y = 1; y < dstp->i_visible_lines - 1; y++ )
            {
                if( (y % 2) == i_field  ||  yadif_parity == 2 )
                {
                    memcpy( &dstp->p_pixels[y * dstp->i_pitch],
                                &curp->p_pixels[y * curp->i_pitch], dstp->i_visible_pitch );
                }
                else
                {
                    int mode;
                    /* Spatial checks only when enough data */
                    mode = (y >= 2 && y < dstp->i_visible_lines - 2) ? 0 : 2;

                    assert( prevp->i_pitch == curp->i_pitch && curp->i_pitch == nextp->i_pitch );
                    filter( &dstp->p_pixels[y * dstp->i_pitch],
                            &prevp->p_pixels[y * prevp->i_pitch],
                            &curp->p_pixels[y * curp->i_pitch],
                            &nextp->p_pixels[y * nextp->i_pitch],
                            dstp->i_visible_pitch,
                            y < dstp->i_visible_lines - 2  ? curp->i_pitch : -curp->i_pitch,
                            y  - 1  ?  -curp->i_pitch : curp->i_pitch,
                            yadif_parity,
                            mode );
                }

                /* We duplicate the first and last lines */
                if( y == 1 )
                    memcpy(&dstp->p_pixels[(y-1) * dstp->i_pitch],
                               &dstp->p_pixels[ y    * dstp->i_pitch],
                               dstp->i_pitch);
                else if( y == dstp->i_visible_lines - 2 )
                    memcpy(&dstp->p_pixels[(y+1) * dstp->i_pitch],
                               &dstp->p_pixels[ y    * dstp->i_pitch],
                               dstp->i_pitch);
            }
        }

        p_sys->context.i_frame_offset = 1; /* p_cur will be rendered at next frame, too */

        return VLC_SUCCESS;
    }
    else if( !p_prev && !p_cur && p_next )
    {
        /* NOTE: For the first frame, we use the default frame offset
                 as set by Open() or SetFilterMethod(). It is always 0. */

        /* FIXME not good as it does not use i_order/i_field */
        RenderX( p_filter, p_dst, p_next );
        return VLC_SUCCESS;
    }
    else
    {
        p_sys->context.i_frame_offset = 1; /* p_cur will be rendered at next frame */

        return VLC_EGENERIC;
    }
}
