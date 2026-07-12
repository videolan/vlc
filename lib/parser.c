/*****************************************************************************
 * parser.c: Libvlc parser API
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#include <assert.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_parser.h>
#include <vlc/libvlc_picture.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_preparser.h>

#include "../src/libvlc.h"
#include "libvlc_internal.h"
#include "media_internal.h"
#include "picture_internal.h"

static_assert(VLC_THUMBNAILER_SEEK_NONE == (int) libvlc_thumbnailer_seek_none &&
              VLC_THUMBNAILER_SEEK_TIME == (int) libvlc_thumbnailer_seek_time &&
              VLC_THUMBNAILER_SEEK_POS  == (int) libvlc_thumbnailer_seek_pos,
              "lib/vlc_thumbnailer seek type mismatch");

static_assert(VLC_THUMBNAILER_SEEK_PRECISE == (int) libvlc_media_thumbnail_seek_precise &&
              VLC_THUMBNAILER_SEEK_FAST    == (int) libvlc_media_thumbnail_seek_fast,
              "lib/vlc_thumbnailer seek speed mismatch");

struct libvlc_parser_t
{
    libvlc_instance_t *libvlc;
    vlc_preparser_t *preparser;
};

union libvlc_parser_cbs_internal
{
    const struct libvlc_parser_cbs *parser;
    const struct libvlc_thumbnailer_cbs *thumbnailer;
};

struct libvlc_parser_task
{
    /* LibVLC instance */
    libvlc_instance_t *instance;

    /* media to parse/thumbnail */
    libvlc_media_t *media;

    /* Dimensions (for thumbnailing) */
    unsigned int width;
    unsigned int height;

    /* True to enable crop (false by default) (for thumbnailing) */
    bool crop;

    /* Picture type (for thumbnailing) */
    libvlc_picture_type_t type;

    /* parser/thumbnailer callbacks */
    union libvlc_parser_cbs_internal cbs;

    /* opaque data for cbs */
    void *cbs_opaque;

    /* preparser request handle */
    vlc_preparser_req *preparser_req;

    /* task reference count */
    vlc_atomic_rc_t rc;
};

/* Internal function to copy the required members of the request object into a heap-allocated
   structure for further processing. The object is allocated on the heap to ensure it remains
   valid for the entire duration of the parsing or thumbnailing operation. */
static struct libvlc_parser_task *
libvlc_parser_task_new(libvlc_media_t *media,
                       const libvlc_thumbnailer_request_t *thumbnailer_req,
                       union libvlc_parser_cbs_internal cbs,
                       void *cbs_opaque)
{
    struct libvlc_parser_task *task = malloc(sizeof(*task));
    if (task == NULL)
        return NULL;

    task->instance = NULL;
    task->media = libvlc_media_retain(media);
    task->cbs = cbs;
    task->cbs_opaque = cbs_opaque;
    task->preparser_req = NULL;
    vlc_atomic_rc_init(&task->rc);
    if (thumbnailer_req != NULL)
    {
        task->width = thumbnailer_req->width;
        task->height = thumbnailer_req->height;
        task->crop = thumbnailer_req->crop;
        task->type = thumbnailer_req->type;
    }
    return task;
}

/* Internal function to destroy the heap allocated task object */
static void libvlc_parser_task_destroy(struct libvlc_parser_task *task)
{
    if (task->instance != NULL)
        libvlc_release(task->instance);

    if (task->preparser_req != NULL)
        vlc_preparser_req_Release(task->preparser_req);

    libvlc_media_release(task->media);
    free(task);
}

static void vlc_preparser_subtree_added(vlc_preparser_req *req,
                                        input_item_node_t *node,
                                        void *user_data)
{
    struct libvlc_parser_task *task = user_data;
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(task->media->p_input_item == item);
    libvlc_media_add_subtree(task->media, node);
    input_item_node_Delete(node);
}

static void
vlc_preparser_ended(vlc_preparser_req *req, int status, void *user_data)
{
    struct libvlc_parser_task *task = user_data;
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(task->media->p_input_item == item);

    libvlc_parser_status_t parser_status;
    switch (status)
    {
        case VLC_EGENERIC:
            parser_status = libvlc_parser_status_failed;
            break;
        case VLC_ETIMEOUT:
            parser_status = libvlc_parser_status_timeout;
            break;
        case -EINTR:
            parser_status = libvlc_parser_status_cancelled;
            break;
        case VLC_SUCCESS:
            parser_status = libvlc_parser_status_done;
            break;
        default:
            vlc_assert_unreachable();
    }

    /* The media only tracks whether it ended up parsed; the detailed
       outcome is reported to the caller through parser_status below. A
       failed/timed out/cancelled parse leaves the media unparsed. */
    atomic_store(&task->media->parsed_status,
                 parser_status == libvlc_parser_status_done
                     ? libvlc_media_parsed_status_done
                     : libvlc_media_parsed_status_none);

    task->cbs.parser->on_parsed(
        task->cbs_opaque, task, parser_status);

    libvlc_parser_task_release(task);
}

