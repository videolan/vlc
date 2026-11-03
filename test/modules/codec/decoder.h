// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * decoder.h: decoder test driver
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#ifndef VLC_TEST_MODULES_CODEC_DECODER_H
#define VLC_TEST_MODULES_CODEC_DECODER_H

#include <stdio.h>

#include <vlc_common.h>

/* A video decoder module built with ENABLE_TEST defines decoder_tests[], ended
 * by an entry without a name. The driver loads the module and feeds each
 * test's packets through the packetizer and the decoder, with dts = pts, then
 * drains them. check_picture returns the number of failures. */

struct decoder_test_packet
{
    const uint8_t *data;
    size_t size;
};

#define DECODER_TEST_PACKET(p) { (p), sizeof (p) }

struct decoder_test
{
    const char *name;
    vlc_fourcc_t codec;
    const struct decoder_test_packet *packets;
    size_t packet_count;

    unsigned picture_count;
    int (*check_picture)(const picture_t *pic);
};

extern const struct decoder_test decoder_tests[];

#define decoder_test_log(...) fprintf(stderr, "testapi: " __VA_ARGS__)

#endif
