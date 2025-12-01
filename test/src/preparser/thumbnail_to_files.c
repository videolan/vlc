/*****************************************************************************
 * thumbnail_to_files.c: test thumbnailing to files API
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_input_item.h>
#include <vlc_picture.h>
#include <vlc_modules.h>
#include <vlc_fs.h>

#include <errno.h>

#define MOCK_WIDTH 640
#define MOCK_HEIGHT 480

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define MOCK_URL "mock://video_track_count=1;" \
                 "video_width="STRINGIFY(MOCK_WIDTH)";" \
                 "video_height="STRINGIFY(MOCK_HEIGHT)

struct test_entry
{
    int in_width;
    int in_height;
    bool in_crop;

    bool should_succeed;
    unsigned out_width;
    unsigned out_height;
};

static const struct test_entry test_entries[] =
{
    {
        .in_width = 0, .in_height = 0, .in_crop = false,
        .should_succeed = true,
        .out_width = MOCK_WIDTH, .out_height = MOCK_HEIGHT
    },

    {
        .in_width = 0, .in_height = 0, .in_crop = false,
        .should_succeed = false,
    },
    {
        .in_width = 320, .in_height = 0, .in_crop = false,
        .should_succeed = true,
        .out_width = 320, .out_height = 240,
    },
};

struct context
{
    size_t test_count;
    const struct vlc_thumbnailer_output *entries;
    const struct test_entry *test_entries;
    vlc_preparser_t *preparser;

    vlc_fourcc_t parsed_fourcc;
    const char *forced_demux;

    vlc_sem_t sem;
};

static void parser_on_ended(vlc_preparser_req *req, int status, void *userdata)
{
    input_item_t *item = vlc_preparser_req_GetItem(req);
    struct context *context = userdata;
    assert(status == VLC_SUCCESS);

    /* Retrieve the test_entry from the uri */
    const struct test_entry *test_entry = NULL;
    for (size_t i = 0; i < context->test_count; ++i)
    {
        if (strcmp(item->psz_uri + strlen("file://"),
                   context->entries[i].file_path) == 0)
        {
            test_entry = &context->test_entries[i];
            break;
        }
    }
    assert(test_entry != NULL);

    assert(item->es_vec.size == 1);
    const struct input_item_es *item_es = &item->es_vec.data[0];
    const es_format_t *es_fmt = &item_es->es;

    /* Check if the video format of the thumbnail is expected */
    assert(es_fmt->i_cat == VIDEO_ES);
    assert(es_fmt->i_codec == context->parsed_fourcc);
    assert(es_fmt->video.i_width == test_entry->out_width);
    assert(es_fmt->video.i_height == test_entry->out_height);

    vlc_sem_post(&context->sem);
    vlc_preparser_req_Release(req);
}

static void on_ended(vlc_preparser_req *thumbnailer_req, int status,
                     const bool *result_array, size_t result_count, void *data)
{
    (void) thumbnailer_req;
    struct context *context = data;
    assert(status == VLC_SUCCESS);
    assert(context->test_count == result_count);

    static const struct vlc_preparser_cbs parser_cbs = {
        .on_ended = parser_on_ended,
    };

    for (size_t i = 0; i < result_count; ++i)
    {
        const struct vlc_thumbnailer_output *entry = &context->entries[i];
        const struct test_entry *test_entry = &context->test_entries[i];

        assert(test_entry->should_succeed == result_array[i]);

        if (!result_array[i])
            vlc_sem_post(&context->sem);
        else
        {
            /* Parse the thumbnail to check its validity */
            char *uri;
            int ret = asprintf(&uri, "file://%s", entry->file_path);
            assert(ret > 0);
            input_item_t *thumb = input_item_New(uri, "thumb");
            free(uri);
            assert(thumb != NULL);

            if (context->forced_demux != NULL)
            {
                /* Setup the forced demux */
                char *option;
                ret = asprintf(&option, ":demux=%s", context->forced_demux);
                assert(ret > 0);
                input_item_AddOption(thumb, option, VLC_INPUT_OPTION_TRUSTED);
                free(option);
            }

            vlc_preparser_req *parser_req =
                vlc_preparser_Push(context->preparser, thumb,
                                   VLC_PREPARSER_TYPE_PARSE,
                                   &parser_cbs, context);
            assert(parser_req != NULL);
            input_item_Release(thumb);
        }
    }
}