static void vlc_preparser_attachments_added(vlc_preparser_req *req,
                                            input_attachment_t *const *array,
                                            size_t count,
                                            void *user_data)
{
    struct libvlc_parser_task *task = user_data;
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(task->media->p_input_item == item);

    if (task->cbs.parser->on_attachments_added == NULL)
        return;

    libvlc_picture_list_t *list =
        libvlc_picture_list_from_attachments(array, count);
    if (!list)
        return;
    if (!libvlc_picture_list_count(list))
    {
        libvlc_picture_list_destroy(list);
        return;
    }

    task->cbs.parser->on_attachments_added(task->cbs_opaque,
                                           task,
                                           list);
    libvlc_picture_list_destroy(list);
}

static const struct vlc_preparser_cbs preparser_callbacks = {
    .on_ended = vlc_preparser_ended,
    .on_subtree_added = vlc_preparser_subtree_added,
    .on_attachments_added = vlc_preparser_attachments_added,
};

static void vlc_thumbnailer_ended(vlc_preparser_req *req, int status, picture_t *thumbnail, void *data)
{
    VLC_UNUSED(status);
    struct libvlc_parser_task *task = data;
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(task->media->p_input_item == item);

    libvlc_picture_t* pic = NULL;
    if (thumbnail != NULL)
        pic = libvlc_picture_new(VLC_OBJECT(task->instance->p_libvlc_int),
                                 thumbnail, task->type, task->width,
                                 task->height, task->crop);

    task->cbs.thumbnailer->on_ended(task->cbs_opaque, task, pic);
    if (pic != NULL)
        libvlc_picture_release(pic);

    libvlc_parser_task_release(task);
}

static const struct vlc_thumbnailer_cbs thumbnailer_callbacks = {
    .on_ended = vlc_thumbnailer_ended,
};

/* Calculate a combination of VLC_PREPARSER_TYPE_* and VLC_PREPARSER_OPTION_* flags
   (to be passed to vlc_preparser_Push) from libvlc_media_parse_flag_t. Return -1 to skip parsing */
static int get_parser_type_options(libvlc_media_parse_flag_t parse_flag)
{
    int parse_scope = 0;
    bool do_parse = false, do_fetch = false;

    if (parse_flag & libvlc_media_parse)
    {
        do_parse = true;
        parse_scope |= VLC_PREPARSER_TYPE_PARSE;
    }

    if (parse_flag & libvlc_media_fetch_local)
    {
        do_fetch = true;
        parse_scope |= VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;
    }

    if (parse_flag & libvlc_media_fetch_network)
    {
        do_fetch = true;
        parse_scope |= VLC_PREPARSER_TYPE_FETCHMETA_NET;
    }

    if (!do_parse && !do_fetch)
        return -1; /* nothing to parse/fetch */

    if (parse_flag & libvlc_media_do_interact)
        parse_scope |= VLC_PREPARSER_OPTION_INTERACT;

    parse_scope |= VLC_PREPARSER_OPTION_SUBITEMS;

    return parse_scope;
}

/* retain a reference to the parser task */
static struct libvlc_parser_task *
libvlc_parser_task_retain(struct libvlc_parser_task *task)
{
    vlc_atomic_rc_inc(&task->rc);
    return task;
}

libvlc_parser_task *
libvlc_parser_queue(libvlc_parser_t *parser,
                    const libvlc_parser_request_t *req,
                    const struct libvlc_parser_cbs *cbs,
                    void *cbs_opaque)
{
    assert(parser != NULL);
    assert(req != NULL && req->media != NULL);
    assert(cbs != NULL && cbs->on_parsed != NULL);

    /* No different versions to handle for now */
    assert(req->version <= 0);
    assert(cbs->version <= 0);

    int type_options = VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_OPTION_SUBITEMS;
    if (req->parse_flags != 0)
    {
        type_options = get_parser_type_options(req->parse_flags);
        if (type_options == -1) /* nothing to parse/fetch */
            return NULL;
    }

    libvlc_media_t *media = req->media;
    input_item_t *item = media->p_input_item;

    union libvlc_parser_cbs_internal req_cbs = {
        .parser = cbs,
    };

    struct libvlc_parser_task *task = libvlc_parser_task_new(media, NULL, req_cbs, cbs_opaque);
    if (task == NULL)
        return NULL;

    libvlc_parser_task_retain(task);

    task->preparser_req = vlc_preparser_Push(parser->preparser,
                                             item,
                                             type_options,
                                             &preparser_callbacks,
                                             task);

    if (task->preparser_req == NULL)
    {
        libvlc_parser_task_destroy(task);
        return NULL;
    }

    return task;
}

