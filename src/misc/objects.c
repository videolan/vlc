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
    vlc_list_foreach(pos, &priv->children, siblings)

static void PrintObjectPrefix(vlc_object_t *obj, bool last)
{
    const char *str;

    if (obj->obj.parent == NULL)
        return;

    PrintObjectPrefix(obj->obj.parent, false);

    if (vlc_list_is_last(&vlc_internals(obj)->siblings,
                         &vlc_internals(obj->obj.parent)->children))
        str = last ? " \xE2\x94\x94" : "  ";
    else
        str = last ? " \xE2\x94\x9C" : " \xE2\x94\x82";

    fputs(str, stdout);
}

static void PrintObject(vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    int canc = vlc_savecancel ();

    PrintObjectPrefix(obj, true);
    printf("\xE2\x94\x80\xE2\x94%c\xE2\x95\xB4%p %s, %u refs\n",
           vlc_list_is_empty(&priv->children) ? 0x80 : 0xAC,
           (void *)obj, vlc_object_typename(obj), atomic_load(&priv->refs));

    vlc_restorecancel (canc);
}

static void DumpStructure(vlc_object_t *obj, unsigned level)
{
    PrintObject(obj);

    if (unlikely(level > 100))
    {
        msg_Warn (obj, "structure tree is too deep");
        return;
    }

    vlc_object_internals_t *priv = vlc_internals(obj);

    /* NOTE: nested locking here (due to recursive call) */
    vlc_mutex_lock (&vlc_internals(obj)->tree_lock);
    vlc_children_foreach(priv, priv)
        DumpStructure(vlc_externals(priv), level + 1);
    vlc_mutex_unlock (&vlc_internals(obj)->tree_lock);
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

    if (cmd[0] == 't')
    {
        flockfile(stdout);
        DumpStructure (obj, 0);
        funlockfile(stdout);
    }

    return VLC_SUCCESS;
}

static vlc_object_t *ObjectExists (vlc_object_t *root, void *obj)
{
    if (root == obj)
        return vlc_object_hold (root);

    vlc_object_internals_t *priv = vlc_internals(root);
    vlc_object_t *ret = NULL;

    /* NOTE: nested locking here (due to recursive call) */
    vlc_mutex_lock (&vlc_internals(root)->tree_lock);

    vlc_children_foreach(priv, priv)
    {
        ret = ObjectExists (vlc_externals (priv), obj);
        if (ret != NULL)
            break;
    }

    vlc_mutex_unlock (&vlc_internals(root)->tree_lock);
    return ret;
}