static int get_formats(enum vlc_thumbnailer_format *out_format,
                       vlc_fourcc_t *parsed_fourcc, const char **forced_demux,
                       const char **ext)
{
    /* This test require any of these images encoders */
    static const struct format
    {
        enum vlc_thumbnailer_format format;
        vlc_fourcc_t parsed_fourcc;
        const char *ext;
        const char *forced_demux;
    } formats[] = {
        {
            .format = VLC_THUMBNAILER_FORMAT_PNG,
            .parsed_fourcc = VLC_CODEC_RGB24,
            .ext = "png"
        },
        {
            .format = VLC_THUMBNAILER_FORMAT_WEBP,
            .parsed_fourcc = VLC_CODEC_I420,
            .ext = "webp"
        },
        {
            .format = VLC_THUMBNAILER_FORMAT_JPEG,
            .parsed_fourcc = VLC_CODEC_MJPG,
            .ext = "jpg",
            /* XXX: The mjpeg demux won't parse width/height */
            .forced_demux = "avformat"
        },
    };

    /* Retrieve the best format */
    enum vlc_thumbnailer_format format;
    int ret = vlc_preparser_GetBestThumbnailerFormat(&format, ext);
    if (ret != 0)
        return ret;

    /* Check that the best format is checked and succeed */
    ret = vlc_preparser_CheckThumbnailerFormat(format);
    assert(ret == 0);

    for (size_t i = 0; i < ARRAY_SIZE(formats); ++i)
    {
        if (formats[i].format != format)
            continue;

        /* Check if the forced demux module (for preparsing) is present */
        if (formats[i].forced_demux != NULL
         && !module_exists(formats[i].forced_demux))
            return VLC_ENOENT;

        assert(strcmp(*ext, formats[i].ext) == 0);

        *out_format = format;
        *parsed_fourcc = formats[i].parsed_fourcc;
        *forced_demux = formats[i].forced_demux;
        return 0;
    }

    vlc_assert_unreachable();
}

int main(int argc, const char *argv[])
{
    test_init();
    argc--;
    argv++;

    size_t test_count = ARRAY_SIZE(test_entries);
    struct vlc_thumbnailer_output entries[test_count];
    int fd_array[test_count];
    char path_array[test_count][sizeof("/tmp/libvlc_XXXXXX")];

    libvlc_instance_t *vlc = libvlc_new(argc, argv);
    assert(vlc);

    /* This test require swscale */
    if (!module_exists("swscale"))
    {
        fprintf(stderr, "skip: no \"swscale\" module\n");
        goto skip;
    }

    enum vlc_thumbnailer_format format;
    const char *forced_demux, *ext;
    vlc_fourcc_t parsed_fourcc;
    int ret = get_formats(&format, &parsed_fourcc, &forced_demux, &ext);
    if (ret == VLC_ENOENT)
    {
        fprintf(stderr, "skip: no \"image encoder\" modules\n");
        goto skip;
    }
    assert(ret == 0);

    fprintf(stderr, "thumbnail_to_files: using format: %s\n", ext);

    /* Fill output entries */
    for (size_t i = 0; i < test_count; ++i)
    {
        const struct test_entry *test_entry = &test_entries[i];
        struct vlc_thumbnailer_output *entry = &entries[i];

        if (test_entry->should_succeed)
        {
            /* Fill the output entry with a valid tmp path */
            strcpy(path_array[i], "/tmp/libvlc_XXXXXX");
            fd_array[i] = vlc_mkstemp(path_array[i]);
            if (i == 0 && fd_array[i] == -1)
            {
                fprintf(stderr, "skip: vlc_mkstemp failed\n");
                goto skip;
            }
            assert(fd_array[i] != -1);
            entry->file_path = path_array[i];
        }
        else
        {
            fd_array[i] = -1;
            entry->file_path = "/this/path/does/not/exist/and/should/fail";
        }
        entry->format = format;
        entry->width = test_entry->in_width;
        entry->height = test_entry->in_height;
        entry->crop = test_entry->in_crop;
        entry->creat_mode = 0666;
    }

    /* Preparser configuration: thumbnailer + parser */
    const struct vlc_preparser_cfg cfg = {
        .types = VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES|VLC_PREPARSER_TYPE_PARSE,
        .max_parser_threads = 1,
        .max_thumbnailer_threads = 1,
        .timeout = 0,
    };
    vlc_preparser_t *preparser = vlc_preparser_New(VLC_OBJECT(vlc->p_libvlc_int),
                                                   &cfg);
    assert(preparser != NULL);

    struct context context = {
        .test_count = test_count,
        .entries = entries,
        .test_entries = test_entries,
        .preparser = preparser,
        .parsed_fourcc = parsed_fourcc,
        .forced_demux = forced_demux,
    };
    vlc_sem_init(&context.sem, 0);

    /* Common arguments */
    const struct vlc_thumbnailer_arg arg =
    {
        .seek = {
            .type = VLC_THUMBNAILER_SEEK_POS,
            .pos = 0.2,
            .speed = VLC_THUMBNAILER_SEEK_FAST,
        },
        .hw_dec = false,
    };
    static const struct vlc_thumbnailer_to_files_cbs cbs = {
        .on_ended = on_ended,
    };

    input_item_t *item = input_item_New(MOCK_URL, "mock");
    assert(item != NULL);

    vlc_preparser_req *req =
        vlc_preparser_GenerateThumbnailToFiles(preparser, item, &arg,
                                               entries, test_count,
                                               &cbs, &context);

    assert(req != NULL);

    /* Wait for all tests */
    for (size_t i = 0; i < test_count; ++i)
        vlc_sem_wait(&context.sem);

    size_t count = vlc_preparser_Cancel(preparser, req);
    assert(count == 0); /* Should not be cancelled and already processed */
    vlc_preparser_req_Release(req);

    for (size_t i = 0; i < test_count; ++i)
        if (fd_array[i] != -1)
        {
            unlink(path_array[i]);
            close(fd_array[i]);
        }

    input_item_Release(item);

    vlc_preparser_Delete(preparser);
    libvlc_release(vlc);
    return 0;

skip:
    libvlc_release(vlc);
    return 77;
}
