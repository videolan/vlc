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

static void
input_item_parser_InputEvent(input_thread_t *input,
                             const struct vlc_input_event *event, void *parser_)
{
    input_item_parser_id_t *parser = parser_;

    switch (event->type)
    {
        case INPUT_EVENT_TIMES:
            input_item_SetDuration(input_GetItem(input), event->times.length);
            break;
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
            break;
        case INPUT_EVENT_ATTACHMENTS:
            if (parser->cbs->on_attachments_added != NULL)
                parser->cbs->on_attachments_added(input_GetItem(input),
                                                  event->attachments.array,
                                                  event->attachments.count,
                                                  parser->userdata);
            break;
        default:
            break;
    }
}

input_item_parser_id_t *
input_item_Parse(input_item_t *item, vlc_object_t *obj,
                 const input_item_parser_cbs_t *cbs, void *userdata)
{
    assert(cbs && cbs->on_ended);
    input_item_parser_id_t *parser = malloc(sizeof(*parser));
    if (!parser)
        return NULL;

    parser->state = INIT_S;
    parser->cbs = cbs;
    parser->userdata = userdata;
    parser->input = input_Create(obj, input_item_parser_InputEvent, parser,
                                 item, INPUT_TYPE_PREPARSING, NULL, NULL);
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
