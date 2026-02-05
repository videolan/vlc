/*****************************************************************************
 * thumbnail.c: test thumbnailing API
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"
#include "vlc_vector.h"

#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_input_item.h>
#include <vlc_picture.h>

#include <errno.h>

#define MOCK_DURATION VLC_TICK_FROM_SEC( 5 * 60 )

const struct {
    /*global*/
    vlc_tick_t length;
    uint32_t audio_track_count;
    uint32_t video_track_count;
    uint32_t sub_track_count;
    /*audio*/
    uint32_t audio_channels;
    vlc_fourcc_t audio_format;
    uint32_t audio_rate;
    /*video*/
    vlc_fourcc_t video_chorma;
    uint32_t video_width;
    uint32_t video_height;
    uint32_t video_frame_rate;
    uint32_t video_orientation;
    /*sub*/
    vlc_fourcc_t sub_format;
} test_params[] = {
    {
        /*global*/ MOCK_DURATION, 999, 999, 999,
        /*audio*/ 9, VLC_CODEC_F32L, 400000,
        /*video*/ VLC_CODEC_I444_16L, 1920, 1080, 2000, ORIENT_TRANSPOSED,
        /*sub*/ VLC_CODEC_SUBT,
    },
};

#define CMP_PRINT(s, i) \
    (fprintf(stderr, "test: %d -> input_item: %s not the same\n", i, s), false)

