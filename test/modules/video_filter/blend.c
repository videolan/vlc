/*****************************************************************************
 * blend.c: test for the video blending module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SIZE 16

static const vlc_fourcc_t destinations[] = {
    VLC_CODEC_RGB555, VLC_CODEC_BGR555, VLC_CODEC_RGB565, VLC_CODEC_BGR565,
    VLC_CODEC_RGB24, VLC_CODEC_BGR24,
    VLC_CODEC_RGBA, VLC_CODEC_ARGB, VLC_CODEC_BGRA, VLC_CODEC_ABGR,
    VLC_CODEC_RGBX, VLC_CODEC_XRGB, VLC_CODEC_BGRX, VLC_CODEC_XBGR,
    VLC_CODEC_I410, VLC_CODEC_I411, VLC_CODEC_YV12, VLC_CODEC_NV12,
    VLC_CODEC_NV21, VLC_CODEC_I420, VLC_CODEC_I422, VLC_CODEC_I444,
#ifdef WORDS_BIGENDIAN
    VLC_CODEC_I420_9B, VLC_CODEC_I420_10B,
    VLC_CODEC_I422_9B, VLC_CODEC_I422_10B, VLC_CODEC_I422_16B,
    VLC_CODEC_I444_9B, VLC_CODEC_I444_10B, VLC_CODEC_I444_16B,
#else
    VLC_CODEC_I420_9L, VLC_CODEC_I420_10L,
    VLC_CODEC_I422_9L, VLC_CODEC_I422_10L, VLC_CODEC_I422_16L,
    VLC_CODEC_I444_9L, VLC_CODEC_I444_10L, VLC_CODEC_I444_16L,
#endif
    VLC_CODEC_YUYV, VLC_CODEC_UYVY, VLC_CODEC_YVYU, VLC_CODEC_VYUY,
};

static const vlc_fourcc_t sources[] = {
    VLC_CODEC_YUVA, VLC_CODEC_RGBA, VLC_CODEC_YUVP,
};

static void Fill(picture_t *pic, int value, int step)
{
    for (int i = 0; i < pic->i_planes; i++)
        memset(pic->p[i].p_pixels, value + step * i,
               pic->p[i].i_lines * pic->p[i].i_pitch);
}

static bool Equal(const picture_t *a, const picture_t *b)
{
    for (int i = 0; i < a->i_planes; i++)
        if (memcmp(a->p[i].p_pixels, b->p[i].p_pixels,
                   a->p[i].i_lines * a->p[i].i_pitch))
            return false;
    return true;
}

static void SetAlpha(picture_t *src, video_palette_t *palette, uint8_t alpha)
{
    plane_t *p;
    switch (src->format.i_chroma)
    {
        case VLC_CODEC_YUVA:
            p = &src->p[3];
            memset(p->p_pixels, alpha, p->i_lines * p->i_pitch);
            break;
        case VLC_CODEC_RGBA:
            p = &src->p[0];
            for (int y = 0; y < p->i_lines; y++)
                for (int x = 3; x < p->i_pitch; x += 4)
                    p->p_pixels[y * p->i_pitch + x] = alpha;
            break;
        default:
            memset(src->p[0].p_pixels, 0, src->p[0].i_lines * src->p[0].i_pitch);
            palette->palette[0][3] = alpha;
            break;
    }
}

static bool Test(vlc_object_t *obj, vlc_fourcc_t dst_chroma, vlc_fourcc_t src_chroma)
{
    video_palette_t palette = { .i_entries = 1, .palette = { { 0xc0, 0x60, 0xa0, 0 } } };

    video_format_t dst_fmt, src_fmt;
    video_format_Init(&dst_fmt, dst_chroma);
    dst_fmt.i_width = dst_fmt.i_visible_width = SIZE;
    dst_fmt.i_height = dst_fmt.i_visible_height = SIZE;
    video_format_Init(&src_fmt, src_chroma);
    src_fmt.i_width = src_fmt.i_visible_width = SIZE;
    src_fmt.i_height = src_fmt.i_visible_height = SIZE;

    picture_t *dst = picture_NewFromFormat(&dst_fmt);
    picture_t *ref = picture_NewFromFormat(&dst_fmt);
    picture_t *src = picture_NewFromFormat(&src_fmt);
    assert(dst != NULL && ref != NULL && src != NULL);
    if (src_chroma == VLC_CODEC_YUVP)
        src_fmt.p_palette = &palette;

    vlc_blender_t *blend = filter_NewBlend(obj, &dst_fmt);
    assert(blend != NULL);
    assert(filter_ConfigureBlend(blend, SIZE, SIZE, &src_fmt) == VLC_SUCCESS);

    Fill(dst, 0x40, 0x20);
    Fill(ref, 0x40, 0x20);
    Fill(src, 0xd0, -0x10);

    SetAlpha(src, &palette, 0);
    assert(filter_Blend(blend, dst, 0, 0, src, 255) == VLC_SUCCESS);
    bool ok = Equal(dst, ref);
    if (!ok)
        fprintf(stderr, "blending transparent %4.4s onto %4.4s changed the picture\n",
                (const char *)&src_chroma, (const char *)&dst_chroma);

    SetAlpha(src, &palette, 255);
    assert(filter_Blend(blend, dst, 0, 0, src, 255) == VLC_SUCCESS);
    assert(!Equal(dst, ref));

    filter_DeleteBlend(blend);
    picture_Release(src);
    picture_Release(ref);
    picture_Release(dst);
    return ok;
}

int main(void)
{
    test_init();

    const char *const args[] = { "-vvv" };
    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);
    if (vlc == NULL)
        return 1;

    unsigned failures = 0;
    for (size_t i = 0; i < ARRAY_SIZE(destinations); i++)
        for (size_t j = 0; j < ARRAY_SIZE(sources); j++)
            if (!Test(VLC_OBJECT(vlc->p_libvlc_int), destinations[i], sources[j]))
                failures++;

    libvlc_release(vlc);
    return failures == 0 ? 0 : 1;
}
