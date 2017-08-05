/*****************************************************************************
 * event.c: event thread for broken old video output display plugins
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
# include <config.h>
#endif

#include <stdnoreturn.h>
#include <stdlib.h>
#include <string.h>
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_vout_display.h>

#include "event_thread.h"

struct vout_display_event_thread {
    vout_display_t *vd;
    block_fifo_t *fifo;
    vlc_thread_t thread;
};

noreturn static void *VoutDisplayEventKeyDispatch(void *data)
{
    vout_display_event_thread_t *vdet = data;
    vout_display_t *vd = vdet->vd;
    block_fifo_t *fifo = vdet->fifo;

    for (;;) {
        block_t *event = block_FifoGet(fifo);

        int cancel = vlc_savecancel();
        int key;

        memcpy(&key, event->p_buffer, sizeof (key));
        block_Release(event);
        vout_display_SendEventKey(vd, key);
        vlc_restorecancel(cancel);
    }
}

void VoutDisplayEventKey(vout_display_event_thread_t *vdet, int key)
{
    if (unlikely(vdet == NULL))
        return;

    block_t *event = block_Alloc(sizeof (key));
    if (likely(event != NULL)) {
        memcpy(event->p_buffer, &key, sizeof (key));
        block_FifoPut(vdet->fifo, event);
    }
}

struct vout_display_event_thread *
VoutDisplayEventCreateThread(vout_display_t *vd)
{
    vout_display_event_thread_t *vdet = malloc(sizeof (*vdet));
    if (unlikely(vdet == NULL))
        return NULL;

    vdet->vd = vd;
    vdet->fifo = block_FifoNew();
    if (unlikely(vdet->fifo == NULL)) {
        free(vdet);
        return NULL;
    }

    if (vlc_clone(&vdet->thread, VoutDisplayEventKeyDispatch, vdet,
                  VLC_THREAD_PRIORITY_LOW)) {
        block_FifoRelease(vdet->fifo);
        free(vdet);
        return NULL;
    }
    return vdet;
}

void VoutDisplayEventKillThread(vout_display_event_thread_t *vdet)
{
    vlc_cancel(vdet->thread);
    vlc_join(vdet->thread, NULL);
    block_FifoRelease(vdet->fifo);
    free(vdet);
}
