/*****************************************************************************
 * test/src/video_output/spu.c
 *****************************************************************************
 * Copyright (C) 2019-2026 VideoLabs, VideoLAN and VLC Authors
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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

#include "../../libvlc/test.h"
#include <assert.h>
#include <vlc_common.h>
#include <vlc_spu.h>
#include <vlc_codec.h>
#include <vlc_subpicture.h>
#define MODULE_NAME saletype
#undef VLC_DYNAMIC_PLUGIN
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_vout_display.h>
#include "../lib/libvlc_internal.h"
#include "../../../modules/codec/substext.h"

#define PRINTDIMFSTR "%dx%d+%d,%d sar=%d:%d"
#define PRINTDIMARGS(x) (x).i_visible_width,(x).i_visible_height,(x).i_x_offset,(x).i_y_offset,(x).i_sar_num,(x).i_sar_den
#define PRINTPLACESTR "%dx%d+%d,%d"
#define PRINTPLACEARGS(p) (p).width,(p).height,(p).x,(p).y

#define BAILOUT(run) { fprintf(stderr, "FAILED %s:%d %s\n", __FUNCTION__, __LINE__, run); \
                        goto failed; }
#define EXPECT(foo) do { if(!(foo)) BAILOUT(__FUNCTION__); } while (0)
#define EXPECT_EQ_INT(foo, expected) do { \
    int a__ = (foo); \
    int e__ = (expected); \
    if (a__ != e__) { \
        fprintf(stderr, "FAILED %s:%d: %s == %s, got %d expected %d\n", \
                __FUNCTION__, __LINE__, #foo, #expected, a__, e__); \
        goto failed; \
    } \
} while (0)

enum
{
    INTERNAL_SCALED,
    EXTERNAL_SCALED,
};

/*
 * Main tested issues:
 *
 * + Check that calling spu_Render multiple time for the same PTS won't move
 *   the subtitle (with or without external scaling).
 *
 * + Check that calling spu_Render with or without external scaling gives the
 *   same values.
 */

const char vlc_module_name[] = "spu_test";

static const char *libvlc_argv[] = {
    "-v",
    "--ignore-config",
    "-Idummy",
    "--no-media-library",
    "--text-renderer=saletype"
};

static subpicture_t *subpicture_new(decoder_t *dec,
                                    const subpicture_updater_t *p_dyn)
{
    (void)dec;
    return subpicture_New(p_dyn);
}

static const struct decoder_owner_callbacks dec_subpicture_cbs =
{
    .spu.buffer_new = subpicture_new,
};

static const es_format_t dec_fmt_in =
{
    .i_cat = SPU_ES,
};

static decoder_t dec_subpicture =
{
    .fmt_in = &dec_fmt_in,
    .cbs = &dec_subpicture_cbs,
};

static subpicture_t *create_subtitle(unsigned w, unsigned h, int nb_regions)
{
    video_format_t fmt;
    video_format_Init(&fmt, 0);
    video_format_Setup(&fmt, VLC_CODEC_RGBA, w, h, w, h, 1, 1);

    subpicture_t *sub = decoder_NewSubpicture(&dec_subpicture, NULL);
    if(!sub)
        return NULL;

    while(nb_regions--)
    {
        subpicture_region_t *region = subpicture_region_New(&fmt);
        if(!region)
        {
            subpicture_Delete(sub);
            return NULL;
        }
        vlc_spu_regions_push(&sub->regions, region);

        region->i_x = 0;
        region->i_y = 0;
    }

    sub->i_start = VLC_TICK_0;
    sub->i_stop = VLC_TICK_0 + vlc_tick_from_sec(1);
    sub->b_subtitle = true;

    return sub;
}

/*************************************************************************************************
 * !Text Updater
 ************************************************************************************************/

static subpicture_t *create_text_subtitle(int nb_regions)
{
    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_TEXT);

    subpicture_t *sub = decoder_NewSubpictureText(&dec_subpicture);
    if(!sub)
        return NULL;

    subtext_updater_sys_t *substextupdt = sub->updater.sys;

    substextupdt->margin_ratio = 0.0;
    substextupdt->region.p_segments = text_segment_New("100x100");

    while(--nb_regions)
    {
        substext_updater_region_t *region = SubpictureUpdaterSysRegionNew();
        if(!region)
        {
            subpicture_Delete(sub);
            return NULL;
        }
        SubpictureUpdaterSysRegionAdd(&substextupdt->region, region);
        region->b_absolute = true;
    }

    sub->b_subtitle = true;

    return sub;
}

static const int aligns_sequence[] = {
    0,
    SUBPICTURE_ALIGN_TOP,
    SUBPICTURE_ALIGN_BOTTOM,
    SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT,
    SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_LEFT,
    SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_RIGHT,
    SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT,
    SUBPICTURE_ALIGN_LEFT,
    SUBPICTURE_ALIGN_RIGHT,
};

