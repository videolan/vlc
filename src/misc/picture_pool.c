/*****************************************************************************
 * picture_pool.c : picture pool functions
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
#include <vlc_picture_pool.h>

/*****************************************************************************
 *
 *****************************************************************************/
struct picture_release_sys_t
{
    /* Saved release */
    void (*pf_release)( picture_t * );
    picture_release_sys_t *p_release_sys;

    /* */
    int  (*pf_lock)( picture_t * );
    void (*pf_unlock)( picture_t * );

    /* */
    int64_t i_tick;
};

struct picture_pool_t
{
    int64_t i_tick;

    int i_picture;
    picture_t **pp_picture;
};

static void PicturePoolPictureRelease( picture_t * );

picture_pool_t *picture_pool_NewExtended( const picture_pool_configuration_t *cfg )
{
    picture_pool_t *p_pool = calloc( 1, sizeof(*p_pool) );
    if( !p_pool )
        return NULL;

    p_pool->i_tick = 1;
    p_pool->i_picture = cfg->picture_count;
    p_pool->pp_picture = calloc( p_pool->i_picture, sizeof(*p_pool->pp_picture) );
    if( !p_pool->pp_picture )
    {
        free( p_pool );
        return NULL;
    }

    for( int i = 0; i < cfg->picture_count; i++ )
    {
        picture_t *p_picture = cfg->picture[i];

        /* The pool must be the only owner of the picture */
        assert( p_picture->i_refcount == 1 );

        /* Install the new release callback */
        picture_release_sys_t *p_release_sys = malloc( sizeof(*p_release_sys) );
        if( !p_release_sys )
            abort();
        p_release_sys->pf_release    = p_picture->pf_release;
        p_release_sys->p_release_sys = p_picture->p_release_sys;
        p_release_sys->pf_lock       = cfg->lock;
        p_release_sys->pf_unlock     = cfg->unlock;
        p_release_sys->i_tick        = 0;

        p_picture->i_refcount = 0;
        p_picture->pf_release = PicturePoolPictureRelease;
        p_picture->p_release_sys = p_release_sys;

        /* */
        p_pool->pp_picture[i] = p_picture;
    }
    return p_pool;

}

picture_pool_t *picture_pool_New( int i_picture, picture_t *pp_picture[] )
{
    picture_pool_configuration_t cfg;

    memset( &cfg, 0, sizeof(cfg) );
    cfg.picture_count = i_picture;
    cfg.picture       = pp_picture;

    return picture_pool_NewExtended( &cfg );
}

picture_pool_t *picture_pool_NewFromFormat( const video_format_t *p_fmt, int i_picture )
{
    picture_t *pp_picture[i_picture];

    for( int i = 0; i < i_picture; i++ )
    {
        pp_picture[i] = picture_New( p_fmt->i_chroma,
                                     p_fmt->i_width, p_fmt->i_height,
                                     p_fmt->i_aspect );
        if( !pp_picture[i] )
            goto error;
    }
    picture_pool_t *p_pool = picture_pool_New( i_picture, pp_picture );
    if( !p_pool )
        goto error;

    return p_pool;

error:
    for( int i = 0; i < i_picture; i++ )
    {
        if( !pp_picture[i] )
            break;
        picture_Release( pp_picture[i] );
    }
    return NULL;
}

void picture_pool_Delete( picture_pool_t *p_pool )
{
    for( int i = 0; i < p_pool->i_picture; i++ )
    {
        picture_t *p_picture = p_pool->pp_picture[i];
        picture_release_sys_t *p_release_sys = p_picture->p_release_sys;

        assert( p_picture->i_refcount == 0 );

        /* Restore old release callback */
        p_picture->i_refcount = 1;
        p_picture->pf_release = p_release_sys->pf_release;
        p_picture->p_release_sys = p_release_sys->p_release_sys;

        picture_Release( p_picture );

        free( p_release_sys );
    }
    free( p_pool->pp_picture );
    free( p_pool );
}

picture_t *picture_pool_Get( picture_pool_t *p_pool )
{
    for( int i = 0; i < p_pool->i_picture; i++ )
    {
        picture_t *p_picture = p_pool->pp_picture[i];
        if( p_picture->i_refcount > 0 )
            continue;

        picture_release_sys_t *p_release_sys = p_picture->p_release_sys;
        if( p_release_sys->pf_lock && p_release_sys->pf_lock(p_picture) )
            continue;

        /* */
        p_picture->p_release_sys->i_tick = p_pool->i_tick++;
        picture_Hold( p_picture );
        return p_picture;
    }
    return NULL;
}

void picture_pool_NonEmpty( picture_pool_t *p_pool, bool b_reset )
{
    picture_t *p_old = NULL;

    for( int i = 0; i < p_pool->i_picture; i++ )
    {
        picture_t *p_picture = p_pool->pp_picture[i];

        if( b_reset )
            p_picture->i_refcount = 0;
        else if( p_picture->i_refcount == 0 )
            return;
        else if( !p_old || p_picture->p_release_sys->i_tick < p_old->p_release_sys->i_tick )
            p_old = p_picture;
    }
    if( !b_reset && p_old )
        p_old->i_refcount = 0;
}

static void PicturePoolPictureRelease( picture_t *p_picture )
{
    assert( p_picture->i_refcount > 0 );

    if( --p_picture->i_refcount > 0 )
        return;

    picture_release_sys_t *p_release_sys = p_picture->p_release_sys;
    if( p_release_sys->pf_unlock )
        p_release_sys->pf_unlock( p_picture );
}

