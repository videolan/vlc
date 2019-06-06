/*****************************************************************************
 * dynamicoverlay_queue.c : dynamic overlay plugin commands
 *****************************************************************************
 * Copyright (C) 2008-2009 VLC authors and VideoLAN
 *
 * Author: Søren Bøg <avacore@videolan.org>
 *         Jean-Paul Saman <jpsaman@videolan.org>
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

#include "dynamicoverlay.h"

/*****************************************************************************
 * queue_t: Command queue
 *****************************************************************************/

int QueueInit( queue_t *p_queue )
{
    memset( p_queue, 0, sizeof( queue_t ) );
    p_queue->p_head = NULL;
    p_queue->p_tail = NULL;

    return VLC_SUCCESS;
}

int QueueDestroy( queue_t *p_queue )
{
    command_t *p_cur = p_queue->p_head, *p_temp;
    while( p_cur != NULL )
    {
        p_temp = p_cur;
        p_cur = p_cur->p_next;
        free( p_temp );
    }
    p_queue->p_head = NULL;
    p_queue->p_tail = NULL;

    return VLC_SUCCESS;
}

int QueueEnqueue( queue_t *p_queue, command_t *p_cmd )
{
    if( p_queue->p_tail != NULL )
    {
        p_queue->p_tail->p_next = p_cmd;
    }
    if( p_queue->p_head == NULL )
    {
        p_queue->p_head = p_cmd;
    }
    p_queue->p_tail = p_cmd;
    p_cmd->p_next = NULL;

    return VLC_SUCCESS;
}

command_t *QueueDequeue( queue_t *p_queue )
{
    if( p_queue->p_head == NULL )
    {
        return NULL;
    }
    else
    {
        command_t *p_ret = p_queue->p_head;
        if( p_queue->p_head == p_queue->p_tail )
        {
            p_queue->p_head = p_queue->p_tail = NULL;
        }
        else
        {
            p_queue->p_head = p_queue->p_head->p_next;
        }
        return p_ret;
    }
}

int QueueTransfer( queue_t *p_sink, queue_t *p_source )
{
    if( p_source->p_head == NULL ) {
        return VLC_SUCCESS;
    }

    if( p_sink->p_head == NULL ) {
        p_sink->p_head = p_source->p_head;
        p_sink->p_tail = p_source->p_tail;
    } else {
        p_sink->p_tail->p_next = p_source->p_head;
        p_sink->p_tail = p_source->p_tail;
    }
    p_source->p_head = p_source->p_tail = NULL;

    return VLC_SUCCESS;
}