const vlc_fourcc_t chroma_list[] =
{
    VLC_CODEC_RGBA, 0
};

static void test_reset_SPU(spu_t *spu)
{
    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_RGBA);
    assert(spu_Render(spu, NULL, &fmt, &fmt, true, NULL, INT64_MAX, INT64_MAX, false) == NULL);
}

static int spu_test_scale_x(int v, const video_format_t *fmt_out)
{
    return v * fmt_out->i_sar_den / fmt_out->i_sar_num;
}

static int spu_test_unscale_output_w(const video_format_t *fmt_out)
{
    return fmt_out->i_visible_width * fmt_out->i_sar_num /
           fmt_out->i_sar_den;
}

static int spu_test_place_x(int x, int w, const video_format_t *fmt_out)
{
    int scaled_x = spu_test_scale_x(x, fmt_out);
    // Because A/R can make the SPU exceed the display size, it needs to be clamped
    int clamped_x = spu_test_scale_x(spu_test_unscale_output_w(fmt_out) - w,
                                     fmt_out);

    return __MIN(scaled_x, clamped_x);
}

static int spu_test_aligned_x(int align, int x, int w, int output_w)
{
    if (align & SUBPICTURE_ALIGN_LEFT)
        return x;
    if (align & SUBPICTURE_ALIGN_RIGHT)
        return output_w - w - x;
    return (output_w - w) / 2 + x;
}

static int spu_test_aligned_y(int align, int y, int h, int output_h)
{
    if (align & SUBPICTURE_ALIGN_TOP)
        return y;
    if (align & SUBPICTURE_ALIGN_BOTTOM)
        return output_h - h - y;
    return (output_h - h) / 2 + y;
}

static int expect_rendered_region(const struct subpicture_region_rendered *region,
                                  const video_format_t *fmt_out,
                                  int align,
                                  int x, int y, int w, int h,
                                  float xratio, float yratio,
                                  bool internal_scaled,
                                  int place_dx, int place_dy)
{
    const video_format_t *fmt = &region->p_picture->format;

    // effective transformations
    const unsigned fmt_sar_num = internal_scaled ? fmt_out->i_sar_num : 1;
    const unsigned fmt_sar_den = internal_scaled ? fmt_out->i_sar_den : 1;
    // original width/height scaling factor
    const float fmt_ratio_x = internal_scaled ? xratio : 1;
    const float fmt_ratio_y = internal_scaled ? yratio : 1;

    const int logical_w = w / xratio;
    const int logical_h = h / yratio;
    const int logical_x = spu_test_aligned_x(align, x, w, fmt_out->i_visible_width) / xratio;
    const int logical_y = spu_test_aligned_y(align, y, h, fmt_out->i_visible_height) / yratio;

    EXPECT_EQ_INT(fmt->i_visible_width,  w / fmt_ratio_x * fmt_sar_den / fmt_sar_num);
    EXPECT_EQ_INT(fmt->i_visible_height, h / fmt_ratio_y);
    EXPECT_EQ_INT(fmt->i_x_offset, 0);
    EXPECT_EQ_INT(fmt->i_y_offset, 0);

    EXPECT_EQ_INT(region->place.width,  logical_w * fmt_out->i_sar_den / fmt_out->i_sar_num);
    EXPECT_EQ_INT(region->place.height, logical_h);

    EXPECT_EQ_INT(region->place.x, spu_test_place_x(logical_x, logical_w, fmt_out) + place_dx);
    EXPECT_EQ_INT(region->place.y, logical_y + place_dy);

    return VLC_SUCCESS;

failed:
    return VLC_EGENERIC;
}

#define CHECK_REGION(...) \
    EXPECT(expect_rendered_region(__VA_ARGS__) == VLC_SUCCESS)