#define CMP_BOOL(ret, i, a, b) \
    do {\
        if (a != b) {\
            ret = false;\
            fprintf(stderr, "test: %d\n\t" #a ": %s\n\t" #b ": %s\n", i, a?"true":"false", b?"true":"false");\
        }\
    }while(0)

#define CMP_STRING(ret, i, a, b) \
    do {\
        if (strcmp(a, b)) {\
            ret = false;\
            fprintf(stderr, "test: %d\n\t" #a ": %s  \n\t" #b ": %s\n", i, a, b);\
        }\
    }while(0)

#define CMP_INT(ret, i, a, b) \
    do {\
        if ((int32_t)a != (int32_t)b) {\
            ret = false;\
            fprintf(stderr, "test: %d  \n\t" #a ": %d  \n\t" #b ": %d\n", i, (int32_t)a, (int32_t)b);\
        }\
    }while(0)

#define CMP_UINT(ret, i, a, b) \
    do {\
        if ((uint32_t)a != (uint32_t)b) {\
            ret = false;\
            fprintf(stderr, "test: %d  \n\t" #a ": %u  \n\t" #b ": %u\n", i, (uint32_t)a, (uint32_t)b);\
        }\
    }while(0)

static bool cmp_item(const input_item_t *a, const input_item_t *b, int i)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    bool same = true;
    CMP_STRING(same, i, a->psz_name, b->psz_name);
    CMP_STRING(same, i, a->psz_uri, b->psz_uri);
    CMP_INT(same, i, a->i_options, b->i_options);
    CMP_INT(same, i, a->i_duration, b->i_duration);

    CMP_UINT(same, i, a->es_vec.size, b->es_vec.size);
    if (a->es_vec.size != b->es_vec.size) {
        goto end_vec;
    }

    struct input_item_es *a_ies = NULL;
    struct input_item_es *b_ies = NULL;

    for (size_t j = 0; j < a->es_vec.size; j++) {
        a_ies = &a->es_vec.data[j];
        b_ies = &b->es_vec.data[j];
        if (!es_format_IsSimilar(&a_ies->es, &b_ies->es)) {
            same = CMP_PRINT("es_vec[] es_format", i);
        }
        CMP_BOOL(same, i, a_ies->es.b_packetized, b_ies->es.b_packetized);
        CMP_STRING(same, i, a_ies->id, b_ies->id);
        CMP_BOOL(same, i, a_ies->id_stable, b_ies->id_stable);
    }

end_vec:

    CMP_INT(same, i, a->i_type, b->i_type);
    CMP_BOOL(same, i, a->b_net, b->b_net);
    return same;
}

struct test_ctx
{
    vlc_cond_t cond;
    vlc_mutex_t lock;
    int status;
    bool done;
};

static void preparser_callback(struct vlc_preparser_req *req, int status, void *userdata)
{
    (void)req;
    struct test_ctx *ctx = userdata;
    vlc_mutex_lock(&ctx->lock);
    ctx->status = status;
    ctx->done = true;
    vlc_cond_signal(&ctx->cond);
    vlc_mutex_unlock(&ctx->lock);
}

static input_item_t *test_preparser(vlc_object_t *obj, const char *mrl, vlc_tick_t timeout, bool external)
{
    struct test_ctx ctx;
    vlc_cond_init(&ctx.cond);
    vlc_mutex_init(&ctx.lock);

    ctx.status = VLC_EGENERIC;
    ctx.done = false;

    const struct vlc_preparser_cfg cfg = {
        .types = VLC_PREPARSER_TYPE_PARSE| VLC_PREPARSER_TYPE_FETCHMETA_LOCAL,
        .timeout = timeout,
        .external_process = external,
    };

    vlc_preparser_t* preparser = vlc_preparser_New(obj, &cfg);
    assert(preparser != NULL);

    input_item_t *item = input_item_New(mrl, "mock item");
    assert(item != NULL);

    vlc_mutex_lock(&ctx.lock);

    struct vlc_preparser_req *req;
    static const struct vlc_preparser_cbs cbs = {
        .on_ended = preparser_callback,
        .on_attachments_added = NULL,
        .on_subtree_added = NULL,
    };

    int options = VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;
    req = vlc_preparser_Push(preparser, item, options, &cbs, &ctx);
    assert(req != NULL);
    vlc_preparser_req_Release(req);

    while (!ctx.done)
        vlc_cond_wait(&ctx.cond, &ctx.lock);

    assert(ctx.status == VLC_SUCCESS);

    vlc_mutex_unlock(&ctx.lock);

    vlc_preparser_Delete(preparser);
    return item;
}

static char *test_create_mock(int i)
{
    assert(i >= 0 && i < (int)ARRAY_SIZE(test_params));

    char audio_format[5] = {0};
    vlc_fourcc_to_char(test_params[i].audio_format, audio_format);
    char video_chroma[5] = {0};
    vlc_fourcc_to_char(test_params[i].video_chorma, video_chroma);
    char sub_format[5] = {0};
    vlc_fourcc_to_char(test_params[i].sub_format, sub_format);

    char *mrl = NULL;
    int ret = asprintf(&mrl, "mock://"
                       "length=%" PRId64 ";"
                       "audio_track_count=%u;"
                       "video_track_count=%u;"
                       "sub_track_count=%u;"
                       "audio_channels=%u;"
                       "audio_format=%s;"
                       "audio_rate=%u;"
                       "video_chorma=%s;"
                       "video_width=%u;"
                       "video_height=%u;"
                       "video_frame_rate=%u;"
                       "video_orientation=%u;"
                       "sub_format=%s",
                       test_params[i].length,
                       test_params[i].audio_track_count,
                       test_params[i].video_track_count,
                       test_params[i].sub_track_count,
                       test_params[i].audio_channels,
                       audio_format,
                       test_params[i].audio_rate,
                       video_chroma,
                       test_params[i].video_width,
                       test_params[i].video_height,
                       test_params[i].video_frame_rate,
                       test_params[i].video_orientation,
                       sub_format);
    assert(ret > 0);
    return mrl;
}

static void test_preparser_cmp(vlc_object_t *obj, int i)
{
    assert(obj != NULL);
    assert(i >= 0 && i < (int)ARRAY_SIZE(test_params));

    char *mrl = test_create_mock(i);
    input_item_t *inter = test_preparser(obj, mrl, VLC_TICK_INVALID, false);
    input_item_t *exter = test_preparser(obj, mrl, VLC_TICK_INVALID, true);
    free(mrl);

    assert(cmp_item(inter, exter, i));
}

int main( void )
{
#if !defined(HAVE_VLC_PROCESS_SPAWN)
    return 77;
#else
    test_init();

    static const char * argv[] = {
        "-v",
        "--ignore-config",
    };
    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(argv), argv);
    assert(vlc);

    for (size_t i = 0; i < ARRAY_SIZE(test_params); ++i) {
        test_preparser_cmp(VLC_OBJECT(vlc->p_libvlc_int), i);
    }

    libvlc_release( vlc );
#endif
}
