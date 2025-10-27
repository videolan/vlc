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
#include <errno.h>
#include <limits.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include <vlc_common.h>
#include <vlc_atomic.h>

#include "../src/libvlc.h"

#include "libvlc_internal.h"
#include "media_internal.h"
#include "picture_internal.h"

static void input_item_subtree_added(vlc_preparser_req *req,
                                     input_item_node_t *node,
                                     void *user_data)
{
    VLC_UNUSED(req);
    libvlc_media_t * p_md = user_data;
    libvlc_media_add_subtree(p_md, node);
    input_item_node_Delete(node);
}

static void send_parsed_changed( libvlc_media_t *p_md,
                                 libvlc_media_parsed_status_t new_status )
{
    libvlc_event_t event;

    if (atomic_exchange(&p_md->parsed_status, new_status) == new_status)
        return;

    /* Duration event */
    event.type = libvlc_MediaDurationChanged;
    event.u.media_duration_changed.new_duration =
        libvlc_time_from_vlc_tick(input_item_GetDuration( p_md->p_input_item ));
    libvlc_event_send( &p_md->event_manager, &event );

    /* Meta event */
    event.type = libvlc_MediaMetaChanged;
    event.u.media_meta_changed.meta_type = 0;
    libvlc_event_send( &p_md->event_manager, &event );

    /* Parsed event */
    event.type = libvlc_MediaParsedChanged;
    event.u.media_parsed_changed.new_status = new_status;
    libvlc_event_send( &p_md->event_manager, &event );
}

/**
 * \internal
 * input_item_preparse_ended (Private) (vlc event Callback)
 */
static void input_item_preparse_ended(vlc_preparser_req *req,
                                      int status, void *user_data)
{
    libvlc_media_t * p_md = user_data;
    libvlc_media_parsed_status_t new_status;

    switch( status )
    {
        case VLC_EGENERIC:
            new_status = libvlc_media_parsed_status_failed;
            break;
        case VLC_ETIMEOUT:
            new_status = libvlc_media_parsed_status_timeout;
            break;
        case -EINTR:
            new_status = libvlc_media_parsed_status_cancelled;
            break;
        case VLC_SUCCESS:
            new_status = libvlc_media_parsed_status_done;
            break;
        default:
            vlc_assert_unreachable();
    }
    send_parsed_changed( p_md, new_status );
    vlc_preparser_req_Release( req );
    p_md->req = NULL;

    if (atomic_fetch_sub_explicit(&p_md->worker_count, 1,
                                  memory_order_release) == 1)
        vlc_atomic_notify_one(&p_md->worker_count);
}

static void input_item_attachments_added( vlc_preparser_req *req,
                                          input_attachment_t *const *array,
                                          size_t count, void *user_data )
{
    VLC_UNUSED(req);
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    libvlc_picture_list_t* list =
        libvlc_picture_list_from_attachments(array, count);
    if( !list )
        return;
    if( !libvlc_picture_list_count(list) )
    {
        libvlc_picture_list_destroy( list );
        return;
    }

    /* Construct the event */
    event.type = libvlc_MediaAttachedThumbnailsFound;
    event.u.media_attached_thumbnails_found.thumbnails = list;

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );

    libvlc_picture_list_destroy( list );
}

static const struct vlc_preparser_cbs preparser_callbacks = {
    .on_ended = input_item_preparse_ended,
    .on_subtree_added = input_item_subtree_added,
    .on_attachments_added = input_item_attachments_added,
};

int libvlc_media_parse_request(libvlc_instance_t *inst, libvlc_media_t *media,
                               libvlc_media_parse_flag_t parse_flag,
                               int timeout)
{
    libvlc_media_parsed_status_t expected = libvlc_media_parsed_status_none;

    while (!atomic_compare_exchange_weak(&media->parsed_status, &expected,
                                        libvlc_media_parsed_status_pending))
        if (expected == libvlc_media_parsed_status_pending
         || expected == libvlc_media_parsed_status_done)
            return -1;

    vlc_preparser_t *parser = libvlc_get_preparser(inst);
    if (unlikely(parser == NULL))
        return -1;

    input_item_t *item = media->p_input_item;
    int parse_scope = 0;
    unsigned int ref = atomic_load_explicit(&media->worker_count,
                                            memory_order_relaxed);
    do
    {
        if (unlikely(ref == UINT_MAX))
            return -1;
    }
    while (!atomic_compare_exchange_weak_explicit(&media->worker_count,
                                                  &ref, ref + 1,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed));

    bool input_net;
    enum input_item_type_e input_type = input_item_GetType(item, &input_net);

    bool do_parse, do_fetch;
    if (parse_flag & libvlc_media_parse_forced)
        do_parse = true;
    else
    {
        if (input_net)
            do_parse = parse_flag & libvlc_media_parse_network;
        else if (parse_flag & libvlc_media_parse_local)
        {
            switch (input_type)
            {
                case ITEM_TYPE_NODE:
                case ITEM_TYPE_FILE:
                case ITEM_TYPE_DIRECTORY:
                case ITEM_TYPE_PLAYLIST:
                    do_parse = true;
                    break;
                default:
                    do_parse = false;
                    break;
            }
        }
        else
            do_parse = false;
    }

    if (do_parse)
        parse_scope |= VLC_PREPARSER_TYPE_PARSE;

    do_fetch = false;
    if (parse_flag & libvlc_media_fetch_local)
    {
        parse_scope |= VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;
        do_fetch = true;
    }
    if (parse_flag & libvlc_media_fetch_network)
    {
        parse_scope |= VLC_PREPARSER_TYPE_FETCHMETA_NET;
        do_fetch = true;
    }

    if (!do_parse && !do_fetch)
    {
        send_parsed_changed( media, libvlc_media_parsed_status_skipped );
        atomic_fetch_sub_explicit(&media->worker_count, 1,
                                  memory_order_relaxed);
        return 0;
    }

    if (parse_flag & libvlc_media_do_interact)
        parse_scope |= VLC_PREPARSER_OPTION_INTERACT;
    parse_scope |= VLC_PREPARSER_OPTION_SUBITEMS;

    if (timeout == -1)
        timeout = var_InheritInteger(inst->p_libvlc_int, "preparse-timeout");

    vlc_preparser_SetTimeout(parser, VLC_TICK_FROM_MS(timeout));

    media->req = vlc_preparser_Push(parser, item, parse_scope,
                                    &preparser_callbacks, media);
    if (media->req == NULL)
    {
        atomic_fetch_sub_explicit(&media->worker_count, 1,
                                  memory_order_relaxed);
        return -1;
    }
    return 0;
}

// Stop parsing of the media
void
libvlc_media_parse_stop(libvlc_instance_t *inst, libvlc_media_t *media)
{
    vlc_preparser_t *parser = libvlc_get_preparser(inst);
    assert(parser != NULL);
    if (media->req != NULL)
    {
        vlc_preparser_Cancel(parser, media->req);
        media->req = NULL;
    }
}