static int VarsCommand (vlc_object_t *obj, char const *cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    void *p;

    (void) cmd; (void) oldval; (void) data;

    if (sscanf (newval.psz_string, "%p", &p) == 1)
    {
        p = ObjectExists (obj, p);
        if (p == NULL)
        {
            msg_Err (obj, "no such object: %s", newval.psz_string);
            return VLC_ENOOBJ;
        }
        obj = p;
    }
    else
        vlc_object_hold (obj);

    printf(" o %p %s, parent %p\n", (void *)obj, vlc_object_typename(obj),
           (void *)obj->obj.parent);
    DumpVariables (obj);
    vlc_object_release (obj);

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

    priv->typename = typename;
    priv->psz_name = NULL;
    priv->var_root = NULL;
    vlc_mutex_init (&priv->var_lock);
    vlc_cond_init (&priv->var_wait);
    atomic_init (&priv->refs, 1);
    priv->pf_destructor = NULL;
    vlc_list_init(&priv->children);
    vlc_mutex_init (&priv->tree_lock);
    priv->resources = NULL;

    vlc_object_t *obj = (vlc_object_t *)(priv + 1);
    obj->obj.force = false;
    memset (obj + 1, 0, length - sizeof (*obj)); /* type-specific stuff */

    if (likely(parent != NULL))
    {
        vlc_object_internals_t *papriv = vlc_internals (parent);

        obj->obj.logger = parent->obj.logger;
        obj->obj.no_interact = parent->obj.no_interact;

        /* Attach the child to its parent (no lock needed) */
        obj->obj.parent = vlc_object_hold (parent);

        /* Attach the parent to its child (structure lock needed) */
        vlc_mutex_lock (&papriv->tree_lock);
        vlc_list_append(&priv->siblings, &papriv->children);
        vlc_mutex_unlock (&papriv->tree_lock);
    }
    else
    {
        obj->obj.no_interact = false;
        obj->obj.parent = NULL;

        /* TODO: should be in src/libvlc.c */
        int canc = vlc_savecancel ();
        var_Create (obj, "tree", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
        var_AddCallback (obj, "tree", TreeCommand, NULL);
        var_Create (obj, "vars", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
        var_AddCallback (obj, "vars", VarsCommand, NULL);
        vlc_restorecancel (canc);
    }

    return obj;
}

#undef vlc_object_create
/**
 * Allocates and initializes a vlc object.
 *
 * @param i_size object byte size
 *
 * @return the new object, or NULL on error.
 */
void *vlc_object_create( vlc_object_t *p_this, size_t i_size )
{
    return vlc_custom_create( p_this, i_size, "generic" );
}

#undef vlc_object_set_destructor
/**
 ****************************************************************************
 * Set the destructor of a vlc object
 *
 * This function sets the destructor of the vlc object. It will be called
 * when the object is destroyed when the its refcount reaches 0.
 * (It is called by the internal function vlc_object_destroy())
 *****************************************************************************/
void vlc_object_set_destructor( vlc_object_t *p_this,
                                vlc_destructor_t pf_destructor )
{
    vlc_object_internals_t *p_priv = vlc_internals(p_this );

    p_priv->pf_destructor = pf_destructor;
}

const char *vlc_object_typename(const vlc_object_t *obj)
{
    return vlc_internals(obj)->typename;
}

vlc_object_t *(vlc_object_parent)(vlc_object_t *obj)
{
    return obj->obj.parent;
}

static vlc_mutex_t name_lock = VLC_STATIC_MUTEX;

#undef vlc_object_set_name
int vlc_object_set_name(vlc_object_t *obj, const char *name)
{
    vlc_object_internals_t *priv = vlc_internals(obj);
    char *newname = name ? strdup (name) : NULL;
    char *oldname;

    vlc_mutex_lock (&name_lock);
    oldname = priv->psz_name;
    priv->psz_name = newname;
    vlc_mutex_unlock (&name_lock);

    free (oldname);
    return (priv->psz_name || !name) ? VLC_SUCCESS : VLC_ENOMEM;
}

char *vlc_object_get_name(const vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals(obj);
    char *name;

    vlc_mutex_lock (&name_lock);
    name = priv->psz_name ? strdup (priv->psz_name) : NULL;
    vlc_mutex_unlock (&name_lock);

    return name;
}

/**
 * Destroys a VLC object once it has no more references.
 *
 * This function must be called with cancellation disabled (currently).
 */
static void vlc_object_destroy( vlc_object_t *p_this )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    assert(p_priv->resources == NULL);

    /* Call the custom "subclass" destructor */
    if( p_priv->pf_destructor )
        p_priv->pf_destructor( p_this );

    if (unlikely(p_this->obj.parent == NULL))
    {
        /* TODO: should be in src/libvlc.c */
        var_DelCallback (p_this, "vars", VarsCommand, NULL);
        var_DelCallback (p_this, "tree", TreeCommand, NULL);
    }

    /* Destroy the associated variables. */
    var_DestroyAll( p_this );

    vlc_mutex_destroy (&p_priv->tree_lock);
    vlc_cond_destroy( &p_priv->var_wait );
    vlc_mutex_destroy( &p_priv->var_lock );
    free( p_priv->psz_name );
    free( p_priv );
}

static vlc_object_t *FindName (vlc_object_t *obj, const char *name)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    if (priv->psz_name != NULL && !strcmp (priv->psz_name, name))
        return vlc_object_hold (obj);

    vlc_object_t *found = NULL;

    /* NOTE: nested locking here (due to recursive call) */
    vlc_mutex_lock (&vlc_internals(obj)->tree_lock);

    vlc_children_foreach(priv, priv)
    {
        found = FindName (vlc_externals(priv), name);
        if (found != NULL)
            break;
    }

    /* NOTE: nested locking here (due to recursive call) */
    vlc_mutex_unlock (&vlc_internals(obj)->tree_lock);
    return found;
}

