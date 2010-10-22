/*****************************************************************************
 * vout_pictures.c :
 *****************************************************************************
 * Copyright (C) 2009-2010 Laurent Aimar
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
#include <libvlc.h>
#include <vlc_vout.h>
#include <vlc_picture_fifo.h>
#include <vlc_picture_pool.h>

#include "vout_internal.h"

/**
 * It retreives a picture from the vout or NULL if no pictures are
 * available yet.
 *
 * You MUST call vout_PutPicture or vout_ReleasePicture on it.
 *
 * You may use vout_HoldPicture(paired with vout_ReleasePicture) to keep a
 * read-only reference.
 */
picture_t *vout_GetPicture( vout_thread_t *p_vout )
{
    /* Get lock */
    vlc_mutex_lock( &p_vout->p->picture_lock );
    picture_t *p_pic = picture_pool_Get(p_vout->p->decoder_pool);
    if (p_pic) {
        picture_Reset(p_pic);
        p_pic->p_next = NULL;
    }
    vlc_mutex_unlock( &p_vout->p->picture_lock );

    return p_pic;
}

/**
 * It gives to the vout a picture to be displayed.
 *
 * The given picture MUST comes from vout_GetPicture.
 *
 * Becareful, after vout_PutPicture is called, picture_t::p_next cannot be
 * read/used.
 */
void vout_PutPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->p->picture_lock );

    p_pic->p_next = NULL;
    picture_fifo_Push(p_vout->p->decoder_fifo, p_pic);

    vlc_mutex_unlock( &p_vout->p->picture_lock );

    vout_control_Wake( &p_vout->p->control);
}

/**
 * It releases a picture retreived by vout_GetPicture.
 */
void vout_ReleasePicture( vout_thread_t *p_vout, picture_t *p_pic  )
{
    vlc_mutex_lock( &p_vout->p->picture_lock );

    picture_Release( p_pic );

    vlc_mutex_unlock( &p_vout->p->picture_lock );

    vout_control_Wake( &p_vout->p->control);
}

/**
 * It increment the reference counter of a picture retreived by
 * vout_GetPicture.
 */
void vout_HoldPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->p->picture_lock );

    picture_Hold( p_pic );

    vlc_mutex_unlock( &p_vout->p->picture_lock );
}

