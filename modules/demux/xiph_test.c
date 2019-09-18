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

enum
{
    OK = VLC_SUCCESS,
    FAIL = VLC_EGENERIC,
};

struct params_s
{
    const void *packets[XIPH_MAX_HEADER_COUNT];
    unsigned packets_sizes[XIPH_MAX_HEADER_COUNT];
    unsigned packets_count;
    bool lavc;
    vlc_fourcc_t codec;
    void *p_append;
    int i_append;
};

#define BAILOUT(run) { fprintf(stderr, "failed %s line %d\n", run, __LINE__); \
                        return 1; }
#define RUN(run, test, a, b, res) \
    if(test(#test " " run, a, b, &params) != res) BAILOUT(#test " " run)
#define EXPECT(foo) if(!(foo)) BAILOUT(run)
#define EXPECT_CLEANUP(foo, cleanup) if(!(foo)) { cleanup; BAILOUT(run) }

static int test_xiph_IsLavcFormat(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    EXPECT(xiph_IsLavcFormat(p_extra, i_extra, source->codec) == source->lavc);
    return OK;
}

static int test_xiph_CountHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    EXPECT(xiph_CountHeaders(p_extra, i_extra) == source->packets_count);
    return OK;
}

static int test_xiph_CountLavcHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    EXPECT(xiph_CountLavcHeaders(p_extra, i_extra) == source->packets_count);
    return OK;
}

static int SplitCompare(const char *run,
                        unsigned packet_size[],
                        const void *packet[], unsigned packet_count,
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
    unsigned packet_sizes[XIPH_MAX_HEADER_COUNT];
    unsigned packet_count;
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
    unsigned packet_sizes[XIPH_MAX_HEADER_COUNT];
    unsigned packet_count;
    int ret = xiph_SplitLavcHeaders(packet_sizes, packets, &packet_count,
                                    i_extra, p_extra);
    if(ret == VLC_SUCCESS)
        ret = SplitCompare(run, packet_sizes, packets, packet_count, source);
    return ret;
}

