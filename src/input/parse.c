/*****************************************************************************
 * parse.c: input_item parsing management
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <time.h>
#include <limits.h>
#include <ctype.h>

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include "item.h"
#include "info.h"
#include "input_internal.h"

#include <vlc_charset.h>

struct input_item_parser_id_t
{
    input_thread_t *input;
    input_state_e state;
    const input_item_parser_cbs_t *cbs;
    void *userdata;
};

static bool
input_item_parser_InputEvent(input_thread_t *input,
                             const struct vlc_input_event *event, void *parser_)
{
    input_item_parser_id_t *parser = parser_;

    bool handled = true;
    switch (event->type)
    {
        case INPUT_EVENT_TIMES:
        {
            vlc_tick_t duration = input_GetItemDuration(input, event->times.length);
            input_item_SetDuration(input_GetItem(input), duration);
            break;
        }
        case INPUT_EVENT_STATE:
            parser->state = event->state.value;
            break;
        case INPUT_EVENT_DEAD:
        {
            int status = parser->state == END_S ? VLC_SUCCESS : VLC_EGENERIC;
            parser->cbs->on_ended(input_GetItem(input), status, parser->userdata);
            break;
        }
        case INPUT_EVENT_SUBITEMS:
            if (parser->cbs->on_subtree_added)
                parser->cbs->on_subtree_added(input_GetItem(input),
                                              event->subitems, parser->userdata);
            else
                input_item_node_Delete(event->subitems);
            break;
        case INPUT_EVENT_ATTACHMENTS:
            if (parser->cbs->on_attachments_added != NULL)
                parser->cbs->on_attachments_added(input_GetItem(input),
                                                  event->attachments.array,
                                                  event->attachments.count,
                                                  parser->userdata);
            break;
        default:
            handled = false;
            break;
    }
    return handled;
}

input_item_parser_id_t *
input_item_Parse(vlc_object_t *obj, input_item_t *item,
                 const struct input_item_parser_cfg *cfg)
{
    assert(cfg != NULL && cfg->cbs != NULL && cfg->cbs->on_ended);
    input_item_parser_id_t *parser = malloc(sizeof(*parser));
    if (!parser)
        return NULL;

    parser->state = INIT_S;
    parser->cbs = cfg->cbs;
    parser->userdata = cfg->cbs_data;

    static const struct vlc_input_thread_callbacks input_cbs = {
        .on_event = input_item_parser_InputEvent,
    };

    const struct vlc_input_thread_cfg input_cfg = {
        .type = INPUT_TYPE_PREPARSING,
        .hw_dec = INPUT_CFG_HW_DEC_DISABLED,
        .cbs = &input_cbs,
        .cbs_data = parser,
        .preparsing.subitems = cfg->subitems,
        .interact = cfg->interact,
    };
    parser->input = input_Create(obj, item, &input_cfg );
    if (!parser->input || input_Start(parser->input))
    {
        if (parser->input)
            input_Close(parser->input);
        free(parser);
        return NULL;
    }
    return parser;
}

void
input_item_parser_id_Interrupt(input_item_parser_id_t *parser)
{
    input_Stop(parser->input);
}

void
input_item_parser_id_Release(input_item_parser_id_t *parser)
{
    input_item_parser_id_Interrupt(parser);
    input_Close(parser->input);
    free(parser);
}
