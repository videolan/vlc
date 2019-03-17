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

static vlc_mutex_t tree_lock = VLC_STATIC_MUTEX;
static struct vlc_list tree_list = VLC_LIST_INITIALIZER(&tree_list);

#define vlc_children_foreach(pos, priv) \
    vlc_list_foreach(pos, &tree_list, list) \
        if (pos->parent == vlc_externals(priv))

static bool ObjectIsLastChild(vlc_object_t *obj, vlc_object_t *parent)
{
    struct vlc_list *node = &vlc_internals(obj)->list;

    while ((node = node->next) != &tree_list) {
        vlc_object_internals_t *priv =
            container_of(node, vlc_object_internals_t, list);

        if (priv->parent == parent)
            return false;
    }
    return true;
}

static bool ObjectHasChildLocked(vlc_object_t *obj)
{
    vlc_object_internals_t *priv;

    vlc_children_foreach(priv, vlc_internals(obj))
        return true;
    return false;
}

static void PrintObjectPrefix(vlc_object_t *obj, FILE *output, bool last)
{
    vlc_object_t *parent = vlc_object_parent(obj);
    const char *str;

    if (parent == NULL)
        return;

    PrintObjectPrefix(parent, output, false);

    if (ObjectIsLastChild(obj, parent))
        str = last ? " \xE2\x94\x94" : "  ";
    else
        str = last ? " \xE2\x94\x9C" : " \xE2\x94\x82";

    fputs(str, output);
}

static void PrintObject(vlc_object_t *obj, FILE *output)
{
    int canc = vlc_savecancel ();

    PrintObjectPrefix(obj, output, true);
    fprintf(output, "\xE2\x94\x80\xE2\x94%c\xE2\x95\xB4%p %s\n",
           ObjectHasChildLocked(obj) ? 0xAC : 0x80,
           (void *)obj, vlc_object_typename(obj));

    vlc_restorecancel (canc);
}

void DumpStructureLocked(vlc_object_t *obj, FILE *output, unsigned level)
{
    PrintObject(obj, output);

    if (unlikely(level > 100))
    {
        msg_Warn (obj, "structure tree is too deep");
        return;
    }

    vlc_object_internals_t *priv;

    vlc_children_foreach(priv, vlc_internals(obj))
        DumpStructureLocked(vlc_externals(priv), output, level + 1);
}

/**
 * Prints the VLC object tree
 *
 * This function prints either an ASCII tree showing the connections between
 * vlc objects, and additional information such as their refcount, thread ID,
 * etc. (command "tree"), or the same data as a simple list (command "list").
 */
static int TreeCommand (vlc_object_t *obj, char const *cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    (void) cmd; (void) oldval; (void) newval; (void) data;

    flockfile(stdout);
    vlc_mutex_lock(&tree_lock);
    DumpStructureLocked(obj, stdout, 0);
    vlc_mutex_unlock(&tree_lock);
    funlockfile(stdout);
    return VLC_SUCCESS;
}

#undef vlc_custom_create
void *vlc_custom_create (vlc_object_t *parent, size_t length,
                         const char *typename)
{
    /* NOTE:
     * VLC objects are laid out as follow:
     * - first the LibVLC-private per-object data,
     * - then VLC_COMMON members from vlc_object_t,
     * - finally, the type-specific data (if any).
     *
     * This function initializes the LibVLC and common data,
     * and zeroes the rest.
     */
    assert (length >= sizeof (vlc_object_t));

    vlc_object_internals_t *priv = malloc (sizeof (*priv) + length);
    if (unlikely(priv == NULL))
        return NULL;

    priv->parent = parent;
    priv->typename = typename;
    priv->var_root = NULL;
    vlc_mutex_init (&priv->var_lock);
    vlc_cond_init (&priv->var_wait);
    priv->pf_destructor = NULL;
    priv->resources = NULL;

    vlc_object_t *obj = (vlc_object_t *)(priv + 1);
    obj->obj.force = false;
    memset (obj + 1, 0, length - sizeof (*obj)); /* type-specific stuff */

    if (likely(parent != NULL))
    {
        obj->obj.logger = parent->obj.logger;
        obj->obj.no_interact = parent->obj.no_interact;

        /* Attach the parent to its child (structure lock needed) */
        vlc_mutex_lock(&tree_lock);
        vlc_list_append(&priv->list, &tree_list);
        vlc_mutex_unlock(&tree_lock);
    }
    else
    {
        obj->obj.no_interact = false;

        /* TODO: should be in src/libvlc.c */
        int canc = vlc_savecancel ();
        var_Create (obj, "tree", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
        var_AddCallback (obj, "tree", TreeCommand, NULL);
        vlc_restorecancel (canc);
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

void (vlc_object_delete)(vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    assert(priv->resources == NULL);

    int canc = vlc_savecancel();

    /* Call the custom "subclass" destructor */
    if (priv->pf_destructor != NULL)
        priv->pf_destructor(obj);

    if (unlikely(priv->parent == NULL))
    {
        /* TODO: should be in src/libvlc.c */
        var_DelCallback(obj, "tree", TreeCommand, NULL);
    }
    else
    {
        vlc_mutex_lock(&tree_lock);
        vlc_list_remove(&priv->list);
        vlc_mutex_unlock(&tree_lock);
    }

    /* Destroy the associated variables. */
    var_DestroyAll(obj);
    vlc_restorecancel(canc);

    vlc_cond_destroy(&priv->var_wait);
    vlc_mutex_destroy(&priv->var_lock);
    free(priv);
}

#undef vlc_object_find_name
/**
 * Finds a named object and increment its reference count.
 * Beware that objects found in this manner can be "owned" by another thread,
 * be of _any_ type, and be attached to any module (if any). With such an
 * object reference, you can set or get object variables, emit log messages.
 * You CANNOT cast the object to a more specific object type, and you
 * definitely cannot invoke object type-specific callbacks with this.
 *
 * @param p_this object to search from
 * @param psz_name name of the object to search for
 *
 * @return a matching object (must be released by the caller),
 * or NULL on error.
 */
vlc_object_t *vlc_object_find_name( vlc_object_t *p_this, const char *psz_name )
{
    (void) p_this; (void) psz_name;
    return NULL;
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

    vlc_vaLog(&obj->obj.logger, prio, typename, module, file, line, func,
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

    vlc_mutex_lock(&tree_lock);
    vlc_children_foreach(priv, vlc_internals(obj))
    {
         if (count < max)
             tab[count] = vlc_object_hold(vlc_externals(priv));
         count++;
    }
    vlc_mutex_unlock(&tree_lock);
    return count;
}