static int test_multiple_spu_render(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = fmt_in.i_visible_width = 1920;
    fmt_in.i_height = fmt_in.i_visible_height = 1080;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);

    /* render multiple regions with alignment */
    subpicture_t *subpic = create_subtitle(100, 100, 2);
    if(!subpic)
        return 1;
    subpic->i_start = VLC_TICK_0;
    subpic->i_stop = VLC_TICK_0 + vlc_tick_from_sec(1);
    {
        struct subpicture_region_t *region = vlc_spu_regions_first_or_null(&subpic->regions);
        region->i_align = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT;
        region->b_absolute = false;
        region = vlc_list_first_entry_or_null(&region->node, subpicture_region_t, node);
        region->i_align = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT;
        region->b_absolute = false;
    }
    spu_PutSubpicture(spu, subpic);

    const struct
    {
        unsigned num;
        unsigned den;
    } aspect_ratios[] = {
        { 1, 1 },
        { 4, 3 },
        { 9, 16 }, // vertical!
    };

    vlc_render_subpicture *output = NULL;
    for(unsigned aridx = 0; aridx < ARRAY_SIZE(aspect_ratios); aridx++)
    {
        fmt_out.i_sar_num = aspect_ratios[aridx].num;
        fmt_out.i_sar_den = aspect_ratios[aridx].den;

        for(int i = INTERNAL_SCALED; i<=EXTERNAL_SCALED; i++)
        {
            fprintf(stderr,"\n\ttesting %s scaling\n", i==INTERNAL_SCALED ? "internal" : "external");
            output = spu_Render(spu, i == INTERNAL_SCALED ? NULL : chroma_list,
                                &fmt_out, &fmt_in, true, NULL,
                                VLC_TICK_0, VLC_TICK_0, false);
            EXPECT(output->regions.size == 2);
            const struct subpicture_region_rendered *region0 = output->regions.data[0];
            const video_format_t *fmt0 = &region0->p_picture->format;
            const struct subpicture_region_rendered *region1 = output->regions.data[1];
            const video_format_t *fmt1 = &region1->p_picture->format;

            fprintf(stderr, "region0 " PRINTDIMFSTR "\n", PRINTDIMARGS(*fmt0));
            fprintf(stderr, " at place " PRINTPLACESTR "\n", PRINTPLACEARGS(region0->place));
            fprintf(stderr, "region1 " PRINTDIMFSTR "\n", PRINTDIMARGS(*fmt1));
            fprintf(stderr, " at place " PRINTPLACESTR "\n", PRINTPLACEARGS(region1->place));

            CHECK_REGION(region0, &fmt_out,
                         SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT,
                         0, 0, 100, 100,
                         1.0, 1.0, (i == INTERNAL_SCALED),
                         +0, +0);

            CHECK_REGION(region1, &fmt_out,
                         SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT,
                         0, 0, 100, 100,
                         1.0, 1.0, (i == INTERNAL_SCALED),
                         +0, -100); // displacement because packed against region 0

            vlc_render_subpicture_Delete(output);
            output = NULL;
        }
    }

    return 0;

failed:
    if(output)
        vlc_render_subpicture_Delete(output);
    return 1;
}

