/*****************************************************************************
 * picture_fifo.c : picture fifo functions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture_fifo.h>

/*****************************************************************************
 *
 *****************************************************************************/
struct picture_fifo_t {
    vlc_mutex_t lock;
    picture_t   *first;
    picture_t   *tail;
};

static void PictureFifoReset(picture_fifo_t *fifo)
{
    fifo->first    = NULL;
    fifo->tail     = NULL;
}
static void PictureFifoPush(picture_fifo_t *fifo, picture_t *picture)
{
    assert(!picture_HasChainedPics(picture));
    fifo->tail = vlc_picture_chain_Append( &fifo->first, fifo->tail, picture );
}
static picture_t *PictureFifoPop(picture_fifo_t *fifo)
{
    return vlc_picture_chain_PopFront( &fifo->first );
}

picture_fifo_t *picture_fifo_New(void)
{
    picture_fifo_t *fifo = malloc(sizeof(*fifo));
    if (!fifo)
        return NULL;

    vlc_mutex_init(&fifo->lock);
    PictureFifoReset(fifo);
    return fifo;
}

void picture_fifo_Push(picture_fifo_t *fifo, picture_t *picture)
{
    vlc_mutex_lock(&fifo->lock);
    PictureFifoPush(fifo, picture);
    vlc_mutex_unlock(&fifo->lock);
}
picture_t *picture_fifo_Pop(picture_fifo_t *fifo)
{
    vlc_mutex_lock(&fifo->lock);
    picture_t *picture = PictureFifoPop(fifo);
    vlc_mutex_unlock(&fifo->lock);

    return picture;
}
bool picture_fifo_IsEmpty(picture_fifo_t *fifo)
{
    vlc_mutex_lock(&fifo->lock);
    bool empty = fifo->first == NULL;
    vlc_mutex_unlock(&fifo->lock);

    return empty;
}
void picture_fifo_Flush(picture_fifo_t *fifo, vlc_tick_t date, bool flush_before)
{
    picture_t *picture;

    vlc_mutex_lock(&fifo->lock);

    picture_t *old_chain = fifo->first;
    PictureFifoReset(fifo);

    picture_fifo_t tmp;
    PictureFifoReset(&tmp);

    while (old_chain) {
        picture_t *picture = vlc_picture_chain_PopFront( &old_chain );

        if ((date == VLC_TICK_INVALID) ||
            ( flush_before && picture->date <= date) ||
            (!flush_before && picture->date >= date))
            PictureFifoPush(&tmp, picture);
        else
            PictureFifoPush(fifo, picture);
    }
    vlc_mutex_unlock(&fifo->lock);

    while ((picture = PictureFifoPop(&tmp)) != NULL)
        picture_Release(picture);
}
void picture_fifo_Delete(picture_fifo_t *fifo)
{
    picture_fifo_Flush(fifo, VLC_TICK_INVALID, true);
    free(fifo);
}

