/*****************************************************************************
 * audio_format.c: NEON assembly optimized audio conversions
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
#include <vlc_filter.h>

#include <assert.h>

static int Open (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("ARM NEON audio format conversions") )
    set_capability ("audio filter", 20)
    set_callbacks (Open, NULL)
vlc_module_end ()

static block_t *Do_F32_S32 (filter_t *, block_t *);
static block_t *Do_S32_S16 (filter_t *, block_t *);

static int Open (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (!AOUT_FMTS_SIMILAR (&filter->fmt_in.audio, &filter->fmt_out.audio))
        return VLC_EGENERIC;

    switch (filter->fmt_in.audio.i_format)
    {
        case VLC_CODEC_FL32:
            switch (filter->fmt_out.audio.i_format)
            {
                case VLC_CODEC_FI32:
                    filter->pf_audio_filter = Do_F32_S32;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_FI32:
            switch (filter->fmt_out.audio.i_format)
            {
                case VLC_CODEC_S16N:
                    filter->pf_audio_filter = Do_S32_S16;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/**
 * Single-precision floating point to signed fixed point conversion.
 */
static block_t *Do_F32_S32 (filter_t *filter, block_t *inbuf)
{
    unsigned nb_samples = inbuf->i_nb_samples
                     * aout_FormatNbChannels (&filter->fmt_in.audio);
    int32_t *outp = (int32_t *)inbuf->p_buffer;
    int32_t *endp = outp + nb_samples;

    if (nb_samples & 1)
    {
        asm volatile (
            "vldr.32 s0, [%[outp]]\n"
            "vcvt.s32.f32 d0, d0, #28\n"
            "vstr.32 s0, [%[outp]]\n"
            :
            : [outp] "r" (outp)
            : "d0", "memory");
        outp++;
    }

    if (nb_samples & 2)
        asm volatile (
            "vld1.f32 {d0}, [%[outp]]\n"
            "vcvt.s32.f32 d0, d0, #28\n"
            "vst1.s32 {d0}, [%[outp]]!\n"
            : [outp] "+r" (outp)
            :
            : "d0", "memory");

    if (nb_samples & 4)
        asm volatile (
            "vld1.f32 {q0}, [%[outp]]\n"
            "vcvt.s32.f32 q0, q0, #28\n"
            "vst1.s32 {q0}, [%[outp]]!\n"
            : [outp] "+r" (outp)
            :
            : "q0", "memory");

    while (outp != endp)
        asm volatile (
            "vld1.f32 {q0-q1}, [%[outp]]\n"
            "vcvt.s32.f32 q0, q0, #28\n"
            "vcvt.s32.f32 q1, q1, #28\n"
            "vst1.s32 {q0-q1}, [%[outp]]!\n"
            : [outp] "+r" (outp)
            :
            : "q0", "q1", "memory");

    return inbuf;
}

void s32_s16_neon_unaligned (int16_t *out, const int32_t *in, unsigned nb);
void s32_s16_neon (int16_t *out, const int32_t *in, unsigned nb);

/**
 * Signed 32-bits fixed point to signed 16-bits integer
 */
static block_t *Do_S32_S16 (filter_t *filter, block_t *inbuf)
{
    const int32_t *in = (int32_t *)inbuf->p_buffer;
    int16_t *out = (int16_t *)in;
    unsigned nb;

    nb = ((-(uintptr_t)in) & 12) >> 2;
    out += nb; /* fix up misalignment */
    inbuf->p_buffer += 2 * nb;

    s32_s16_neon_unaligned (out, in, nb);
    in += nb;
    out += nb;

    nb = inbuf->i_nb_samples
         * aout_FormatNbChannels (&filter->fmt_in.audio) - nb;
    assert (!(((uintptr_t)in) & 15));
    assert (!(((uintptr_t)out) & 15));

    s32_s16_neon (out, in, nb);
    inbuf->i_buffer /= 2;
    return inbuf;
}