static int test_spu_scaling(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = 2000;
    fmt_in.i_visible_width = 1920;
    fmt_in.i_height = 1100;
    fmt_in.i_visible_height = 1080;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);

    const vout_display_place_t video_position = {fmt_out.i_x_offset,
                                                 fmt_out.i_y_offset,
                                                 fmt_out.i_visible_width,
                                                 fmt_out.i_visible_height};

    vlc_render_subpicture *output0 = NULL;
    vlc_render_subpicture *output1 = NULL;

    {
        subpicture_t *subpic = create_subtitle(100, 100, 1);
        EXPECT(subpic);
        spu_PutSubpicture(spu, subpic);
        output0 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, &video_position,
                             VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output0);
        EXPECT(output0->regions.size == 1);

        test_reset_SPU(spu);

        subpic = create_subtitle(100, 100, 1);
        EXPECT(subpic);
        spu_PutSubpicture(spu, subpic);
        output1 = spu_Render(spu, chroma_list, &fmt_out, &fmt_in,
                            true, &video_position,
                            VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output1);
        EXPECT(output1->regions.size == 1);

        const struct subpicture_region_rendered *region1, *region2;
        region1 = output0->regions.data[0];
        region2 = output1->regions.data[0];

        /* Without external scaling, a non-1:1 aspect ratio would scale the image to
         * have a 1:1 aspect ratio, as outputs can't handle scale.
         * However, aspect ratio can be different that 1:1 with external scaling, as
         * subtitles can be rasterized at a different aspect ratio.
         * See substext.h for example. */
        const video_format_t *fmt1 = &region1->p_picture->format;
        const video_format_t *fmt2 = &region2->p_picture->format;
        EXPECT(fmt1->i_sar_num == fmt2->i_sar_den);

        fprintf(stderr, "place " PRINTPLACESTR, PRINTPLACEARGS(region1->place));
        fprintf(stderr, "output0 " PRINTDIMFSTR, PRINTDIMARGS(*fmt1));
        fprintf(stderr, "place " PRINTPLACESTR, PRINTPLACEARGS(region2->place) );
        fprintf(stderr, "output1 " PRINTDIMFSTR, PRINTDIMARGS(*fmt2));

        /* There should be no differences for subpicture rendering whether external
     * scaling is enabled or not. */
        EXPECT(fmt1->i_x_offset == fmt2->i_x_offset && fmt1->i_y_offset == fmt2->i_y_offset);
        EXPECT(region1->place.x == region2->place.x && region1->place.y == region2->place.y);

        vlc_render_subpicture_Delete(output0);
        output0 = NULL;
        vlc_render_subpicture_Delete(output1);
        output1 = NULL;
    }

    test_reset_SPU(spu);

    /*
     * Check that original picture dimensions (the ones the SPU were designed against).
     * The SPU can come from other source, or the video can be encoded to the non native res.
     */
    const struct
    {
        unsigned num;
        unsigned den;
    } aspect_ratios[] = {
        { 1, 1 },
        { 4, 3 },
        { 16, 9 },
    };

    for(unsigned aridx = 0; aridx < ARRAY_SIZE(aspect_ratios); aridx++)
    {
        float xratio = 0.5, yratio = 0.5;

        fmt_out.i_sar_num = aspect_ratios[aridx].num;
        fmt_out.i_sar_den = aspect_ratios[aridx].den;

        for(int i = INTERNAL_SCALED; i<=EXTERNAL_SCALED; i++)
        {
            subpicture_t *subpic = create_subtitle(100, 100, 1);
            EXPECT(subpic);
            subpic->i_original_picture_width = fmt_in.i_visible_width * xratio;
            subpic->i_original_picture_height = fmt_in.i_visible_height * yratio;

            struct subpicture_region_t *srcregion = vlc_spu_regions_first_or_null(&subpic->regions);
            if(!srcregion)
            {
                subpicture_Delete(subpic);
                goto failed;
            }
            srcregion->i_align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
            srcregion->i_x = 10;
            srcregion->i_y = 10;

            spu_PutSubpicture(spu, subpic);

            fprintf(stderr,"\n\ttesting %s original picture dimensions\n",
                    i==INTERNAL_SCALED ? "internal" : "external");
            output0 = spu_Render(spu, i == INTERNAL_SCALED ? NULL : chroma_list,
                                 &fmt_out, &fmt_in, true, NULL,
                                 VLC_TICK_0, VLC_TICK_0, false);
            const struct subpicture_region_rendered *region = output0->regions.data[0];
            const video_format_t *fmt = &region->p_picture->format;

            fprintf(stderr, PRINTDIMFSTR "\n", PRINTDIMARGS(*fmt));
            fprintf(stderr, " at place " PRINTPLACESTR "\n", PRINTPLACEARGS(region->place));

            CHECK_REGION(region, &fmt_out,
                         SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT,
                         10, 10, 100, 100,
                         xratio, yratio, (i == INTERNAL_SCALED),
                         +0, +0);

            vlc_render_subpicture_Delete(output0);
            output0 = NULL;
            test_reset_SPU(spu);
        }

    }

    /* Check that destination forced A/R is applied, ie, we compensate */
    fprintf(stderr, "\nchecking destination A/R\n");

    for(unsigned aridx = 1; aridx < ARRAY_SIZE(aspect_ratios); aridx++)
    {
        float xratio = 1.0, yratio = 1.0;

        fmt_out.i_sar_num = aspect_ratios[aridx].num;
        fmt_out.i_sar_den = aspect_ratios[aridx].den;

        for(int i = INTERNAL_SCALED; i<=EXTERNAL_SCALED; i++)
        {
            subpicture_t *subpic = create_subtitle(100, 100, 1);
            EXPECT(subpic);

            struct subpicture_region_t *srcregion = vlc_spu_regions_first_or_null(&subpic->regions);
            if(!srcregion)
            {
                subpicture_Delete(subpic);
                goto failed;
            }
            srcregion->i_align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
            srcregion->i_x = 10;
            srcregion->i_y = 10;

            spu_PutSubpicture(spu, subpic);

            fprintf(stderr,"\n\ttesting %s scaling\n", i==INTERNAL_SCALED ? "internal" : "external");
            output0 = spu_Render(spu, i == INTERNAL_SCALED ? NULL : chroma_list,
                                 &fmt_out, &fmt_in, true, NULL,
                                 VLC_TICK_0, VLC_TICK_0, false);
            EXPECT(output0);
            const struct subpicture_region_rendered *region = output0->regions.data[0];
            const video_format_t *fmt = &region->p_picture->format;

            fprintf(stderr, "\t" PRINTDIMFSTR "\n", PRINTDIMARGS(*fmt));
            fprintf(stderr, "\t at place " PRINTPLACESTR "\n", PRINTPLACEARGS(region->place));

            CHECK_REGION(region, &fmt_out,
                         SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT,
                         10, 10, 100, 100,
                         xratio, yratio, (i == INTERNAL_SCALED),
                         +0, +0);

            vlc_render_subpicture_Delete(output0);
            output0 = NULL;
            test_reset_SPU(spu);
        }

    }

    return 0;

