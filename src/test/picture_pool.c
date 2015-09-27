/*****************************************************************************
 * picture_pool.c: test cases for picture_poo_t
 *****************************************************************************
 * Copyright (C) 2014 Rémi Denis-Courmont
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

#include <stdbool.h>
#undef NDEBUG
#include <assert.h>

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_picture_pool.h>

#define PICTURES 10

static video_format_t fmt;
static picture_pool_t *pool, *reserve;

static void test(bool zombie)
{
    picture_t *pics[PICTURES];

    pool = picture_pool_NewFromFormat(&fmt, PICTURES);
    assert(pool != NULL);

    for (unsigned i = 0; i < PICTURES; i++) {
        pics[i] = picture_pool_Get(pool);
        assert(pics[i] != NULL);
    }

    for (unsigned i = 0; i < PICTURES; i++)
        assert(picture_pool_Get(pool) == NULL);

    // Reserve currently assumes that all pictures are free (or reserved).
    //assert(picture_pool_Reserve(pool, 1) == NULL);

    for (unsigned i = 0; i < PICTURES / 2; i++)
        picture_Hold(pics[i]);

    for (unsigned i = 0; i < PICTURES / 2; i++)
        picture_Release(pics[i]);

    for (unsigned i = 0; i < PICTURES; i++) {
        void *plane = pics[i]->p[0].p_pixels;
        assert(plane != NULL);
        picture_Release(pics[i]);

        pics[i] = picture_pool_Get(pool);
        assert(pics[i] != NULL);
        assert(pics[i]->p[0].p_pixels == plane);
    }

    for (unsigned i = 0; i < PICTURES; i++)
        picture_Release(pics[i]);

    for (unsigned i = 0; i < PICTURES; i++) {
        pics[i] = picture_pool_Wait(pool);
        assert(pics[i] != NULL);
    }

    for (unsigned i = 0; i < PICTURES; i++)
        picture_Release(pics[i]);

    reserve = picture_pool_Reserve(pool, PICTURES / 2);
    assert(reserve != NULL);

    for (unsigned i = 0; i < PICTURES / 2; i++) {
        pics[i] = picture_pool_Get(pool);
        assert(pics[i] != NULL);
    }

    for (unsigned i = PICTURES / 2; i < PICTURES; i++) {
        assert(picture_pool_Get(pool) == NULL);
        pics[i] = picture_pool_Get(reserve);
        assert(pics[i] != NULL);
    }

    if (!zombie)
        for (unsigned i = 0; i < PICTURES; i++)
            picture_Release(pics[i]);

    picture_pool_Release(reserve);
    picture_pool_Release(pool);

    if (zombie)
        for (unsigned i = 0; i < PICTURES; i++)
            picture_Release(pics[i]);
}

int main(void)
{
    video_format_Setup(&fmt, VLC_CODEC_I420, 320, 200, 320, 200, 1, 1);

    pool = picture_pool_NewFromFormat(&fmt, PICTURES);
    assert(pool != NULL);
    assert(picture_pool_GetSize(pool) == PICTURES);

    reserve = picture_pool_Reserve(pool, PICTURES / 2);
    assert(reserve != NULL);

    picture_pool_Release(reserve);
    picture_pool_Release(pool);

    test(false);
    test(true);

    return 0;
}
