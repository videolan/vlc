/*****************************************************************************
 * arm_neon.c: NEON assembly optimized audio conversions
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

static int Open (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("ARM NEON audio format conversions") )
    add_submodule ()
        set_capability ("audio filter", 20)
        set_callbacks (Open, NULL)
vlc_module_end ()

static void Do_F32_S32 (aout_instance_t *, aout_filter_t *,
                        aout_buffer_t *, aout_buffer_t *);

static int Open (vlc_object_t *obj)
{
    aout_filter_t *filter = (aout_filter_t *)obj;

    if (!(vlc_CPU () & CPU_CAPABILITY_NEON))
        return VLC_EGENERIC;
    if (!AOUT_FMTS_SIMILAR (&filter->input, &filter->output))
        return VLC_EGENERIC;

    if (filter->input.i_format == VLC_CODEC_FL32)
    {
        if (filter->output.i_format == VLC_CODEC_FI32)
            filter->pf_do_work = Do_F32_S32;
        else
            return VLC_EGENERIC;
    }
    else
        return VLC_EGENERIC;

    filter->b_in_place = true;
    return 0;
}

/**
 * Half-precision floating point to signed fixed point conversion.
 */
static void Do_F32_S32 (aout_instance_t *aout, aout_filter_t *filter,
                        aout_buffer_t *inbuf, aout_buffer_t *outbuf)
{
    unsigned nb_samples = inbuf->i_nb_samples
                     * aout_FormatNbChannels (&filter->input);
    const float *inp = (float *)inbuf->p_buffer;
    const float *endp = inp + nb_samples;
    int32_t *outp = (int32_t *)outbuf->p_buffer;

    if (nb_samples & 1)
        *(outp++) = *(inp++) * FIXED32_ONE;

    if (nb_samples & 2)
        asm volatile (
            "vld1.f32 {d0}, [%[inp]]!\n"
            "vcvt.s32.f32 d1, d0, #28\n"
            "vst1.s32 {d1}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "memory");

    if (nb_samples & 4)
        asm volatile (
            "vld2.f32 {q0}, [%[inp]]!\n"
            "vcvt.s32.f32 q1, q0, #28\n"
            "vst2.s32 {q1}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "q1", "memory");

    while (inp != endp)
        asm volatile (
            "vld4.f32 {q0-q1}, [%[inp]]!\n"
            "vcvt.s32.f32 q2, q0, #28\n"
            "vcvt.s32.f32 q3, q1, #28\n"
            "vst4.s32 {q2-q3}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "q1", "q2", "q3", "memory");

    outbuf->i_nb_samples = inbuf->i_nb_samples;
    outbuf->i_nb_bytes = inbuf->i_nb_bytes;
    (void) aout;
}