failed:
    if(output0)
        vlc_render_subpicture_Delete(output0);
    if(output1)
        vlc_render_subpicture_Delete(output1);
    return 1;
}

static int test_spu_align(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = fmt_in.i_visible_width = 1920;
    fmt_in.i_height = fmt_in.i_visible_height = 1080;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);

    /* Check all aligns */
    vlc_render_subpicture *output0 = NULL;
    fmt_out.i_sar_num = 1; fmt_out.i_sar_den = 1;
    for(unsigned j = 0; j<ARRAY_SIZE(aligns_sequence); j++)
    {
        fprintf(stderr, "\n\nchecking align flags %x\n", aligns_sequence[j]);
        for(int i = INTERNAL_SCALED; i<=EXTERNAL_SCALED; i++)
        {
            subpicture_t *subpic = create_subtitle(100, 100, 1);
            EXPECT(subpic);

            unsigned offset_x = j % 3 ? 10 : 0, offset_y = j % 4 ? 10 : 0;

            struct subpicture_region_t *srcregion = vlc_spu_regions_first_or_null(&subpic->regions);
            if(!srcregion)
            {
                subpicture_Delete(subpic);
                return 1;
            }
            srcregion->b_absolute = false;
            srcregion->i_align = aligns_sequence[j]; /* change our align */
            srcregion->i_x = offset_x;
            srcregion->i_y = offset_y;

            spu_PutSubpicture(spu, subpic);

            fprintf(stderr,"\ttesting %s scaling\n", i==INTERNAL_SCALED ? "internal" : "external");
            output0 = spu_Render(spu, i == INTERNAL_SCALED ? NULL : chroma_list,
                                 &fmt_out, &fmt_in, true, NULL,
                                 VLC_TICK_0, VLC_TICK_0, false);
            EXPECT(output0);

            const struct subpicture_region_rendered *region = output0->regions.data[0];
            const video_format_t *fmt = &region->p_picture->format;

            fprintf(stderr, "\t" PRINTDIMFSTR "\n", PRINTDIMARGS(*fmt));
            fprintf(stderr, "\t at place " PRINTPLACESTR "\n", PRINTPLACEARGS(region->place));

            CHECK_REGION(region, &fmt_out,
                         aligns_sequence[j],
                         offset_x, offset_y, 100, 100,
                         1.0, 1.0, (i == INTERNAL_SCALED),
                         +0, +0);

            vlc_render_subpicture_Delete(output0);
            output0 = NULL;
            test_reset_SPU(spu);
        }
    }

    return 0;

failed:
    if(output0)
        vlc_render_subpicture_Delete(output0);
    return 1;
}

static int test_text_rendering(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = fmt_in.i_visible_width = 1920;
    fmt_in.i_height = fmt_in.i_visible_height = 1080;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);

    vlc_render_subpicture *output0 = NULL;
    fmt_out.i_sar_num = 1; fmt_out.i_sar_den = 1;
    for(unsigned j = 0; j<ARRAY_SIZE(aligns_sequence); j++)
    {
        for(int i = INTERNAL_SCALED; i<=EXTERNAL_SCALED; i++)
        {
            subpicture_t *subpic = create_text_subtitle(1);
            EXPECT(subpic);
            subpic->i_start = VLC_TICK_0;
            subpic->i_stop = VLC_TICK_0 + vlc_tick_from_sec(1);
            subtext_updater_sys_t *updtsys = subpic->updater.sys;
            substext_updater_region_t *updtregion = &updtsys->region;

            /* change our align */
            updtregion->align = aligns_sequence[j];
            updtregion->origin.x = j % 3 ? 10 : 0;
            updtregion->origin.y = j % 4 ? 10 : 0;

            fprintf(stderr, "\n\nchecking with updater align flags %x origin %f %f\n", updtregion->align, updtregion->origin.x, updtregion->origin.y);

            /* Ensure it will be re-rendered by updater */
            vlc_spu_regions_Clear(&subpic->regions);
            updtregion->flags &= ~UPDT_REGION_FIXED_DONE;
            updtregion->b_absolute = false;

            spu_PutSubpicture(spu, subpic);

            fprintf(stderr,"\t%d origin %f %f absolute %d\n", __LINE__, updtregion->origin.x, updtregion->origin.y, updtregion->b_absolute);
            fprintf(stderr,"\ttesting %s scaling\n", i==INTERNAL_SCALED ? "internal" : "external");
            output0 = spu_Render(spu, i == INTERNAL_SCALED ? NULL : chroma_list,
                                 &fmt_out, &fmt_in, true, NULL,
                                 VLC_TICK_0, VLC_TICK_0, false);
            EXPECT(output0);
            EXPECT(output0->regions.size);
            fprintf(stderr,"\t%d origin %f %f\n", __LINE__, updtregion->origin.x, updtregion->origin.y);
            const struct subpicture_region_rendered *region0 = output0->regions.data[0];
            const video_format_t *fmt0 = &region0->p_picture->format;

            fprintf(stderr, "\tregion0 " PRINTDIMFSTR "\n", PRINTDIMARGS(*fmt0));
            fprintf(stderr, "\t at place " PRINTPLACESTR "\n", PRINTPLACEARGS(region0->place));

            CHECK_REGION(region0, &fmt_out,
                         aligns_sequence[j],
                         j % 3 ? 10 : 0, j % 4 ? 10 : 0, 100, 100,
                         1.0, 1.0, (i == INTERNAL_SCALED),
                         +0, +0);

            vlc_render_subpicture_Delete(output0);
            output0 = NULL;
            test_reset_SPU(spu);
        }
    }

    return 0;

