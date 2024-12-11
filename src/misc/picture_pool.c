/*****************************************************************************
 * picture_pool.c : picture pool functions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 * Copyright (C) 2013-2015 Rémi Denis-Courmont
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
# include "config.h"
#endif
#include <assert.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_picture_pool.h>
#include <vlc_atomic.h>
#include <vlc_list.h>
#include "picture.h"

#define POOL_MAX 256

struct picture_pool_t {
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    vlc_atomic_rc_t    refs;
    bool released;
    struct vlc_list inuse_list;
    struct vlc_list available_list;
};

static void picture_pool_Destroy(picture_pool_t *pool)
{
    if (!vlc_atomic_rc_dec(&pool->refs))
        return;

    assert(vlc_list_is_empty(&pool->inuse_list));
    assert(vlc_list_is_empty(&pool->available_list));

    free(pool);
}

void picture_pool_Release(picture_pool_t *pool)
{
    vlc_mutex_lock(&pool->lock);

    /* Release pictures from both available and in-use lists */
    for (size_t i = 0; i < 2; ++i)
    {
        struct vlc_list *list = i == 0 ? &pool->available_list : &pool->inuse_list;

        picture_priv_t *priv;
        vlc_list_foreach(priv, list, pool_node)
        {
            assert(priv->pool == pool);
            vlc_list_remove(&priv->pool_node);
            picture_Release(&priv->picture);
        }
    }
    /* Prevent in-use cloned pictures to be added back to lists */
    pool->released = true;
    vlc_mutex_unlock(&pool->lock);
    picture_pool_Destroy(pool);
}

static void picture_pool_ReleaseClone(picture_t *clone)
{
    picture_priv_t *priv = container_of(clone, picture_priv_t, picture);

    /* Retrieve the original pic that was cloned */
    picture_t *original = priv->gc.opaque;
    picture_priv_t *original_priv = container_of(original, picture_priv_t, picture);

    picture_pool_t *pool = original_priv->pool;
    assert(pool != NULL);

    vlc_mutex_lock(&pool->lock);

    if (!pool->released)
    {
        vlc_list_remove(&original_priv->pool_node);
        vlc_list_append(&original_priv->pool_node, &pool->available_list);
        vlc_cond_signal(&pool->wait);
    }

    vlc_mutex_unlock(&pool->lock);

    picture_Release(original);

    picture_pool_Destroy(pool);
}

static picture_t *picture_pool_ClonePicture(picture_pool_t *pool,
                                            picture_t *picture)
{
    picture_t *clone = picture_InternalClone(picture, picture_pool_ReleaseClone,
                                             picture);
    if (clone != NULL) {
        assert(!picture_HasChainedPics(clone));
        vlc_atomic_rc_inc(&pool->refs);
    }
    return clone;
}

static void picture_pool_AppendPic(picture_pool_t *pool, picture_t *pic)
{
    picture_priv_t *priv = container_of(pic, picture_priv_t, picture);
    assert(priv->pool == NULL);
    vlc_list_append(&priv->pool_node, &pool->available_list);
    priv->pool = pool;
}

static picture_pool_t *
picture_pool_NewCommon(void)
{
    picture_pool_t *pool = malloc(sizeof(*pool));

    if (unlikely(pool == NULL))
        return NULL;

    vlc_list_init(&pool->inuse_list);
    vlc_list_init(&pool->available_list);

    vlc_mutex_init(&pool->lock);
    vlc_cond_init(&pool->wait);
    vlc_atomic_rc_init(&pool->refs);
    pool->released = false;

    return pool;
}

picture_pool_t *picture_pool_New(unsigned count, picture_t *const *tab)
{
    if (unlikely(count > POOL_MAX))
        return NULL;

    picture_pool_t *pool = picture_pool_NewCommon();
    if (unlikely(pool == NULL))
        return NULL;

    for (unsigned i = 0; i < count; ++i)
        picture_pool_AppendPic(pool, tab[i]);
    return pool;
}

picture_pool_t *picture_pool_NewFromFormat(const video_format_t *fmt,
                                           unsigned count)
{
    if (count == 0)
        vlc_assert_unreachable();
    if (unlikely(count > POOL_MAX))
        return NULL;

    picture_pool_t *pool = picture_pool_NewCommon();
    if (unlikely(pool == NULL))
        return NULL;

    for (unsigned i = 0; i < count; ++i)
    {
        picture_t *pic = picture_NewFromFormat(fmt);
        if (pic == NULL)
        {
            picture_pool_Release(pool);
            return NULL;
        }
        picture_pool_AppendPic(pool, pic);
    }

    return pool;
}

static picture_t *picture_pool_GetAvailableLocked(picture_pool_t *pool)
{
    picture_priv_t *priv = vlc_list_first_entry_or_null(&pool->available_list,
                                                        picture_priv_t,
                                                        pool_node);
    assert(priv != NULL);

    assert(priv->pool == pool);

    vlc_list_remove(&priv->pool_node);
    vlc_list_append(&priv->pool_node, &pool->inuse_list);

    return &priv->picture;
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{

    vlc_mutex_lock(&pool->lock);
    assert(vlc_atomic_rc_get(&pool->refs) > 0);

    if (vlc_list_is_empty(&pool->available_list))
    {
        vlc_mutex_unlock(&pool->lock);
        return NULL;
    }

    picture_t *pic = picture_pool_GetAvailableLocked(pool);

    vlc_mutex_unlock(&pool->lock);

    return picture_pool_ClonePicture(pool, pic);
}

picture_t *picture_pool_Wait(picture_pool_t *pool)
{
    vlc_mutex_lock(&pool->lock);
    assert(vlc_atomic_rc_get(&pool->refs) > 0);

    while (vlc_list_is_empty(&pool->available_list))
        vlc_cond_wait(&pool->wait, &pool->lock);

    picture_t *pic = picture_pool_GetAvailableLocked(pool);

    vlc_mutex_unlock(&pool->lock);

    return picture_pool_ClonePicture(pool, pic);
}
