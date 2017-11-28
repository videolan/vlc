/*****************************************************************************
 * objres.c: vlc_object_t resources
 *****************************************************************************
 * Copyright (C) 2017 Rémi Denis-Courmont
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
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>

#include <vlc_common.h>

#include "libvlc.h"
#include "variables.h"

struct vlc_res
{
    struct vlc_res *prev;
    void (*release)(void *);
    max_align_t payload[];
};

static struct vlc_res **vlc_obj_res(vlc_object_t *obj)
{
    return &vlc_internals(obj)->resources;
}

void *vlc_objres_new(size_t size, void (*release)(void *))
{
    if (unlikely(add_overflow(sizeof (struct vlc_res), size, &size)))
    {
        errno = ENOMEM;
        return NULL;
    }

    struct vlc_res *res = malloc(size);
    if (unlikely(res == NULL))
        return NULL;

    res->release = release;
    return res->payload;
}

void vlc_objres_push(vlc_object_t *obj, void *data)
{
    struct vlc_res **restrict pp = vlc_obj_res(obj);
    struct vlc_res *res = container_of(data, struct vlc_res, payload);

    res->prev = *pp;
    *pp = res;
}

static void *vlc_objres_pop(vlc_object_t *obj)
{
    struct vlc_res **restrict pp = vlc_obj_res(obj);
    struct vlc_res *res = *pp;

    if (res == NULL)
        return NULL;
    *pp = res->prev;
    return res->payload;
}

void vlc_objres_clear(vlc_object_t *obj)
{
    void *data;

    while ((data = vlc_objres_pop(obj)) != NULL)
    {
        struct vlc_res *res = container_of(data, struct vlc_res, payload);

        res->release(res->payload);
        free(res);
    }
}

void vlc_objres_remove(vlc_object_t *obj, void *data,
                       bool (*match)(void *, void *))
{
    struct vlc_res **restrict pp = vlc_obj_res(obj);

    /* With a doubly-linked list, this function could have constant complexity.
     * But that would require one more pointer per resource.
     *
     * Any given list should contain a fairly small number of resources,
     * and in most cases, the resources are destroyed implicitly by
     * vlc_objres_clear().
     */
    for (;;)
    {
        struct vlc_res *res = *pp;

        assert(res != NULL); /* invalid free? */

        if (match(res->payload, data))
        {
            *pp = res->prev;
            res->release(res->payload);
            free(res);
            return;
        }

        pp = &res->prev;
    }
}

static void dummy_release(void *data)
{
    (void) data;
}

static bool ptrcmp(void *a, void *b)
{
    return a == b;
}

void *vlc_obj_malloc(vlc_object_t *obj, size_t size)
{
    void *ptr = vlc_objres_new(size, dummy_release);
    if (likely(ptr != NULL))
        vlc_objres_push(obj, ptr);
    return ptr;
}

static void *vlc_obj_alloc_common(vlc_object_t *obj, size_t nmemb, size_t size,
                                  bool do_memset)
{
    size_t tabsize;
    if (mul_overflow(nmemb, size, &tabsize))
    {
        errno = ENOMEM;
        return NULL;
    }

    void *ptr = vlc_objres_new(tabsize, dummy_release);
    if (likely(ptr != NULL))
    {
        if (do_memset)
            memset(ptr, 0, tabsize);
        vlc_objres_push(obj, ptr);
    }
    return ptr;
}

void *vlc_obj_calloc(vlc_object_t *obj, size_t nmemb, size_t size)
{
    return vlc_obj_alloc_common(obj, nmemb, size, true);
}

void vlc_obj_free(vlc_object_t *obj, void *ptr)
{
    vlc_objres_remove(obj, ptr, ptrcmp);
}