failed:
    if(output0)
        vlc_render_subpicture_Delete(output0);
    return 1;
}

static int test_spu_rescaling(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = 2000;
    fmt_in.i_visible_width = 1920;
    fmt_in.i_height = 1200;
    fmt_in.i_visible_height = 1080;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);

    vlc_render_subpicture *output0 = NULL;
    vlc_render_subpicture *output1 = NULL;
    {
        subpicture_t *subpic = create_subtitle(100, 100, 1);
        EXPECT(subpic);
        struct subpicture_region_t *srcregion = vlc_spu_regions_first_or_null(&subpic->regions);
        if(!srcregion)
        {
            subpicture_Delete(subpic);
            return 1;
        }
        srcregion->i_x = 50;
        srcregion->i_y = 50;
        spu_PutSubpicture(spu, subpic);

        output0 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, NULL, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output0);
        EXPECT(output0->regions.size);

        fmt_out.i_visible_width = 1920/10;
        fmt_out.i_visible_height = 1080/10;

        output1 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, NULL, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output1);
        EXPECT(output1->regions.size);

        vlc_render_subpicture_Delete(output0);
        output0 = NULL;
        vlc_render_subpicture_Delete(output1);
        output1 = NULL;
    }

    return 0;

failed:
    if(output0)
        vlc_render_subpicture_Delete(output0);
    if(output1)
        vlc_render_subpicture_Delete(output1);
    return 1;
}

