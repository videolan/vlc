/*****************************************************************************
 * picture_fifo.c : picture fifo functions
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
struct picture_fifo_t
{
    vlc_mutex_t lock;
    picture_t *p_first;
    picture_t **pp_last;
};

static void PictureFifoReset( picture_fifo_t *p_fifo )
{
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}
static void PictureFifoPush( picture_fifo_t *p_fifo, picture_t *p_picture )
{
    assert( !p_picture->p_next );
    *p_fifo->pp_last = p_picture;
    p_fifo->pp_last = &p_picture->p_next;
}
static picture_t *PictureFifoPop( picture_fifo_t *p_fifo )
{
    picture_t *p_picture = p_fifo->p_first;

    if( p_picture )
    {
        p_fifo->p_first = p_picture->p_next;
        if( !p_fifo->p_first )
            p_fifo->pp_last = &p_fifo->p_first;
    }
    return p_picture;
}

picture_fifo_t *picture_fifo_New(void)
{
    picture_fifo_t *p_fifo = malloc( sizeof(*p_fifo) );
    if( !p_fifo )
        return NULL;

    vlc_mutex_init( &p_fifo->lock );
    PictureFifoReset( p_fifo );
    return p_fifo;
}

void picture_fifo_Push( picture_fifo_t *p_fifo, picture_t *p_picture )
{
    vlc_mutex_lock( &p_fifo->lock );
    PictureFifoPush( p_fifo, p_picture );
    vlc_mutex_unlock( &p_fifo->lock );
}
picture_t *picture_fifo_Pop( picture_fifo_t *p_fifo )
{
    vlc_mutex_lock( &p_fifo->lock );
    picture_t *p_picture = PictureFifoPop( p_fifo );
    vlc_mutex_unlock( &p_fifo->lock );

    return p_picture;
}
picture_t *picture_fifo_Peek( picture_fifo_t *p_fifo )
{
    vlc_mutex_lock( &p_fifo->lock );
    picture_t *p_picture = p_fifo->p_first;
    if( p_picture )
        picture_Hold( p_picture );
    vlc_mutex_unlock( &p_fifo->lock );

    return p_picture;
}
void picture_fifo_Flush( picture_fifo_t *p_fifo, mtime_t i_date, bool b_below )
{
    picture_t *p_picture;

    vlc_mutex_lock( &p_fifo->lock );

    p_picture = p_fifo->p_first;
    PictureFifoReset( p_fifo );

    picture_fifo_t tmp;
    PictureFifoReset( &tmp );

    while( p_picture )
    {
        picture_t *p_next = p_picture->p_next;

        p_picture->p_next = NULL;
        if( (  b_below && p_picture->date <= i_date ) ||
            ( !b_below && p_picture->date >= i_date ) )
            PictureFifoPush( &tmp, p_picture );
        else
            PictureFifoPush( p_fifo, p_picture );
        p_picture = p_next;
    }
    vlc_mutex_unlock( &p_fifo->lock );

    for( ;; )
    {
        picture_t *p_picture = PictureFifoPop( &tmp );
        if( !p_picture )
            break;
        picture_Release( p_picture );
    }
}
void picture_fifo_OffsetDate( picture_fifo_t *p_fifo, mtime_t i_delta )
{
    vlc_mutex_lock( &p_fifo->lock );
    for( picture_t *p_picture = p_fifo->p_first; p_picture != NULL; )
    {
        p_picture->date += i_delta;
        p_picture = p_picture->p_next;
    }
    vlc_mutex_unlock( &p_fifo->lock );
}
void picture_fifo_Delete( picture_fifo_t *p_fifo )
{
    picture_fifo_Flush( p_fifo, INT64_MAX, true );
    vlc_mutex_destroy( &p_fifo->lock );
}

