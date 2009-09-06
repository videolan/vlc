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
static void Do_S32_S16 (aout_instance_t *, aout_filter_t *,
                        aout_buffer_t *, aout_buffer_t *);

static int Open (vlc_object_t *obj)
{
    aout_filter_t *filter = (aout_filter_t *)obj;

    if (!(vlc_CPU () & CPU_CAPABILITY_NEON))
        return VLC_EGENERIC;
    if (!AOUT_FMTS_SIMILAR (&filter->input, &filter->output))
        return VLC_EGENERIC;

    switch (filter->input.i_format)
    {
        case VLC_CODEC_FL32:
            switch (filter->output.i_format)
            {
                case VLC_CODEC_FI32:
                    filter->pf_do_work = Do_F32_S32;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_FI32:
            switch (filter->output.i_format)
            {
                case VLC_CODEC_S16N:
                    filter->pf_do_work = Do_S32_S16;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;
        default:
            return VLC_EGENERIC;
    }

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
    {
        asm volatile (
            "vldr.32 s0, [%[inp]]\n"
            "vcvt.s32.f32 d0, d0, #28\n"
            "vstr.32 s0, [%[outp]]\n"
            :
            : [outp] "r" (outp), [inp] "r" (inp)
            : "d0", "memory");
        outp++;
        inp++;
    }

    if (nb_samples & 2)
        asm volatile (
            "vld1.f32 {d0}, [%[inp]]!\n"
            "vcvt.s32.f32 d0, d0, #28\n"
            "vst1.s32 {d0}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "d0", "memory");

    if (nb_samples & 4)
        asm volatile (
            "vld1.f32 {q0}, [%[inp]]!\n"
            "vcvt.s32.f32 q0, q0, #28\n"
            "vst1.s32 {q0}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "memory");

    while (inp != endp)
        asm volatile (
            "vld1.f32 {q0-q1}, [%[inp]]!\n"
            "vcvt.s32.f32 q0, q0, #28\n"
            "vcvt.s32.f32 q1, q1, #28\n"
            "vst1.s32 {q0-q1}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "q1", "memory");

    outbuf->i_nb_samples = inbuf->i_nb_samples;
    outbuf->i_nb_bytes = inbuf->i_nb_bytes;
    (void) aout;
}

/**
 * Signed 32-bits fixed point to signed 16-bits integer
 */
static void Do_S32_S16 (aout_instance_t *aout, aout_filter_t *filter,
                        aout_buffer_t *inbuf, aout_buffer_t *outbuf)
{
    unsigned nb_samples = inbuf->i_nb_samples
                     * aout_FormatNbChannels (&filter->input);
    int32_t *inp = (int32_t *)inbuf->p_buffer;
    const int32_t *endp = inp + nb_samples;
    int16_t *outp = (int16_t *)outbuf->p_buffer;

    while (nb_samples & 3)
    {
        const int16_t roundup = 1 << 12;
        asm volatile (
            "qadd r0, %[inv], %[roundup]\n"
            "ssat %[outv], #16, r0, asr #13\n"
            : [outv] "=r" (*outp)
            : [inv] "r" (*inp), [roundup] "r" (roundup)
            : "r0");
        inp++;
        outp++;
        nb_samples--;
    }

    if (nb_samples & 4)
        asm volatile (
            "vld1.s32 {q0}, [%[inp]]!\n"
            "vrshrn.i32 d0, q0, #13\n"
            "vst1.s16 {d0}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "memory");

    if (nb_samples & 8)
        asm volatile (
            "vld1.s32 {q0-q1}, [%[inp]]!\n"
            "vrshrn.i32 d0, q0, #13\n"
            "vrshrn.i32 d1, q1, #13\n"
            "vst1.s16 {q0}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "q1", "memory");

    while (inp != endp)
        asm volatile (
            "vld1.s32 {q0-q1}, [%[inp]]!\n"
            "vld1.s32 {q2-q3}, [%[inp]]!\n"
            "vrshrn.s32 d0, q0, #13\n"
            "vrshrn.s32 d1, q1, #13\n"
            "vrshrn.s32 d2, q2, #13\n"
            "vrshrn.s32 d3, q3, #13\n"
            "vst1.s16 {q0-q1}, [%[outp]]!\n"
            : [outp] "+r" (outp), [inp] "+r" (inp)
            :
            : "q0", "q1", "q2", "q3", "memory");

    outbuf->i_nb_samples = inbuf->i_nb_samples;
    outbuf->i_nb_bytes = inbuf->i_nb_bytes / 2;
    (void) aout;
}