static int test_spu_rescaling_text(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = 1100;
    fmt_in.i_visible_width = 1000;
    fmt_in.i_height = 600;
    fmt_in.i_visible_height = 500;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);
    fmt_out.i_width = 2100;
    fmt_out.i_visible_width = 2000;
    fmt_out.i_height = 1100;
    fmt_out.i_visible_height = 1000;

    vlc_render_subpicture *output0 = NULL;
    vlc_render_subpicture *output1 = NULL;

    /* Rescaling for text is tricky as the rendered region size will only be
     * decided by the text_renderer.
     * - Text is usually rendered at *destination* size if bigger than source
     * - Otherwise it is rendered at source resolution, but rescaled (we're <= source size)
     * Scaling then does not apply to the text_renderer output size difference but it should
     * apply for A/R compensation with the target display. (but A/R can depend on size diff :/)
     */
    {
        subpicture_t *subpic = create_text_subtitle(1);
        assert(subpic);
        subpic->i_start = VLC_TICK_0;
        subpic->i_stop = VLC_TICK_0 + vlc_tick_from_sec(1);
        subtext_updater_sys_t *updtsys = subpic->updater.sys;
        substext_updater_region_t *updtregion = &updtsys->region;
        updtregion->align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
        updtregion->origin.x = .05;
        updtregion->origin.y = .05;
        updtregion->flags |= UPDT_REGION_ORIGIN_X_IS_RATIO|UPDT_REGION_ORIGIN_Y_IS_RATIO;
        updtregion->b_absolute = false;

        spu_PutSubpicture(spu, subpic);

        output0 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, NULL, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output0);
        EXPECT(output0->regions.size);
        const struct subpicture_region_rendered *region0 = output0->regions.data[0];

        EXPECT_EQ_INT(region0->place.x, fmt_out.i_visible_width * 0.05);
        EXPECT_EQ_INT(region0->place.y, fmt_out.i_visible_height * 0.05);

        // resize of dest window
        fmt_out.i_visible_width /= 4;
        fmt_out.i_visible_height /= 4;

        output1 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, NULL, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output1);
        EXPECT(output1->regions.size);
        const struct subpicture_region_rendered *region1 = output1->regions.data[0];

        EXPECT_EQ_INT(region1->place.x, fmt_out.i_visible_width * 0.05);
        EXPECT_EQ_INT(region1->place.y, (unsigned)(fmt_out.i_visible_height * 0.05));
        EXPECT_EQ_INT(region1->place.x, region0->place.x /4);
        EXPECT_EQ_INT(region1->place.y, region0->place.y /4);
        EXPECT_EQ_INT(region1->place.width, region0->place.width);
        EXPECT_EQ_INT(region1->place.height, region0->place.height);

        vlc_render_subpicture_Delete(output0);
        output0 = NULL;
        vlc_render_subpicture_Delete(output1);
        output1 = NULL;
    }

    {
        /* Render on a wider area / video_position + in_window */
        const vout_display_place_t video_position = {10,
                                                     10,
                                                     fmt_out.i_visible_width * 2,
                                                     fmt_out.i_visible_height * 2};

        subpicture_t *subpic = create_text_subtitle(1);
        assert(subpic);
        subpic->i_start = VLC_TICK_0;
        subpic->i_stop = VLC_TICK_0 + vlc_tick_from_sec(1);
        subtext_updater_sys_t *updtsys = subpic->updater.sys;
        substext_updater_region_t *updtregion = &updtsys->region;
        updtregion->align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
        updtregion->origin.x = .05;
        updtregion->origin.y = .05;
        updtregion->flags |= UPDT_REGION_ORIGIN_X_IS_RATIO|UPDT_REGION_ORIGIN_Y_IS_RATIO;
        updtregion->b_absolute = false;

        spu_PutSubpicture(spu, subpic);

        updtregion->b_in_window = true;
        output0 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, &video_position, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output0);
        EXPECT(output0->regions.size);
        const struct subpicture_region_rendered *region0 = output0->regions.data[0];

        EXPECT_EQ_INT(region0->place.x, 10 + (unsigned)(fmt_out.i_visible_width * 0.05) * 2);
        EXPECT_EQ_INT(region0->place.y, 10 + (unsigned)(fmt_out.i_visible_height * 0.05) * 2);
        EXPECT_EQ_INT(region0->place.width, 100 * 2);
        EXPECT_EQ_INT(region0->place.height, 100 * 2);

        vlc_render_subpicture_Delete(output0);
        output0 = NULL;

        /* video_position but not in_window */
        updtregion->b_in_window = false;
        output1 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, &video_position, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output1);
        EXPECT(output1->regions.size);
        const struct subpicture_region_rendered *region1 = output1->regions.data[0];

        EXPECT_EQ_INT(region1->place.x, 10 + video_position.width * 5/100);
        EXPECT_EQ_INT(region1->place.y, 10 + video_position.height * 5/100);
        EXPECT_EQ_INT(region1->place.width, 100);
        EXPECT_EQ_INT(region1->place.height, 100);

        vlc_render_subpicture_Delete(output1);
        output1 = NULL;
    }

    return 0;

failed:
    if(output0)
        vlc_render_subpicture_Delete(output0);
    if(output1)
        vlc_render_subpicture_Delete(output1);
    return 1;
}

