/*****************************************************************************
 * objects.c: vlc_object_t handling
 *****************************************************************************
 * Copyright (C) 2004-2008 VLC authors and VideoLAN
 * Copyright (C) 2006-2010 Rémi Denis-Courmont
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/**
 * \file
 * This file contains the functions to handle the vlc_object_t type
 *
 * Unless otherwise stated, functions in this file are not cancellation point.
 * All functions in this file are safe w.r.t. deferred cancellation.
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "../libvlc.h"
#include <vlc_aout.h>
#include "audio_output/aout_internal.h"

#include "vlc_interface.h"
#include "vlc_codec.h"

#include "variables.h"

#ifdef HAVE_SEARCH_H
# include <search.h>
#endif

#include <limits.h>
#include <assert.h>

#define vlc_children_foreach(pos, priv) \
    while (((void)(pos), (void)(priv), 0))

int vlc_object_init(vlc_object_t *restrict obj, vlc_object_t *parent,
                    const char *typename)
{
    vlc_object_internals_t *priv = malloc(sizeof (*priv));
    if (unlikely(priv == NULL))
        return -1;

    priv->parent = parent;
    priv->typename = typename;
    priv->var_root = NULL;
    vlc_mutex_init (&priv->var_lock);
    vlc_cond_init (&priv->var_wait);
    priv->resources = NULL;

    obj->priv = priv;
    obj->force = false;

    if (likely(parent != NULL))
    {
        obj->logger = parent->logger;
        obj->no_interact = parent->no_interact;
    }
    else
    {
        obj->logger = NULL;
        obj->no_interact = false;
    }

    return 0;
}

void *(vlc_custom_create)(vlc_object_t *parent, size_t length,
                          const char *typename)
{
    assert(length >= sizeof (vlc_object_t));

    vlc_object_t *obj = calloc(length, 1);
    if (unlikely(obj == NULL || vlc_object_init(obj, parent, typename))) {
        free(obj);
        obj = NULL;
    }
    return obj;
}

void *(vlc_object_create)(vlc_object_t *p_this, size_t i_size)
{
    return vlc_custom_create( p_this, i_size, "generic" );
}

const char *vlc_object_typename(const vlc_object_t *obj)
{
    return vlc_internals(obj)->typename;
}

vlc_object_t *(vlc_object_parent)(vlc_object_t *obj)
{
    return vlc_internals(obj)->parent;
}

void vlc_object_deinit(vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    assert(priv->resources == NULL);

    /* Destroy the associated variables. */
    int canc = vlc_savecancel();
    var_DestroyAll(obj);
    vlc_restorecancel(canc);

    free(priv);
}

void (vlc_object_delete)(vlc_object_t *obj)
{
    vlc_object_deinit(obj);
    free(obj);
}

void vlc_object_vaLog(vlc_object_t *obj, int prio, const char *module,
                      const char *file, unsigned line, const char *func,
                      const char *format, va_list ap)
{
    if (obj == NULL)
        return;

    const char *typename = vlc_object_typename(obj);
    /* FIXME: libvlc allows NULL type but modules don't */
    if (typename == NULL)
        typename = "generic";

    vlc_vaLog(&obj->logger, prio, typename, module, file, line, func,
              format, ap);
}

void vlc_object_Log(vlc_object_t *obj, int prio, const char *module,
                    const char *file, unsigned line, const char *func,
                    const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vlc_object_vaLog(obj, prio, module, file, line, func, format, ap);
    va_end(ap);
}

/**
 * Lists the children of an object.
 *
 * Fills a table of pointers to children object of an object, incrementing the
 * reference count for each of them.
 *
 * @param obj object whose children are to be listed
 * @param tab base address to hold the list of children [OUT]
 * @param max size of the table
 *
 * @return the actual numer of children (may be larger than requested).
 *
 * @warning The list of object can change asynchronously even before the
 * function returns. The list meant exclusively for debugging and tracing,
 * not for functional introspection of any kind.
 *
 * @warning Objects appear in the object tree early, and disappear late.
 * Most object properties are not accessible or not defined when the object is
 * accessed through this function.
 * For instance, the object cannot be used as a message log target
 * (because object flags are not accessible asynchronously).
 * Also type-specific object variables may not have been created yet, or may
 * already have been deleted.
 */
size_t vlc_list_children(vlc_object_t *obj, vlc_object_t **restrict tab,
                         size_t max)
{
    vlc_object_internals_t *priv;
    size_t count = 0;

    vlc_children_foreach(priv, vlc_internals(obj))
    {
         if (count < max)
             tab[count] = vlc_object_hold(vlc_externals(priv));
         count++;
    }
    return count;
}