libvlc_parser_task *
libvlc_parser_queue_thumbnailing(libvlc_parser_t *parser,
                                 const libvlc_thumbnailer_request_t *req,
                                 const struct libvlc_thumbnailer_cbs *cbs,
                                 void *cbs_opaque)
{
    assert(parser != NULL);
    assert(req != NULL && req->media != NULL);
    assert(cbs != NULL && cbs->on_ended != NULL);

    /* No different versions to handle for now */
    assert(req->version <= 0);
    assert(cbs->version <= 0);

    input_item_t *item = req->media->p_input_item;

    union libvlc_parser_cbs_internal req_cbs = {
        .thumbnailer = cbs,
    };

    struct libvlc_parser_task *task = libvlc_parser_task_new(req->media, req, req_cbs, cbs_opaque);

    if (task == NULL)
        return NULL;

    task->instance = libvlc_retain(parser->libvlc);
    libvlc_thumbnailer_seek_type_t seek_type = req->seek.type;
    struct vlc_thumbnailer_arg thumb_arg;
    thumb_arg.hw_dec = req->hw_dec;

    switch (seek_type)
    {
        case libvlc_thumbnailer_seek_time:
            thumb_arg.seek.type = VLC_THUMBNAILER_SEEK_TIME;
            thumb_arg.seek.time = vlc_tick_from_libvlc_time(req->seek.value.time);
            break;
        case libvlc_thumbnailer_seek_pos:
            thumb_arg.seek.type = VLC_THUMBNAILER_SEEK_POS;
            thumb_arg.seek.pos = req->seek.value.pos;
            break;
        default:
            thumb_arg.seek.type = VLC_THUMBNAILER_SEEK_NONE;
            break;
    }

    thumb_arg.seek.speed = req->seek.speed == libvlc_media_thumbnail_seek_fast
                         ? VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE;

    libvlc_parser_task_retain(task);

    task->preparser_req = vlc_preparser_GenerateThumbnail(parser->preparser, item, &thumb_arg,
                                                          &thumbnailer_callbacks, task);

    if (task->preparser_req == NULL)
    {
        libvlc_parser_task_destroy(task);
        return NULL;
    }

    return task;
}

size_t libvlc_parser_cancel_request(libvlc_parser_t *parser,
                                    libvlc_parser_task *task)
{
    if (task == NULL)
        return vlc_preparser_Cancel(parser->preparser, NULL);

    return vlc_preparser_Cancel(parser->preparser, task->preparser_req);
}

libvlc_parser_t *libvlc_parser_new(libvlc_instance_t *inst,
                                   const struct libvlc_parser_cfg *cfg)
{
    assert(inst != NULL);
    assert(cfg != NULL);
    libvlc_time_t timeout;

    /* No different versions to handle for now */
    assert(cfg->version <= 0);

    libvlc_parser_t *parser = malloc(sizeof(*parser));
    if (parser == NULL)
        return NULL;

    if (cfg->timeout == -1)
        /* "preparse-timeout" is in ms; libvlc_time_t is in us */
        timeout = var_InheritInteger(inst->p_libvlc_int, "preparse-timeout") * INT64_C(1000);
    else
        timeout = cfg->timeout;

    const struct vlc_preparser_cfg preparser_cfg = {
        .types = VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_TYPE_FETCHMETA_ALL |
                 VLC_PREPARSER_TYPE_THUMBNAIL,
        .max_parser_threads = cfg->max_parser_threads,
        .max_thumbnailer_threads = cfg->max_thumbnailer_threads,
        .timeout = vlc_tick_from_libvlc_time(timeout),
        .external_process = false,
    };
    parser->preparser =
        vlc_preparser_New(VLC_OBJECT(inst->p_libvlc_int), &preparser_cfg);
    if (parser->preparser == NULL)
    {
        free(parser);
        return NULL;
    }
    parser->libvlc = libvlc_retain(inst);

    return parser;
}

libvlc_media_t *libvlc_parser_task_get_media(libvlc_parser_task *task)
{
    assert(task != NULL);
    return task->media;
}

void libvlc_parser_task_release(libvlc_parser_task *task)
{
    assert(task != NULL);
    if (!vlc_atomic_rc_dec(&task->rc))
        return;

    libvlc_parser_task_destroy(task);
}

void libvlc_parser_destroy(libvlc_parser_t *parser)
{
    vlc_preparser_Delete(parser->preparser);
    libvlc_release(parser->libvlc);
    free(parser);
}