static int test_spu_crop(spu_t *spu)
{
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_in.i_width = fmt_in.i_visible_width = 1000;
    fmt_in.i_height = fmt_in.i_visible_height = 500;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;
    video_format_Copy(&fmt_out, &fmt_in);

    vlc_render_subpicture *output0 = NULL;
    vlc_render_subpicture *output1 = NULL;

    {
        subpicture_t *subpic = create_subtitle(100, 100, 1);
        EXPECT(subpic);

        struct subpicture_region_t *srcregion =
            vlc_spu_regions_first_or_null(&subpic->regions);
        if(!srcregion)
        {
            subpicture_Delete(subpic);
            goto failed;
        }
        srcregion->i_align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
        srcregion->i_x = 20;
        srcregion->i_y = 30;
        srcregion->i_max_width = 40;
        srcregion->i_max_height = 25;

        spu_PutSubpicture(spu, subpic);

        output0 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, NULL, VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output0);
        EXPECT(output0->regions.size == 1);

        const struct subpicture_region_rendered *region = output0->regions.data[0];
        const video_format_t *fmt = &region->p_picture->format;

        EXPECT_EQ_INT(fmt->i_x_offset, 0);
        EXPECT_EQ_INT(fmt->i_y_offset, 0);
        EXPECT_EQ_INT(fmt->i_visible_width, 40);
        EXPECT_EQ_INT(fmt->i_visible_height, 25);
        EXPECT_EQ_INT(region->place.x, 20);
        EXPECT_EQ_INT(region->place.y, 30);
        EXPECT_EQ_INT(region->place.width, 40);
        EXPECT_EQ_INT(region->place.height, 25);

        vlc_render_subpicture_Delete(output0);
        output0 = NULL;
    }

    test_reset_SPU(spu);

    {
        fmt_in.i_width = fmt_in.i_visible_width = 200;
        fmt_in.i_height = fmt_in.i_visible_height = 100;
        video_format_Copy(&fmt_out, &fmt_in);
        fmt_out.i_width = fmt_out.i_visible_width = 400;
        fmt_out.i_height = fmt_out.i_visible_height = 200;

        const vout_display_place_t video_position = {10, 20, 200, 100};

        subpicture_t *subpic = create_subtitle(20, 10, 1);
        EXPECT(subpic);

        struct subpicture_region_t *srcregion =
            vlc_spu_regions_first_or_null(&subpic->regions);
        if(!srcregion)
        {
            subpicture_Delete(subpic);
            goto failed;
        }
        srcregion->i_align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
        srcregion->i_x = 5;
        srcregion->i_y = 6;
        srcregion->b_in_window = false;

        spu_PutSubpicture(spu, subpic);

        output0 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, &video_position,
                             VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output0);
        EXPECT(output0->regions.size == 1);
        const struct subpicture_region_rendered *region0 = output0->regions.data[0];

        EXPECT_EQ_INT(region0->place.x, 15);
        EXPECT_EQ_INT(region0->place.y, 26);
        EXPECT_EQ_INT(region0->place.width, 20);
        EXPECT_EQ_INT(region0->place.height, 10);

        srcregion->b_in_window = true;

        output1 = spu_Render(spu, NULL, &fmt_out, &fmt_in,
                             true, &video_position,
                             VLC_TICK_0, VLC_TICK_0, false);
        EXPECT(output1);
        EXPECT(output1->regions.size == 1);
        const struct subpicture_region_rendered *region1 = output1->regions.data[0];

        EXPECT_EQ_INT(region1->place.x, 10);
        EXPECT_EQ_INT(region1->place.y, 12);
        EXPECT_EQ_INT(region1->place.width, 40);
        EXPECT_EQ_INT(region1->place.height, 20);
    }

    vlc_render_subpicture_Delete(output0);
    vlc_render_subpicture_Delete(output1);
    return 0;

failed:
    if(output0)
        vlc_render_subpicture_Delete(output0);
    if(output1)
        vlc_render_subpicture_Delete(output1);
    return 1;
}

int main(int argc, char **argv)
{
    test_init();

    (void)argc; (void)argv;

    libvlc_instance_t *libvlc = libvlc_new(ARRAY_SIZE(libvlc_argv), libvlc_argv);
    assert(libvlc);

    spu_t *spu = spu_Create(&libvlc->p_libvlc_int->obj, NULL);
    assert(spu);

    int ret = test_spu_scaling(spu);

    /* flush SPU before running other test */
    test_reset_SPU(spu);

    ret |= test_multiple_spu_render(spu);

    /* flush SPU before running other test */
    test_reset_SPU(spu);

    ret |= test_text_rendering(spu);

    /* flush SPU before running other test */
    test_reset_SPU(spu);

    ret |= test_spu_align(spu);

    /* flush SPU before running other test */
    test_reset_SPU(spu);

    ret |= test_spu_rescaling(spu);

    /* flush SPU before running other test */
    test_reset_SPU(spu);

    ret |= test_spu_rescaling_text(spu);

    /* flush SPU before running other test */
    test_reset_SPU(spu);

    ret |= test_spu_crop(spu);

    spu_Destroy(spu);

    libvlc_release(libvlc);

    return ret;
}

/*************************************************************************************************
 * Fake Renderer
 ************************************************************************************************/

static void renderer_Destroy( filter_t *p_filter )
{
    VLC_UNUSED(p_filter);
}

static subpicture_region_t *renderer_Render( filter_t *p_filter,
                                             const subpicture_region_t *p_region_in,
                                             const vlc_fourcc_t *p_chroma_list )
{
    VLC_UNUSED(p_filter); VLC_UNUSED(p_chroma_list);
    unsigned w,h;
    assert(sscanf(p_region_in->p_text->psz_text, "%ux%u", &w, &h) == 2);

    video_format_t fmt;
    video_format_Init(&fmt, 0);
    video_format_Setup(&fmt, VLC_CODEC_RGBA, w, h, w, h, 1, 1);
    subpicture_region_t *r = subpicture_region_New(&fmt);
    r->b_absolute = p_region_in->b_absolute;
    r->i_x = p_region_in->i_x;
    r->i_y = p_region_in->i_y;
    r->i_align = p_region_in->i_align;
    return r;
}

static const struct vlc_filter_operations renderer_ops =
{
        .render = renderer_Render, .close = renderer_Destroy,
};

static int renderer_Create( filter_t *p_filter )
{
    p_filter->ops = &renderer_ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_subcategory(SUBCAT_VIDEO_SUBPIC)
    add_shortcut("saletype")
    set_callback_text_renderer(renderer_Create, 0)
    add_submodule ()
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