static int strcmp_void(const void *a, const void *b)
{
    return strcmp(a, b);
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
    vlc_object_t *p_found;

    /* The object name is not thread-safe, provides no warranty that the
     * object is fully initialized and still active, and that its owner can
     * deal with asynchronous and external state changes. There may be multiple
     * objects with the same name, and the function may fail even if a matching
     * object exists. DO NOT USE THIS IN NEW CODE. */
#ifndef NDEBUG
    /* This was officially deprecated on August 19 2009. For the convenience of
     * wannabe code janitors, this is the list of names that remain used
     * and unfixed since then. */
    static const char bad[][5] = { "v4l2", "zvbi" };
    if( bsearch( psz_name, bad, 2, 5, strcmp_void ) == NULL )
        return NULL;
    msg_Err( p_this, "looking for object \"%s\"... FIXME XXX", psz_name );
#endif

    vlc_mutex_lock (&name_lock);
    p_found = FindName (p_this, psz_name);
    vlc_mutex_unlock (&name_lock);
    return p_found;
}

#undef vlc_object_hold
/**
 * Increment an object reference counter.
 */
void * vlc_object_hold( vlc_object_t *p_this )
{
    vlc_object_internals_t *internals = vlc_internals( p_this );
    unsigned refs = atomic_fetch_add_explicit(&internals->refs, 1,
                                              memory_order_relaxed);

    assert (refs > 0); /* Avoid obvious freed object uses */
    (void) refs;
    return p_this;
}

#undef vlc_object_release
/**
 * Drops a reference to an object (decrements the reference count).
 * If the count reaches zero, the object is destroyed.
 */
void vlc_object_release (vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals(obj);
    unsigned refs = atomic_load_explicit(&priv->refs, memory_order_relaxed);

    /* Fast path */
    while (refs > 1)
    {
        if (atomic_compare_exchange_weak_explicit(&priv->refs, &refs, refs - 1,
                                   memory_order_release, memory_order_relaxed))
            return; /* There are still other references to the object */

        assert (refs > 0);
    }

    vlc_object_t *parent = obj->obj.parent;

    if (unlikely(parent == NULL))
    {   /* Destroying the root object */
        refs = atomic_fetch_sub_explicit(&priv->refs, 1, memory_order_relaxed);
        assert (refs == 1); /* nobody to race against in this case */
        /* no children can be left */
        assert(vlc_list_is_empty(&priv->children));

        int canc = vlc_savecancel ();
        vlc_object_destroy (obj);
        vlc_restorecancel (canc);
        return;
    }

    /* Slow path */
    vlc_object_internals_t *papriv = vlc_internals (parent);

    vlc_mutex_lock (&papriv->tree_lock);
    refs = atomic_fetch_sub_explicit(&priv->refs, 1, memory_order_release);
    assert (refs > 0);

    if (likely(refs == 1))
        /* Detach from parent to protect against vlc_object_find_name() */
        vlc_list_remove(&priv->siblings);
    vlc_mutex_unlock (&papriv->tree_lock);

    if (likely(refs == 1))
    {
        atomic_thread_fence(memory_order_acquire);
        /* no children can be left (because children reference their parent) */
        assert(vlc_list_is_empty(&priv->children));

        int canc = vlc_savecancel ();
        vlc_object_destroy (obj);
        vlc_restorecancel (canc);

        vlc_object_release (parent);
    }
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

    vlc_mutex_lock (&vlc_internals(obj)->tree_lock);
    vlc_children_foreach(priv, vlc_internals(obj))
    {
         if (count < max)
             tab[count] = vlc_object_hold(vlc_externals(priv));
         count++;
    }
    vlc_mutex_unlock (&vlc_internals(obj)->tree_lock);
    return count;
}
