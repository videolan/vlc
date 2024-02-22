/*****************************************************************************
 * xiph_test.c: Xiph unit tests
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs, VideoLAN and VLC Authors
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
#include <vlc_codec.h>

#include "xiph.h"

static const uint8_t xiph0[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
static const uint8_t xiph1[] = { 0x02, 0x05, 0x01,
                                 0x01, 0x02, 0x03, 0x04, 0x05,
                                 0x01, 0x0A, 0x0B };
static const uint8_t xiphlavc0[] = { 0x00,   30,
                                     0x01, 0x02, 0x03, 0x04, 0x05,
                                     0x06, 0x07, 0x08, 0x09, 0x0A,
                                     0x01, 0x02, 0x03, 0x04, 0x05,
                                     0x06, 0x07, 0x08, 0x09, 0x0A,
                                     0x01, 0x02, 0x03, 0x04, 0x05,
                                     0x06, 0x07, 0x08, 0x09, 0x0A,
                                     0x00, 0x01,
                                     0x01,
                                     0x00, 0x00 };

struct params_s
{
    const void *packets[XIPH_MAX_HEADER_COUNT];
    size_t packets_sizes[XIPH_MAX_HEADER_COUNT];
    size_t packets_count;
    bool lavc;
    vlc_fourcc_t codec;
    void *p_append;
    size_t i_append;
};

#define BAILOUT(run) { fprintf(stderr, "failed %s line %d\n", run, __LINE__); \
                        return 1; }
#define RUN(run, test, a, b, res) \
    if((!test(#test " " run, a, b, &params)) != res) \
       BAILOUT(#test " " run)
#define PASS(run, test, a, b) RUN(run, test, a, b, true)
#define FAIL(run, test, a, b) RUN(run, test, a, b, false)
#define EXPECT(foo) if(!(foo)) BAILOUT(run)
#define EXPECT_CLEANUP(foo, cleanup) if(!(foo)) { cleanup; BAILOUT(run) }

static int test_xiph_IsLavcFormat(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    EXPECT(xiph_IsLavcFormat(p_extra, i_extra, source->codec) == source->lavc);
    return 0;
}

static int test_xiph_CountHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    EXPECT(xiph_CountHeaders(p_extra, i_extra) == source->packets_count);
    return 0;
}

static int test_xiph_CountLavcHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    EXPECT(xiph_CountLavcHeaders(p_extra, i_extra) == source->packets_count);
    return 0;
}

static int SplitCompare(const char *run,
                        size_t packet_size[],
                        const void *packet[], size_t packet_count,
                        const struct params_s *source)
{
    EXPECT(source->packets_count == packet_count);
    for(unsigned i=0; i<packet_count; i++)
    {
        EXPECT(source->packets[i] == packet[i]);
        EXPECT(source->packets_sizes[i] == packet_size[i]);
    }
    return VLC_SUCCESS;
}

static int test_xiph_SplitHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    const void *packets[XIPH_MAX_HEADER_COUNT];
    size_t packet_sizes[XIPH_MAX_HEADER_COUNT];
    size_t packet_count;
    int ret = xiph_SplitHeaders(packet_sizes, packets, &packet_count,
                                i_extra, p_extra);
    if(ret == VLC_SUCCESS)
        ret = SplitCompare(run, packet_sizes, packets, packet_count, source);
    return ret;
}

static int test_xiph_SplitLavcHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    const void *packets[XIPH_MAX_HEADER_COUNT];
    size_t packet_sizes[XIPH_MAX_HEADER_COUNT];
    size_t packet_count;
    int ret = xiph_SplitLavcHeaders(packet_sizes, packets, &packet_count,
                                    i_extra, p_extra);
    if(ret == VLC_SUCCESS)
        ret = SplitCompare(run, packet_sizes, packets, packet_count, source);
    return ret;
}

static int test_xiph_PackHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 struct params_s *source)
{
    void *p_result;
    size_t i_result;

    int ret = xiph_PackHeaders(&i_result, &p_result,
                               source->packets_sizes,
                               source->packets, source->packets_count);
    if(ret == VLC_SUCCESS)
    {
        EXPECT_CLEANUP((i_extra == (unsigned)i_result), free(p_result));
        EXPECT_CLEANUP(!memcmp(p_extra, p_result, i_extra), free(p_result));
        free(p_result);
    }
    return ret;
}

static int test_xiph_AppendHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 struct params_s *source)
{
    int ret = xiph_AppendHeaders(&source->i_append, &source->p_append,
                                 source->packets_sizes[0], source->packets[0]);
    if(ret == VLC_SUCCESS)
    {
        EXPECT_CLEANUP((i_extra == (unsigned)source->i_append),
                       free(source->p_append));
        EXPECT_CLEANUP(!memcmp(p_extra, source->p_append, source->i_append),
                       free(source->p_append));
    }
    return ret;
}

int main(void)
{
    struct params_s params;

    params.lavc = false;
    params.codec = VLC_CODEC_VORBIS;

    /* check if we can detect lavc format */
    PASS("0", test_xiph_IsLavcFormat, xiph0, 0);
    PASS("1", test_xiph_IsLavcFormat, xiph0, 1);
    PASS("2", test_xiph_IsLavcFormat, xiph0, 2);
    PASS("3", test_xiph_IsLavcFormat, xiph0, 6);
    PASS("lavc0", test_xiph_IsLavcFormat, xiph0, 0);
    PASS("lavc1", test_xiph_IsLavcFormat, xiph0, 1);
    PASS("lavc2", test_xiph_IsLavcFormat, xiph0, 6);
    PASS("lavc3", test_xiph_IsLavcFormat, xiphlavc0, 0);
    PASS("lavc4", test_xiph_IsLavcFormat, xiphlavc0, 1);
    params.lavc = true;
    PASS("lavc5", test_xiph_IsLavcFormat, xiphlavc0, 37);
    params.codec = 0;
    params.lavc = false;
    PASS("lavc6", test_xiph_IsLavcFormat, xiphlavc0, 37);

    /* check count and return 0 on error */
    params.packets_count = 0;
    PASS("0", test_xiph_CountHeaders, xiph0, 0);
    params.packets_count = 1;
    PASS("1", test_xiph_CountHeaders, xiph0, 1);
    params.packets_count = 3;
    PASS("2", test_xiph_CountHeaders, xiph1, 11);

    /* check lavc only valid with count == 3 */
    params.packets_count = 3;
    params.codec = VLC_CODEC_VORBIS;
    PASS("lavc0", test_xiph_CountLavcHeaders, xiphlavc0, 37);
    params.packets_count = 0;
    PASS("lavc1", test_xiph_CountLavcHeaders, xiphlavc0, 35);
    PASS("lavc2", test_xiph_CountLavcHeaders, xiphlavc0, 0);

    /* check split on single/trail packet (no index) */
    params.packets[0] = &xiph0[1];
    params.packets_sizes[0] = 5;
    params.packets_count = 1;
    PASS("0", test_xiph_SplitHeaders, xiph0, 6);
    params.packets_sizes[0] = 0;
    PASS("1", test_xiph_SplitHeaders, xiph0, 1);

    /* check split */
    params.packets_count = 3;
    params.packets[0] = &xiph1[3];
    params.packets_sizes[0] = 5;
    params.packets[1] = &xiph1[8];
    params.packets_sizes[1] = 1;
    params.packets[2] = &xiph1[9];
    params.packets_sizes[2] = 2;
    PASS("2", test_xiph_SplitHeaders, xiph1, 11);
    FAIL("3", test_xiph_SplitHeaders, xiph1, 7);

    /* check variable length decoding */
    uint8_t xiph2[265];
    memset(xiph2, 0xFF, 265);
    FAIL("4", test_xiph_SplitHeaders, xiph2, 265);
    xiph2[0] = 1;
    FAIL("5", test_xiph_SplitHeaders, xiph2, 265);
    xiph2[2] = 1;
    params.packets_count = 2;
    params.packets[0] = &xiph2[3];
    params.packets_sizes[0] = 256;
    params.packets[1] = &xiph2[3+256];
    params.packets_sizes[1] = 6;
    PASS("6", test_xiph_SplitHeaders, xiph2, 265);
    /* /!\ xiph2 content reused in another test below */

    /* check lavc split */
    params.packets_count = 3;
    params.packets[0] = &xiphlavc0[2];
    params.packets_sizes[0] = 30;
    params.packets[1] = &xiphlavc0[34];
    params.packets_sizes[1] = 1;
    params.packets[2] = &xiphlavc0[37];
    params.packets_sizes[2] = 0;
    PASS("lavc0", test_xiph_SplitLavcHeaders, xiphlavc0, 37);
    FAIL("lavc1", test_xiph_SplitLavcHeaders, xiphlavc0, 36);
    FAIL("lavc2", test_xiph_SplitLavcHeaders, xiphlavc0, 31);

    /* Test single packet packing */
    params.packets_count = XIPH_MAX_HEADER_COUNT + 1;
    FAIL("0", test_xiph_PackHeaders, xiph0, 6);
    params.packets_count = 1;
    params.packets[0] = &xiph0[1];
    params.packets_sizes[0] = 5;
    PASS("1", test_xiph_PackHeaders, xiph0, 6);

    /* Test multiple packets packing */
    params.packets_count = 0;
    FAIL("2", test_xiph_PackHeaders, xiph1, 11);
    params.packets_count = 3;
    params.packets[0] = &xiph1[3];
    params.packets_sizes[0] = 5;
    params.packets[1] = &xiph1[8];
    params.packets_sizes[1] = 1;
    params.packets[2] = &xiph1[9];
    params.packets_sizes[2] = 2;
    PASS("3", test_xiph_PackHeaders, xiph1, 11);

    /* Test multiple packets packing variable length encoding */
    params.packets_count = 2;
    params.packets[0] = &xiph2[3];
    params.packets_sizes[0] = 256;
    params.packets[1] = &xiph2[3+256];
    params.packets_sizes[1] = 6;
    PASS("4", test_xiph_PackHeaders, xiph2, 265);

    /* Appending */
    params.i_append = 0;
    params.p_append = NULL;
    params.packets[0] = &xiph0[1];
    params.packets_sizes[0] = 5;
    PASS("0", test_xiph_AppendHeaders, xiph0, 6);
    /* append second time */
    xiph2[0] = 1;
    xiph2[1] = 5;
    memcpy(&xiph2[2+0], &xiph0[1], 5);
    memcpy(&xiph2[2+5], &xiph0[1], 5);
    PASS("1", test_xiph_AppendHeaders, xiph2, 12);
    /* check append array overflow */
    free(params.p_append);
    params.i_append = 0;
    params.p_append = NULL;
    for(size_t i=0; i<=XIPH_MAX_HEADER_COUNT; i++)
    {
        params.packets_sizes[0] = 0;
        xiph2[0] = i;
        xiph2[1 + i] = 0;
        RUN("2", test_xiph_AppendHeaders, xiph2, 1 + i,
            (i < XIPH_MAX_HEADER_COUNT) );
    }

    free(params.p_append);

    return 0;
}