static int test_xiph_PackHeaders(const char *run,
                 const uint8_t *p_extra, size_t i_extra,
                 const struct params_s *source)
{
    void *p_result;
    int i_result;

    int ret = xiph_PackHeaders(&i_result, &p_result,
                               (unsigned *) source->packets_sizes,
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
    RUN("0", test_xiph_IsLavcFormat, xiph0, 0, OK);
    RUN("1", test_xiph_IsLavcFormat, xiph0, 1, OK);
    RUN("2", test_xiph_IsLavcFormat, xiph0, 2, OK);
    RUN("3", test_xiph_IsLavcFormat, xiph0, 6, OK);
    RUN("lavc0", test_xiph_IsLavcFormat, xiph0, 0, OK);
    RUN("lavc1", test_xiph_IsLavcFormat, xiph0, 1, OK);
    RUN("lavc2", test_xiph_IsLavcFormat, xiph0, 6, OK);
    RUN("lavc3", test_xiph_IsLavcFormat, xiphlavc0, 0, OK);
    RUN("lavc4", test_xiph_IsLavcFormat, xiphlavc0, 1, OK);
    params.lavc = true;
    RUN("lavc5", test_xiph_IsLavcFormat, xiphlavc0, 37, OK);
    params.codec = 0;
    params.lavc = false;
    RUN("lavc6", test_xiph_IsLavcFormat, xiphlavc0, 37, OK);

    /* check count and return 0 on error */
    params.packets_count = 0;
    RUN("0", test_xiph_CountHeaders, xiph0, 0, OK);
    params.packets_count = 1;
    RUN("1", test_xiph_CountHeaders, xiph0, 1, OK);
    params.packets_count = 3;
    RUN("2", test_xiph_CountHeaders, xiph1, 11, OK);

    /* check lavc only valid with count == 3 */
    params.packets_count = 3;
    params.codec = VLC_CODEC_VORBIS;
    RUN("lavc0", test_xiph_CountLavcHeaders, xiphlavc0, 37, OK);
    params.packets_count = 0;
    RUN("lavc1", test_xiph_CountLavcHeaders, xiphlavc0, 35, OK);
    RUN("lavc2", test_xiph_CountLavcHeaders, xiphlavc0, 0, OK);

    /* check split on single/trail packet (no index) */
    params.packets[0] = &xiph0[1];
    params.packets_sizes[0] = 5;
    params.packets_count = 1;
    RUN("0", test_xiph_SplitHeaders, xiph0, 6, OK);
    params.packets_sizes[0] = 0;
    RUN("1", test_xiph_SplitHeaders, xiph0, 1, OK);

    /* check split */
    params.packets_count = 3;
    params.packets[0] = &xiph1[3];
    params.packets_sizes[0] = 5;
    params.packets[1] = &xiph1[8];
    params.packets_sizes[1] = 1;
    params.packets[2] = &xiph1[9];
    params.packets_sizes[2] = 2;
    RUN("2", test_xiph_SplitHeaders, xiph1, 11, OK);
    RUN("3", test_xiph_SplitHeaders, xiph1, 7, FAIL);

    /* check variable length decoding */
    uint8_t xiph2[265];
    memset(xiph2, 0xFF, 265);
    RUN("4", test_xiph_SplitHeaders, xiph2, 265, FAIL);
    xiph2[0] = 1;
    RUN("5", test_xiph_SplitHeaders, xiph2, 265, FAIL);
    xiph2[2] = 1;
    params.packets_count = 2;
    params.packets[0] = &xiph2[3];
    params.packets_sizes[0] = 256;
    params.packets[1] = &xiph2[3+256];
    params.packets_sizes[1] = 6;
    RUN("6", test_xiph_SplitHeaders, xiph2, 265, OK);
    /* /!\ xiph2 content reused in another test below */

    /* check lavc split */
    params.packets_count = 3;
    params.packets[0] = &xiphlavc0[2];
    params.packets_sizes[0] = 30;
    params.packets[1] = &xiphlavc0[34];
    params.packets_sizes[1] = 1;
    params.packets[2] = &xiphlavc0[37];
    params.packets_sizes[2] = 0;
    RUN("lavc0", test_xiph_SplitLavcHeaders, xiphlavc0, 37, OK);
    RUN("lavc1", test_xiph_SplitLavcHeaders, xiphlavc0, 36, FAIL);
    RUN("lavc2", test_xiph_SplitLavcHeaders, xiphlavc0, 31, FAIL);

    /* Test single packet packing */
    params.packets_count = XIPH_MAX_HEADER_COUNT + 1;
    RUN("0", test_xiph_PackHeaders, xiph0, 6, FAIL);
    params.packets_count = 1;
    params.packets[0] = &xiph0[1];
    params.packets_sizes[0] = 5;
    RUN("1", test_xiph_PackHeaders, xiph0, 6, OK);

    /* Test multiple packets packing */
    params.packets_count = 0;
    RUN("2", test_xiph_PackHeaders, xiph1, 11, FAIL);
    params.packets_count = 3;
    params.packets[0] = &xiph1[3];
    params.packets_sizes[0] = 5;
    params.packets[1] = &xiph1[8];
    params.packets_sizes[1] = 1;
    params.packets[2] = &xiph1[9];
    params.packets_sizes[2] = 2;
    RUN("3", test_xiph_PackHeaders, xiph1, 11, OK);

    /* Test multiple packets packing variable length encoding */
    params.packets_count = 2;
    params.packets[0] = &xiph2[3];
    params.packets_sizes[0] = 256;
    params.packets[1] = &xiph2[3+256];
    params.packets_sizes[1] = 6;
    RUN("4", test_xiph_PackHeaders, xiph2, 265, OK);

    /* Appending */
    params.i_append = 0;
    params.p_append = NULL;
    params.packets[0] = &xiph0[1];
    params.packets_sizes[0] = 5;
    RUN("0", test_xiph_AppendHeaders, xiph0, 6, OK);
    /* append second time */
    xiph2[0] = 1;
    xiph2[1] = 5;
    memcpy(&xiph2[2+0], &xiph0[1], 5);
    memcpy(&xiph2[2+5], &xiph0[1], 5);
    RUN("1", test_xiph_AppendHeaders, xiph2, 12, OK);
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
            ((i < XIPH_MAX_HEADER_COUNT) ? OK : FAIL) );
    }

    free(params.p_append);

    return 0;
}
