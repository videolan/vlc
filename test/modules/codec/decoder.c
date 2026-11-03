// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * decoder.c: decoder test driver
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

/* Built with the decoder module under test as a static plugin: MODULE_NAME
 * and MODULE_STRING name that module. */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_es.h>
#include "../../src/input/decoder.h"
#include "decoder.h"

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

struct test_run
{
    const struct decoder_test *test;
    unsigned pictures;
    int failures;
};

static void OnPicture(void *opaque, const picture_t *pic)
{
    struct test_run *run = opaque;
    run->failures += run->test->check_picture(pic);
    run->pictures++;
}

static int RunTest(vlc_object_t *parent, const struct decoder_test *test)
{
    struct test_run run = { .test = test };

    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, test->codec);
    decoder_t *dec = test_decoder_create(parent, &fmt, OnPicture, &run);
    es_format_Clean(&fmt);
    if (dec == NULL)
    {
        test_log("FAIL: %s: cannot load the decoder\n", test->name);
        return 1;
    }

    for (size_t i = 0; i < test->packet_count; i++)
    {
        const struct decoder_test_packet *packet = &test->packets[i];
        block_t *block = block_Alloc(packet->size);
        assert(block != NULL);
        memcpy(block->p_buffer, packet->data, packet->size);
        block->i_pts = block->i_dts = VLC_TICK_0 + i * VLC_TICK_FROM_MS(40);
        test_decoder_process(dec, block);
    }
    test_decoder_process(dec, NULL);
    test_decoder_destroy(dec);

    if (run.pictures != test->picture_count)
    {
        test_log("FAIL: %s: %u pictures, expected %u\n", test->name,
                 run.pictures, test->picture_count);
        run.failures++;
    }
    return run.failures != 0;
}

int main(void)
{
    test_init();

    libvlc_instance_t *vlc = libvlc_new(test_defaults_nargs, test_defaults_args);
    assert(vlc != NULL);
    vlc_object_t *parent = VLC_OBJECT(vlc->p_libvlc_int);

    /* Only the module built with the tests, not its plugin */
    var_Create(parent, "codec", VLC_VAR_STRING);
    var_SetString(parent, "codec", MODULE_STRING ",none");

    int ret = 0;
    for (const struct decoder_test *test = decoder_tests;
         test->name != NULL; test++)
    {
        test_log("Testing %s\n", test->name);
        if (RunTest(parent, test))
            ret = 1;
    }

    libvlc_release(vlc);
    return ret;
}
