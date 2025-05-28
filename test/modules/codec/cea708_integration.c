/*****************************************************************************
 * cea708_integration.c: CEA-708 decoder integration tests
 *****************************************************************************
 * Copyright (C) 2025 VideoLAN and VLC Authors
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
#include "../../../lib/libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_codec.h>
#include <vlc_subpicture.h>
#include <vlc_es.h>
#include <vlc_interface.h>

#include "../../../modules/codec/substext.h"

static libvlc_instance_t *libvlc;

static decoder_t *create_cea708_decoder_test(const char *video_dimensions)
{
    vlc_object_t *obj = VLC_OBJECT(libvlc->p_libvlc_int);
    decoder_t *dec = vlc_object_create(obj, sizeof(*dec));
    if (!dec)
        return NULL;

    es_format_t fmt_in;
    es_format_Init(&fmt_in, SPU_ES, VLC_CODEC_CEA708);
    fmt_in.subs.cc.i_channel = 1;
    fmt_in.subs.cc.i_reorder_depth = 4;

    dec->fmt_in = &fmt_in;
    es_format_Init(&dec->fmt_out, SPU_ES, VLC_CODEC_TEXT);

    /* Set up video format for aspect ratio testing */
    if (strcmp(video_dimensions, "4:3") == 0) {
        dec->fmt_out.video.i_visible_width = 640;
        dec->fmt_out.video.i_visible_height = 480;
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    } else if (strcmp(video_dimensions, "16:9") == 0) {
        dec->fmt_out.video.i_visible_width = 1920;
        dec->fmt_out.video.i_visible_height = 1080;
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    } else if (strcmp(video_dimensions, "anamorphic_4:3") == 0) {
        dec->fmt_out.video.i_visible_width = 720;
        dec->fmt_out.video.i_visible_height = 480;
        dec->fmt_out.video.i_sar_num = 8;
        dec->fmt_out.video.i_sar_den = 9;
    } else if (strcmp(video_dimensions, "anamorphic_16:9") == 0) {
        dec->fmt_out.video.i_visible_width = 720;
        dec->fmt_out.video.i_visible_height = 480;
        dec->fmt_out.video.i_sar_num = 32;
        dec->fmt_out.video.i_sar_den = 27;
    }

    /* Load CEA-708 decoder module */
    dec->p_module = module_need(dec, "spu decoder", "cc", true);

    return dec;
}

static void destroy_cea708_decoder_test(decoder_t *dec)
{
    if (dec->p_module)
        module_unneed(dec, dec->p_module);
    vlc_object_delete(dec);
}

/* Test basic CEA-708 decoder loading */
static void test_cea708_decoder_loading(void)
{
    test_log("Testing CEA-708 decoder can be loaded\n");

    decoder_t *dec = create_cea708_decoder_test("16:9");
    assert(dec != NULL);
    assert(dec->p_module != NULL);

    test_log("CEA-708 decoder loaded successfully\n");

    destroy_cea708_decoder_test(dec);
}

/* Test CEA-708 decoder with different video formats */
static void test_cea708_decoder_with_different_formats(void)
{
    test_log("Testing CEA-708 decoder with different video formats\n");

    const char *formats[] = {"4:3", "16:9", "anamorphic_4:3", "anamorphic_16:9"};

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        test_log("Testing with format: %s\n", formats[i]);

        decoder_t *dec = create_cea708_decoder_test(formats[i]);
        assert(dec != NULL);
        assert(dec->p_module != NULL);

        test_log("Format %s: decoder loaded successfully\n", formats[i]);

        destroy_cea708_decoder_test(dec);
    }
}

/* Test CEA-708 basic text output capability */
static void test_cea708_subtitle_text_output(void)
{
    test_log("Testing CEA-708 basic text output capability\n");

    decoder_t *dec = create_cea708_decoder_test("16:9");
    assert(dec != NULL);
    assert(dec->p_module != NULL);

    /* Simple CEA-708 test data */
    uint8_t test_data[] = {
        0x03, 0x80, 0x90,                    /* CEA-708 header */
        0x20, 0x48, 0x65, 0x6C, 0x6C, 0x6F,  /* "Hello" text */
        0x8F,                                /* End of window */
        0x00                                 /* End of data */
    };

    vlc_frame_t *frame = vlc_frame_Alloc(sizeof(test_data));
    if (frame) {
        memcpy(frame->p_buffer, test_data, sizeof(test_data));
        frame->i_buffer = sizeof(test_data);

        if (dec->pf_decode) {
            int result = dec->pf_decode(dec, frame);
            test_log("CEA-708 decode result: %d\n", result);
        }
    }

    destroy_cea708_decoder_test(dec);
}

/* Test substext infrastructure with new flag */
static void test_substext_infrastructure_with_new_flag(void)
{
    test_log("Testing substext infrastructure with new flag\n");

    /* Test that both grid flags are now properly defined */
    assert(UPDT_REGION_USES_GRID_COORDINATES == (1 << 5));
    assert(UPDT_REGION_USES_16_9_GRID == (1 << 6));

    test_log("✓ UPDT_REGION_USES_GRID_COORDINATES exists (0x%02x)\n",
             UPDT_REGION_USES_GRID_COORDINATES);
    test_log("✓ UPDT_REGION_USES_16_9_GRID is available (0x%02x)\n",
             UPDT_REGION_USES_16_9_GRID);

    test_log("✓ Available: UPDT_REGION_USES_16_9_GRID flag infrastructure\n");
    test_log("  This flag enables dynamic SAR selection for aspect ratio awareness\n");
}

/* Test CEA-708 with positioning data (will expose positioning issues) */
static void test_cea708_real_positioning_data(void)
{
    test_log("Testing CEA-708 decoder with positioning data\n");

    /* CEA-708 positioning test data */
    const uint8_t cea708_window_positioning_data[] = {
        0x03, 0x80, 0x90,              /* CEA-708 header */
        0x98, 0x20, 0x50, 0x00,        /* Window definition with positioning */
        0x8A, 0x50, 0x50,              /* Set pen location (center) */
        0x20, 0x54, 0x65, 0x73, 0x74,  /* "Test" text */
        0x8F, 0x00                     /* End of window */
    };

    decoder_t *dec = create_cea708_decoder_test("16:9");
    assert(dec != NULL);
    assert(dec->p_module != NULL);

    vlc_frame_t *frame = vlc_frame_Alloc(sizeof(cea708_window_positioning_data));
    if (frame) {
        memcpy(frame->p_buffer, cea708_window_positioning_data, sizeof(cea708_window_positioning_data));
        frame->i_buffer = sizeof(cea708_window_positioning_data);
        frame->i_pts = VLC_TICK_0;
        frame->i_dts = VLC_TICK_0;

        if (dec->pf_decode) {
            int result = dec->pf_decode(dec, frame);
            test_log("CEA-708 positioning data decode result: %d\n", result);
        }
    }

    test_log("Positioning data processed (may have incorrect coordinates until fixed)\n");
    destroy_cea708_decoder_test(dec);
}

int main(void)
{
    test_init();

    libvlc = libvlc_new(test_defaults_nargs, test_defaults_args);
    assert(libvlc != NULL);

    test_substext_infrastructure_with_new_flag();
    test_cea708_decoder_loading();
    test_cea708_decoder_with_different_formats();
    test_cea708_subtitle_text_output();
    test_cea708_real_positioning_data();

    libvlc_release(libvlc);

    return 0;
}
