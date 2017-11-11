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

static void PrintObject (vlc_object_t *obj, const char *prefix)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    int canc = vlc_savecancel ();
    printf (" %so %p %s, %u refs, parent %p\n", prefix, (void *)obj,
            obj->obj.object_type, atomic_load(&priv->refs),
            (void *)obj->obj.parent);
    vlc_restorecancel (canc);
}

static void DumpStructure (vlc_object_t *obj, unsigned level, char *psz_foo)
{
    char back = psz_foo[level];

    psz_foo[level] = '\0';
    PrintObject (obj, psz_foo);
    psz_foo[level] = back;

    if (level / 2 >= MAX_DUMPSTRUCTURE_DEPTH)
    {
        msg_Warn (obj, "structure tree is too deep");
        return;
    }

    vlc_object_internals_t *priv = vlc_internals(obj);

    /* NOTE: nested locking here (due to recursive call) */
    vlc_mutex_lock (&vlc_internals(obj)->tree_lock);
    for (priv = priv->first; priv != NULL; priv = priv->next)
    {
        if (level > 0)
        {
            assert(level >= 2);
            psz_foo[level - 1] = ' ';

            if (psz_foo[level - 2] == '`')
                psz_foo[level - 2] = ' ';
        }

        psz_foo[level] = priv->next ? '|' : '`';
        psz_foo[level + 1] = '-';
        psz_foo[level + 2] = '\0';

        DumpStructure (vlc_externals(priv), level + 2, psz_foo);
    }
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
        char psz_foo[2 * MAX_DUMPSTRUCTURE_DEPTH + 1];

        psz_foo[0] = '|';
        DumpStructure (obj, 0, psz_foo);
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

    for (priv = priv->first; priv != NULL && ret == NULL; priv = priv->next)
        ret = ObjectExists (vlc_externals (priv), obj);

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

    PrintObject (obj, "");
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
    priv->psz_name = NULL;
    priv->var_root = NULL;
    vlc_mutex_init (&priv->var_lock);
    vlc_cond_init (&priv->var_wait);
    atomic_init (&priv->refs, 1);
    priv->pf_destructor = NULL;
    priv->prev = NULL;
    priv->first = NULL;
    vlc_mutex_init (&priv->tree_lock);
    priv->resources = NULL;

    vlc_object_t *obj = (vlc_object_t *)(priv + 1);
    obj->obj.object_type = typename;
    obj->obj.header = NULL;
    obj->obj.force = false;
    memset (obj + 1, 0, length - sizeof (*obj)); /* type-specific stuff */

    if (likely(parent != NULL))
    {
        vlc_object_internals_t *papriv = vlc_internals (parent);

        obj->obj.flags = parent->obj.flags;
        obj->obj.libvlc = parent->obj.libvlc;

        /* Attach the child to its parent (no lock needed) */
        obj->obj.parent = vlc_object_hold (parent);

        /* Attach the parent to its child (structure lock needed) */
        vlc_mutex_lock (&papriv->tree_lock);
        priv->next = papriv->first;
        if (priv->next != NULL)
            priv->next->prev = priv;
        papriv->first = priv;
        vlc_mutex_unlock (&papriv->tree_lock);
    }
    else
    {
        libvlc_int_t *self = (libvlc_int_t *)obj;

        obj->obj.flags = 0;
        obj->obj.libvlc = self;
        obj->obj.parent = NULL;
        priv->next = NULL;

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

#undef vlc_object_get_name
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
    free( p_this->obj.header );
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

    for (priv = priv->first; priv != NULL && found == NULL; priv = priv->next)
        found = FindName (vlc_externals(priv), name);

    /* NOTE: nested locking here (due to recursive call) */
    vlc_mutex_unlock (&vlc_internals(obj)->tree_lock);
    return found;
}

#undef vlc_object_find_name
/**
 * Finds a named object and increment its reference count.
 * Beware that objects found in this manner can be "owned" by another thread,
 * be of _any_ type, and be attached to any module (if any). With such an
 * object reference, you can set or get object variables, emit log messages,
 * and read write-once object parameters (obj.object_type, etc).
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
    static const char bad[][11] = { "adjust", "clone", "colorthres",
        "erase", "extract", "gradient", "logo", "marq", "motionblur", "puzzle",
        "rotate", "sharpen", "transform", "v4l2", "wall" };
    static const char poor[][13] = { "invert", "magnify", "motiondetect",
        "psychedelic", "ripple", "wave" };
    if( bsearch( psz_name, bad, 15, 11, (void *)strcmp ) == NULL
     && bsearch( psz_name, poor, 6, 13, (void *)strcmp ) == NULL )
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
#ifndef NDEBUG
    unsigned refs = atomic_fetch_add (&internals->refs, 1);
    assert (refs > 0); /* Avoid obvious freed object uses */
#else
    atomic_fetch_add (&internals->refs, 1);
#endif
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
    unsigned refs = atomic_load (&priv->refs);

    /* Fast path */
    while (refs > 1)
    {
        if (atomic_compare_exchange_weak (&priv->refs, &refs, refs - 1))
            return; /* There are still other references to the object */

        assert (refs > 0);
    }

    vlc_object_t *parent = obj->obj.parent;

    if (unlikely(parent == NULL))
    {   /* Destroying the root object */
        refs = atomic_fetch_sub (&priv->refs, 1);
        assert (refs == 1); /* nobody to race against in this case */

        assert (priv->first == NULL); /* no children can be left */

        int canc = vlc_savecancel ();
        vlc_object_destroy (obj);
        vlc_restorecancel (canc);
        return;
    }

    /* Slow path */
    vlc_object_internals_t *papriv = vlc_internals (parent);

    vlc_mutex_lock (&papriv->tree_lock);
    refs = atomic_fetch_sub (&priv->refs, 1);
    assert (refs > 0);

    if (likely(refs == 1))
    {   /* Detach from parent to protect against vlc_object_find_name() */
        vlc_object_internals_t *prev = priv->prev;
        vlc_object_internals_t *next = priv->next;

        if (prev != NULL)
        {
            assert (prev->next == priv);
            prev->next = next;
        }
        else
        {
            assert (papriv->first == priv);
            papriv->first = next;
        }
        if (next != NULL)
        {
            assert (next->prev == priv);
            next->prev = prev;
        }
    }
    vlc_mutex_unlock (&papriv->tree_lock);

    if (likely(refs == 1))
    {
        assert (priv->first == NULL); /* no children can be left */

        int canc = vlc_savecancel ();
        vlc_object_destroy (obj);
        vlc_restorecancel (canc);

        vlc_object_release (parent);
    }
}

