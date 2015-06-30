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


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DumpCommand( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

static vlc_object_t * FindName ( vlc_object_internals_t *, const char * );
static void PrintObject( vlc_object_internals_t *, const char * );
static void DumpStructure( vlc_object_internals_t *, unsigned, char * );

static vlc_list_t   * NewList       ( int );

static void vlc_object_destroy( vlc_object_t *p_this );

/*****************************************************************************
 * Local structure lock
 *****************************************************************************/
static void libvlc_lock (libvlc_int_t *p_libvlc)
{
    vlc_mutex_lock (&(libvlc_priv (p_libvlc)->structure_lock));
}

static void libvlc_unlock (libvlc_int_t *p_libvlc)
{
    vlc_mutex_unlock (&(libvlc_priv (p_libvlc)->structure_lock));
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

    vlc_object_t *obj = (vlc_object_t *)(priv + 1);
    obj->psz_object_type = typename;
    obj->psz_header = NULL;
    obj->b_force = false;
    memset (obj + 1, 0, length - sizeof (*obj)); /* type-specific stuff */

    if (likely(parent != NULL))
    {
        vlc_object_internals_t *papriv = vlc_internals (parent);

        obj->i_flags = parent->i_flags;
        obj->p_libvlc = parent->p_libvlc;

        /* Attach the child to its parent (no lock needed) */
        obj->p_parent = vlc_object_hold (parent);

        /* Attach the parent to its child (structure lock needed) */
        libvlc_lock (obj->p_libvlc);
        priv->next = papriv->first;
        if (priv->next != NULL)
            priv->next->prev = priv;
        papriv->first = priv;
        libvlc_unlock (obj->p_libvlc);
    }
    else
    {
        libvlc_int_t *self = (libvlc_int_t *)obj;

        obj->i_flags = 0;
        obj->p_libvlc = self;
        obj->p_parent = NULL;
        priv->next = NULL;
        vlc_mutex_init (&(libvlc_priv (self)->structure_lock));

        /* TODO: should be in src/libvlc.c */
        int canc = vlc_savecancel ();
        var_Create (obj, "tree", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
        var_AddCallback (obj, "tree", DumpCommand, obj);
        var_Create (obj, "vars", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
        var_AddCallback (obj, "vars", DumpCommand, obj);
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

    /* Call the custom "subclass" destructor */
    if( p_priv->pf_destructor )
        p_priv->pf_destructor( p_this );

    if (unlikely(p_this == VLC_OBJECT(p_this->p_libvlc)))
    {
        /* TODO: should be in src/libvlc.c */
        var_DelCallback (p_this, "tree", DumpCommand, p_this);
        var_DelCallback (p_this, "vars", DumpCommand, p_this);
    }

    /* Destroy the associated variables. */
    var_DestroyAll( p_this );

    vlc_cond_destroy( &p_priv->var_wait );
    vlc_mutex_destroy( &p_priv->var_lock );

    free( p_this->psz_header );

    free( p_priv->psz_name );

    if( VLC_OBJECT(p_this->p_libvlc) == p_this )
        vlc_mutex_destroy (&(libvlc_priv ((libvlc_int_t *)p_this)->structure_lock));

    free( p_priv );
}


#undef vlc_object_find_name
/**
 * Finds a named object and increment its reference count.
 * Beware that objects found in this manner can be "owned" by another thread,
 * be of _any_ type, and be attached to any module (if any). With such an
 * object reference, you can set or get object variables, emit log messages,
 * and read write-once object parameters (psz_object_type, etc).
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

    libvlc_lock (p_this->p_libvlc);
    vlc_mutex_lock (&name_lock);
    p_found = FindName (vlc_internals (p_this), psz_name);
    vlc_mutex_unlock (&name_lock);
    libvlc_unlock (p_this->p_libvlc);
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
void vlc_object_release( vlc_object_t *p_this )
{
    vlc_object_internals_t *internals = vlc_internals( p_this );
    vlc_object_t *parent = NULL;
    unsigned refs = atomic_load (&internals->refs);

    /* Fast path */
    while (refs > 1)
    {
        if (atomic_compare_exchange_weak (&internals->refs, &refs, refs - 1))
            return; /* There are still other references to the object */

        assert (refs > 0);
    }

    /* Slow path */
    libvlc_lock (p_this->p_libvlc);
    refs = atomic_fetch_sub (&internals->refs, 1);
    assert (refs > 0);

    if (likely(refs == 1))
    {
        /* Detach from parent to protect against vlc_object_find_name() */
        parent = p_this->p_parent;
        if (likely(parent))
        {
           /* Unlink */
           vlc_object_internals_t *prev = internals->prev;
           vlc_object_internals_t *next = internals->next;

           if (prev != NULL)
               prev->next = next;
           else
               vlc_internals (parent)->first = next;
           if (next != NULL)
               next->prev = prev;
        }

        /* We have no children */
        assert (internals->first == NULL);
    }
    libvlc_unlock (p_this->p_libvlc);

    if (likely(refs == 1))
    {
        int canc = vlc_savecancel ();
        vlc_object_destroy( p_this );
        vlc_restorecancel (canc);
        if (parent)
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
    vlc_list_t *l;
    vlc_object_internals_t *priv;
    unsigned count = 0;

    libvlc_lock (obj->p_libvlc);
    for (priv = vlc_internals (obj)->first; priv; priv = priv->next)
         count++;
    l = NewList (count);
    if (likely(l != NULL))
    {
        unsigned i = 0;

        for (priv = vlc_internals (obj)->first; priv; priv = priv->next)
            l->p_values[i++].p_address = vlc_object_hold (vlc_externals (priv));
    }
    libvlc_unlock (obj->p_libvlc);
    return l;
}

/*****************************************************************************
 * DumpCommand: print the current vlc structure
 *****************************************************************************
 * This function prints either an ASCII tree showing the connections between
 * vlc objects, and additional information such as their refcount, thread ID,
 * etc. (command "tree"), or the same data as a simple list (command "list").
 *****************************************************************************/
static int DumpCommand( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)oldval;
    vlc_object_t *p_object = NULL;

    if( *newval.psz_string )
    {
        /* try using the object's name to find it */
        p_object = vlc_object_find_name( p_data, newval.psz_string );
        if( !p_object )
            return VLC_ENOOBJ;
    }

    libvlc_lock (p_this->p_libvlc);
    if( *psz_cmd == 't' )
    {
        char psz_foo[2 * MAX_DUMPSTRUCTURE_DEPTH + 1];

        if( !p_object )
            p_object = VLC_OBJECT(p_this->p_libvlc);

        psz_foo[0] = '|';
        DumpStructure( vlc_internals(p_object), 0, psz_foo );
    }
    else if( *psz_cmd == 'v' )
    {
        if( !p_object )
            p_object = p_this->p_libvlc ? VLC_OBJECT(p_this->p_libvlc) : p_this;

        PrintObject( vlc_internals(p_object), "" );
        DumpVariables(p_object);
    }
    libvlc_unlock (p_this->p_libvlc);

    if( *newval.psz_string )
    {
        vlc_object_release( p_object );
    }
    return VLC_SUCCESS;
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

/* Following functions are local */

static vlc_object_t *FindName (vlc_object_internals_t *priv, const char *name)
{
    if (priv->psz_name != NULL && !strcmp (priv->psz_name, name))
        return vlc_object_hold (vlc_externals (priv));

    for (priv = priv->first; priv != NULL; priv = priv->next)
    {
        vlc_object_t *found = FindName (priv, name);
        if (found != NULL)
            return found;
    }
    return NULL;
}

static void PrintObject( vlc_object_internals_t *priv,
                         const char *psz_prefix )
{
    char psz_refcount[20], psz_name[50], psz_parent[20];

    int canc = vlc_savecancel ();
    memset( &psz_name, 0, sizeof(psz_name) );

    vlc_mutex_lock (&name_lock);
    if (priv->psz_name != NULL)
    {
        snprintf( psz_name, 49, " \"%s\"", priv->psz_name );
        if( psz_name[48] )
            psz_name[48] = '\"';
    }
    vlc_mutex_unlock (&name_lock);

    snprintf( psz_refcount, 19, ", %u refs", atomic_load( &priv->refs ) );

    psz_parent[0] = '\0';
    /* FIXME: need structure lock!!! */
    if( vlc_externals(priv)->p_parent )
        snprintf( psz_parent, 19, ", parent %p",
                  vlc_externals(priv)->p_parent );

    printf( " %so %p %s%s%s%s\n", psz_prefix,
            vlc_externals(priv), vlc_externals(priv)->psz_object_type,
            psz_name, psz_refcount, psz_parent );
    vlc_restorecancel (canc);
}

static void DumpStructure (vlc_object_internals_t *priv, unsigned i_level,
                           char *psz_foo)
{
    char i_back = psz_foo[i_level];
    psz_foo[i_level] = '\0';

    PrintObject (priv, psz_foo);

    psz_foo[i_level] = i_back;

    if( i_level / 2 >= MAX_DUMPSTRUCTURE_DEPTH )
    {
        msg_Warn( vlc_externals(priv), "structure tree is too deep" );
        return;
    }

    for (priv = priv->first; priv != NULL; priv = priv->next)
    {
        if( i_level )
        {
            psz_foo[i_level-1] = ' ';

            if( psz_foo[i_level-2] == '`' )
            {
                psz_foo[i_level-2] = ' ';
            }
        }

        psz_foo[i_level] = priv->next ? '|' : '`';
        psz_foo[i_level+1] = '-';
        psz_foo[i_level+2] = '\0';

        DumpStructure (priv, i_level + 2, psz_foo);
    }
}

static vlc_list_t * NewList( int i_count )
{
    vlc_list_t * p_list = malloc( sizeof( vlc_list_t ) );
    if( p_list == NULL )
        return NULL;

    p_list->i_count = i_count;

    if( i_count == 0 )
    {
        p_list->p_values = NULL;
        return p_list;
    }

    p_list->p_values = malloc( i_count * sizeof( vlc_value_t ) );
    if( p_list->p_values == NULL )
    {
        p_list->i_count = 0;
        return p_list;
    }

    return p_list;
}