#undef vlc_list_children
/**
 * Gets the list of children of an object, and increment their reference
 * count.
 * @return a list (possibly empty) or NULL in case of error.
 */
vlc_list_t *vlc_list_children( vlc_object_t *obj )
{
    vlc_list_t *l = malloc (sizeof (*l));
    if (unlikely(l == NULL))
        return NULL;

    l->i_count = 0;
    l->p_values = NULL;

    vlc_object_internals_t *priv;
    unsigned count = 0;

    vlc_mutex_lock (&vlc_internals(obj)->tree_lock);
    for (priv = vlc_internals (obj)->first; priv; priv = priv->next)
         count++;

    if (count > 0)
    {
        l->p_values = vlc_alloc (count, sizeof (vlc_value_t));
        if (unlikely(l->p_values == NULL))
        {
            vlc_mutex_unlock (&vlc_internals(obj)->tree_lock);
            free (l);
            return NULL;
        }
        l->i_count = count;
    }

    unsigned i = 0;

    for (priv = vlc_internals (obj)->first; priv; priv = priv->next)
        l->p_values[i++].p_address = vlc_object_hold (vlc_externals (priv));
    vlc_mutex_unlock (&vlc_internals(obj)->tree_lock);
    return l;
}

/*****************************************************************************
 * vlc_list_release: free a list previously allocated by vlc_list_find
 *****************************************************************************
 * This function decreases the refcount of all objects in the list and
 * frees the list.
 *****************************************************************************/
void vlc_list_release( vlc_list_t *p_list )
{
    for( int i = 0; i < p_list->i_count; i++ )
        vlc_object_release( p_list->p_values[i].p_address );

    free( p_list->p_values );
    free( p_list );
}
