/*****************************************************************************
 * objects.c: vlc_object_t handling
 *****************************************************************************
 * Copyright (C) 2004-2008 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#ifndef WIN32
# include <unistd.h>
#else
# include <io.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# undef  read
# define read( a, b, c )  recv (a, b, c, 0)
# undef  write
# define write( a, b, c ) send (a, b, c, 0)
# undef  close
# define close( a )       closesocket (a)
#endif

#include <search.h>
#include <limits.h>
#include <assert.h>

#if defined (HAVE_SYS_EVENTFD_H)
# include <sys/eventfd.h>
#endif


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DumpCommand( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

static vlc_object_t * FindParent    ( vlc_object_t *, int );
static vlc_object_t * FindChild     ( vlc_object_t *, int );
static vlc_object_t * FindParentName( vlc_object_t *, const char * );
static vlc_object_t * FindChildName ( vlc_object_t *, const char * );
static void           PrintObject   ( vlc_object_t *, const char * );
static void           DumpStructure ( vlc_object_t *, int, char * );

static vlc_list_t   * NewList       ( int );
static void           ListReplace   ( vlc_list_t *, vlc_object_t *, int );
/*static void           ListAppend    ( vlc_list_t *, vlc_object_t * );*/
static int            CountChildren ( vlc_object_t *, int );
static void           ListChildren  ( vlc_list_t *, vlc_object_t *, int );

static void vlc_object_destroy( vlc_object_t *p_this );
static void vlc_object_detach_unlocked (vlc_object_t *p_this);

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

void *__vlc_custom_create( vlc_object_t *p_this, size_t i_size,
                           int i_type, const char *psz_type )
{
    vlc_object_t *p_new;
    vlc_object_internals_t *p_priv;

    /* NOTE:
     * VLC objects are laid out as follow:
     * - first the LibVLC-private per-object data,
     * - then VLC_COMMON members from vlc_object_t,
     * - finally, the type-specific data (if any).
     *
     * This function initializes the LibVLC and common data,
     * and zeroes the rest.
     */
    p_priv = calloc( 1, sizeof( *p_priv ) + i_size );
    if( p_priv == NULL )
        return NULL;

    assert (i_size >= sizeof (vlc_object_t));
    p_new = (vlc_object_t *)(p_priv + 1);

    p_priv->i_object_type = i_type;
    p_new->psz_object_type = psz_type;
    p_priv->psz_name = NULL;

    p_new->b_die = false;
    p_new->b_error = false;
    p_new->b_force = false;

    p_new->psz_header = NULL;

    if (p_this)
        p_new->i_flags = p_this->i_flags
            & (OBJECT_FLAGS_NODBG|OBJECT_FLAGS_QUIET|OBJECT_FLAGS_NOINTERACT);

    p_priv->var_root = NULL;

    if( p_this == NULL )
    {
        libvlc_int_t *self = (libvlc_int_t*)p_new;
        p_new->p_libvlc = self;
        vlc_mutex_init (&(libvlc_priv (self)->structure_lock));
        p_this = p_new;
    }
    else
        p_new->p_libvlc = p_this->p_libvlc;

    vlc_spin_init( &p_priv->ref_spin );
    p_priv->i_refcount = 1;
    p_priv->pf_destructor = NULL;
    p_priv->b_thread = false;
    p_new->p_parent = NULL;
    p_priv->pp_children = NULL;
    p_priv->i_children = 0;

    /* Initialize mutexes and condvars */
    vlc_mutex_init( &p_priv->var_lock );
    vlc_cond_init( &p_priv->var_wait );
    p_priv->pipes[0] = p_priv->pipes[1] = -1;

    if (p_new == VLC_OBJECT(p_new->p_libvlc))
    {   /* TODO: should be in src/libvlc.c */
        int canc = vlc_savecancel ();
        var_Create( p_new, "tree", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "tree", DumpCommand, NULL );
        var_Create( p_new, "vars", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "vars", DumpCommand, NULL );
        vlc_restorecancel (canc);
    }

    return p_new;
}


/**
 * Allocates and initializes a vlc object.
 *
 * @param i_type known object type (all of them are negative integer values),
 *               or object byte size (always positive).
 *
 * @return the new object, or NULL on error.
 */
void * __vlc_object_create( vlc_object_t *p_this, int i_type )
{
    const char   * psz_type;
    size_t         i_size;

    switch( i_type )
    {
        case VLC_OBJECT_DECODER:
            i_size = sizeof(decoder_t);
            psz_type = "decoder";
            break;
        case VLC_OBJECT_AOUT:
            i_size = sizeof(aout_instance_t);
            psz_type = "audio output";
            break;
        default:
            assert( i_type > 0 ); /* unknown type?! */
            i_size = i_type;
            i_type = VLC_OBJECT_GENERIC;
            psz_type = "generic";
            break;
    }

    return vlc_custom_create( p_this, i_size, i_type, psz_type );
}


/**
 ****************************************************************************
 * Set the destructor of a vlc object
 *
 * This function sets the destructor of the vlc object. It will be called
 * when the object is destroyed when the its refcount reaches 0.
 * (It is called by the internal function vlc_object_destroy())
 *****************************************************************************/
void __vlc_object_set_destructor( vlc_object_t *p_this,
                                  vlc_destructor_t pf_destructor )
{
    vlc_object_internals_t *p_priv = vlc_internals(p_this );

    vlc_spin_lock( &p_priv->ref_spin );
    p_priv->pf_destructor = pf_destructor;
    vlc_spin_unlock( &p_priv->ref_spin );
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
 ****************************************************************************
 * Destroy a vlc object (Internal)
 *
 * This function destroys an object that has been previously allocated with
 * vlc_object_create. The object's refcount must be zero and it must not be
 * attached to other objects in any way.
 *
 * This function must be called with cancellation disabled (currently).
 *****************************************************************************/
static void vlc_object_destroy( vlc_object_t *p_this )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    /* Objects are always detached beforehand */
    assert( !p_this->p_parent );

    /* Send a kill to the object's thread if applicable */
    vlc_object_kill( p_this );

    /* Call the custom "subclass" destructor */
    if( p_priv->pf_destructor )
        p_priv->pf_destructor( p_this );

    /* Any thread must have been cleaned up at this point. */
    assert( !p_priv->b_thread );

    /* Destroy the associated variables. */
    var_DestroyAll( p_this );

    vlc_cond_destroy( &p_priv->var_wait );
    vlc_mutex_destroy( &p_priv->var_lock );

    free( p_this->psz_header );

    free( p_priv->psz_name );

    vlc_spin_destroy( &p_priv->ref_spin );
    if( p_priv->pipes[1] != -1 && p_priv->pipes[1] != p_priv->pipes[0] )
        close( p_priv->pipes[1] );
    if( p_priv->pipes[0] != -1 )
        close( p_priv->pipes[0] );
    if( VLC_OBJECT(p_this->p_libvlc) == p_this )
        vlc_mutex_destroy (&(libvlc_priv ((libvlc_int_t *)p_this)->structure_lock));

    free( p_priv );
}


#ifdef WIN32
/**
 * select()-able pipes emulated using Winsock
 */
static int pipe (int fd[2])
{
    SOCKADDR_IN addr;
    int addrlen = sizeof (addr);

    SOCKET l = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP), a,
           c = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ((l == INVALID_SOCKET) || (c == INVALID_SOCKET))
        goto error;

    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    if (bind (l, (PSOCKADDR)&addr, sizeof (addr))
     || getsockname (l, (PSOCKADDR)&addr, &addrlen)
     || listen (l, 1)
     || connect (c, (PSOCKADDR)&addr, addrlen))
        goto error;

    a = accept (l, NULL, NULL);
    if (a == INVALID_SOCKET)
        goto error;

    closesocket (l);
    //shutdown (a, 0);
    //shutdown (c, 1);
    fd[0] = c;
    fd[1] = a;
    return 0;

error:
    if (l != INVALID_SOCKET)
        closesocket (l);
    if (c != INVALID_SOCKET)
        closesocket (c);
    return -1;
}
#endif /* WIN32 */

static vlc_mutex_t pipe_lock = VLC_STATIC_MUTEX;

/**
 * Returns the readable end of a pipe that becomes readable once termination
 * of the object is requested (vlc_object_kill()).
 * This can be used to wake-up out of a select() or poll() event loop, such
 * typically when doing network I/O.
 *
 * Note that the pipe will remain the same for the lifetime of the object.
 * DO NOT read the pipe nor close it yourself. Ever.
 *
 * @param obj object that would be "killed"
 * @return a readable pipe descriptor, or -1 on error.
 */
int vlc_object_waitpipe( vlc_object_t *obj )
{
    vlc_object_internals_t *internals = vlc_internals( obj );

    vlc_mutex_lock (&pipe_lock);
    if (internals->pipes[0] == -1)
    {
        /* This can only ever happen if someone killed us without locking: */
        assert (internals->pipes[1] == -1);

#if defined (HAVE_SYS_EVENTFD_H)
        internals->pipes[0] = internals->pipes[1] = eventfd (0, 0);
        if (internals->pipes[0] == -1)
#endif
        {
            if (pipe (internals->pipes))
                internals->pipes[0] = internals->pipes[1] = -1;
        }

        if (internals->pipes[0] != -1 && obj->b_die)
        {   /* Race condition: vlc_object_kill() already invoked! */
            msg_Dbg (obj, "waitpipe: object already dying");
            write (internals->pipes[1], &(uint64_t){ 1 }, sizeof (uint64_t));
        }
    }
    vlc_mutex_unlock (&pipe_lock);
    return internals->pipes[0];
}


/**
 * Requests termination of an object, cancels the object thread, and make the
 * object wait pipe (if it exists) readable. Not a cancellation point.
 */
void __vlc_object_kill( vlc_object_t *p_this )
{
    vlc_object_internals_t *priv = vlc_internals( p_this );
    int fd = -1;

    vlc_thread_cancel( p_this );
    vlc_mutex_lock( &pipe_lock );
    if( !p_this->b_die )
    {
        fd = priv->pipes[1];
        p_this->b_die = true;
    }

    /* This also serves as a memory barrier toward vlc_object_alive(): */
    vlc_mutex_unlock( &pipe_lock );

    if (fd != -1)
    {
        int canc = vlc_savecancel ();

        /* write _after_ setting b_die, so vlc_object_alive() returns false */
        write (fd, &(uint64_t){ 1 }, sizeof (uint64_t));
        msg_Dbg (p_this, "waitpipe: object killed");
        vlc_restorecancel (canc);
    }
}


/*****************************************************************************
 * find a typed object and increment its refcount
 *****************************************************************************
 * This function recursively looks for a given object type. i_mode can be one
 * of FIND_PARENT, FIND_CHILD or FIND_ANYWHERE.
 *****************************************************************************/
void * __vlc_object_find( vlc_object_t *p_this, int i_type, int i_mode )
{
    vlc_object_t *p_found;

    /* If we are of the requested type ourselves, don't look further */
    if( vlc_internals (p_this)->i_object_type == i_type )
    {
        vlc_object_hold( p_this );
        return p_this;
    }

    /* Otherwise, recursively look for the object */
    if (i_mode == FIND_ANYWHERE)
        return vlc_object_find (p_this->p_libvlc, i_type, FIND_CHILD);

    switch (i_type)
    {
        case VLC_OBJECT_VOUT:
        case VLC_OBJECT_AOUT:
            break;
        case VLC_OBJECT_INPUT:
            /* input can only be accessed like this from children,
             * otherwise we could not promise that it is initialized */
            if (i_mode != FIND_PARENT)
                return NULL;
            break;
        default:
            return NULL;
    }

    libvlc_lock (p_this->p_libvlc);
    switch (i_mode)
    {
        case FIND_PARENT:
            p_found = FindParent (p_this, i_type);
            break;
        case FIND_CHILD:
            p_found = FindChild (p_this, i_type);
            break;
        default:
            assert (0);
    }
    libvlc_unlock (p_this->p_libvlc);
    return p_found;
}


static int objnamecmp(const vlc_object_t *obj, const char *name)
{
    char *objname = vlc_object_get_name(obj);
    if (objname == NULL)
        return INT_MIN;

    int ret = strcmp (objname, name);
    free (objname);
    return ret;
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
 * @param i_mode search direction: FIND_PARENT, FIND_CHILD or FIND_ANYWHERE.
 *
 * @return a matching object (must be released by the caller),
 * or NULL on error.
 */
vlc_object_t *vlc_object_find_name( vlc_object_t *p_this,
                                    const char *psz_name, int i_mode )
{
    vlc_object_t *p_found;

    /* Reading psz_object_name from a separate inhibits thread-safety.
     * Use a libvlc address variable instead for that sort of things! */
    msg_Warn( p_this, "%s(%s) is not safe!", __func__, psz_name );
    /* If have the requested name ourselves, don't look further */
    if( !objnamecmp(p_this, psz_name) )
    {
        vlc_object_hold( p_this );
        return p_this;
    }

    /* Otherwise, recursively look for the object */
    if (i_mode == FIND_ANYWHERE)
        return vlc_object_find_name (VLC_OBJECT(p_this->p_libvlc), psz_name,
                                     FIND_CHILD);

    libvlc_lock (p_this->p_libvlc);
    switch (i_mode)
    {
        case FIND_PARENT:
            p_found = FindParentName (p_this, psz_name);
            break;
        case FIND_CHILD:
            p_found = FindChildName (p_this, psz_name);
            break;
        default:
            assert (0);
    }
    libvlc_unlock (p_this->p_libvlc);
    return p_found;
}

/**
 * Increment an object reference counter.
 */
void * __vlc_object_hold( vlc_object_t *p_this )
{
    vlc_object_internals_t *internals = vlc_internals( p_this );

    vlc_spin_lock( &internals->ref_spin );
    /* Avoid obvious freed object uses */
    assert( internals->i_refcount > 0 );
    /* Increment the counter */
    internals->i_refcount++;
    vlc_spin_unlock( &internals->ref_spin );
    return p_this;
}

/*****************************************************************************
 * Decrement an object refcount
 * And destroy the object if its refcount reach zero.
 *****************************************************************************/
void __vlc_object_release( vlc_object_t *p_this )
{
    vlc_object_internals_t *internals = vlc_internals( p_this );
    vlc_object_t *parent = NULL;
    bool b_should_destroy;

    vlc_spin_lock( &internals->ref_spin );
    assert( internals->i_refcount > 0 );

    if( internals->i_refcount > 1 )
    {
        /* Fast path */
        /* There are still other references to the object */
        internals->i_refcount--;
        vlc_spin_unlock( &internals->ref_spin );
        return;
    }
    vlc_spin_unlock( &internals->ref_spin );

    /* Slow path */
    /* Remember that we cannot hold the spin while waiting on the mutex */
    libvlc_lock (p_this->p_libvlc);
    /* Take the spin again. Note that another thread may have held the
     * object in the (very short) mean time. */
    vlc_spin_lock( &internals->ref_spin );
    b_should_destroy = --internals->i_refcount == 0;
    vlc_spin_unlock( &internals->ref_spin );

    if( b_should_destroy )
    {
        parent = p_this->p_parent;
        if (parent)
            /* Detach from parent to protect against FIND_CHILDREN */
            vlc_object_detach_unlocked (p_this);

        /* We have no children */
        assert (internals->i_children == 0);
    }
    libvlc_unlock (p_this->p_libvlc);

    if( b_should_destroy )
    {
        int canc;

        canc = vlc_savecancel ();
        vlc_object_destroy( p_this );
        vlc_restorecancel (canc);
        if (parent)
            vlc_object_release (parent);
    }
}

/**
 ****************************************************************************
 * attach object to a parent object
 *****************************************************************************
 * This function sets p_this as a child of p_parent, and p_parent as a parent
 * of p_this. This link can be undone using vlc_object_detach.
 *****************************************************************************/
void __vlc_object_attach( vlc_object_t *p_this, vlc_object_t *p_parent )
{
    if( !p_this ) return;

    vlc_object_hold (p_parent);
    libvlc_lock (p_this->p_libvlc);

    /* Attach the parent to its child */
    assert (!p_this->p_parent);
    p_this->p_parent = p_parent;

    /* Attach the child to its parent */
    vlc_object_internals_t *priv = vlc_internals( p_parent );
    INSERT_ELEM( priv->pp_children, priv->i_children, priv->i_children,
                 p_this );
    libvlc_unlock (p_this->p_libvlc);
}


static void vlc_object_detach_unlocked (vlc_object_t *p_this)
{
    if (p_this->p_parent == NULL)
        return;

    vlc_object_internals_t *priv = vlc_internals( p_this->p_parent );

    int i_index, i;

    /* Remove p_this's parent */
    p_this->p_parent = NULL;

    /* Remove all of p_parent's children which are p_this */
    for( i_index = priv->i_children ; i_index-- ; )
    {
        if( priv->pp_children[i_index] == p_this )
        {
            priv->i_children--;
            for( i = i_index ; i < priv->i_children ; i++ )
                priv->pp_children[i] = priv->pp_children[i+1];
        }
    }

    if( priv->i_children )
    {
        vlc_object_t **pp_children = (vlc_object_t **)
            realloc( priv->pp_children,
                     priv->i_children * sizeof(vlc_object_t *) );
        if( pp_children )
            priv->pp_children = pp_children;
    }
    else
    {
        /* Special case - don't realloc() to zero to avoid leaking */
        free( priv->pp_children );
        priv->pp_children = NULL;
    }
}


/**
 ****************************************************************************
 * detach object from its parent
 *****************************************************************************
 * This function removes all links between an object and its parent.
 *****************************************************************************/
void __vlc_object_detach( vlc_object_t *p_this )
{
    vlc_object_t *p_parent;
    if( !p_this ) return;

    libvlc_lock (p_this->p_libvlc);
    p_parent = p_this->p_parent;
    if (p_parent)
        vlc_object_detach_unlocked( p_this );
    libvlc_unlock (p_this->p_libvlc);

    if (p_parent)
        vlc_object_release (p_parent);
}


/**
 ****************************************************************************
 * find a list typed objects and increment their refcount
 *****************************************************************************
 * This function recursively looks for a given object type. i_mode can be one
 * of FIND_PARENT, FIND_CHILD or FIND_ANYWHERE.
 *****************************************************************************/
vlc_list_t * vlc_list_find( vlc_object_t *p_this, int i_type, int i_mode )
{
    vlc_list_t *p_list;
    int i_count = 0;

    /* Look for the objects */
    switch( i_mode )
    {
    case FIND_ANYWHERE:
        return vlc_list_find (VLC_OBJECT(p_this->p_libvlc), i_type, FIND_CHILD);

    case FIND_CHILD:
        libvlc_lock (p_this->p_libvlc);
        i_count = CountChildren( p_this, i_type );
        p_list = NewList( i_count );

        /* Check allocation was successful */
        if( p_list->i_count != i_count )
        {
            libvlc_unlock (p_this->p_libvlc);
            p_list->i_count = 0;
            break;
        }

        p_list->i_count = 0;
        ListChildren( p_list, p_this, i_type );
        libvlc_unlock (p_this->p_libvlc);
        break;

    default:
        msg_Err( p_this, "unimplemented!" );
        p_list = NewList( 0 );
        break;
    }

    return p_list;
}

/**
 * Gets the list of children of an objects, and increment their reference
 * count.
 * @return a list (possibly empty) or NULL in case of error.
 */
vlc_list_t *__vlc_list_children( vlc_object_t *obj )
{
    vlc_list_t *l;
    vlc_object_internals_t *priv = vlc_internals( obj );

    libvlc_lock (obj->p_libvlc);
    l = NewList( priv->i_children );
    for (int i = 0; i < l->i_count; i++)
    {
        vlc_object_hold( priv->pp_children[i] );
        l->p_values[i].p_object = priv->pp_children[i];
    }
    libvlc_unlock (obj->p_libvlc);
    return l;
}

static void DumpVariable (const void *data, const VISIT which, const int depth)
{
    if (which != postorder && which != leaf)
        return;
    (void) depth;

    const variable_t *p_var = *(const variable_t **)data;
    const char *psz_type = "unknown";

    switch( p_var->i_type & VLC_VAR_TYPE )
    {
#define MYCASE( type, nice )    \
        case VLC_VAR_ ## type:  \
            psz_type = nice;    \
            break;
        MYCASE( VOID, "void" );
        MYCASE( BOOL, "bool" );
        MYCASE( INTEGER, "integer" );
        MYCASE( HOTKEY, "hotkey" );
        MYCASE( STRING, "string" );
        MYCASE( MODULE, "module" );
        MYCASE( FILE, "file" );
        MYCASE( DIRECTORY, "directory" );
        MYCASE( VARIABLE, "variable" );
        MYCASE( FLOAT, "float" );
        MYCASE( TIME, "time" );
        MYCASE( ADDRESS, "address" );
        MYCASE( MUTEX, "mutex" );
        MYCASE( LIST, "list" );
#undef MYCASE
    }
    printf( " *-o \"%s\" (%s", p_var->psz_name, psz_type );
    if( p_var->psz_text )
        printf( ", %s", p_var->psz_text );
    fputc( ')', stdout );
    if( p_var->i_type & VLC_VAR_HASCHOICE )
        fputs( ", has choices", stdout );
    if( p_var->i_type & VLC_VAR_ISCOMMAND )
        fputs( ", command", stdout );
    if( p_var->i_entries )
        printf( ", %d callbacks", p_var->i_entries );
    switch( p_var->i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_VOID:
        case VLC_VAR_MUTEX:
            break;
        case VLC_VAR_BOOL:
            printf( ": %s", p_var->val.b_bool ? "true" : "false" );
            break;
        case VLC_VAR_INTEGER:
            printf( ": %d", p_var->val.i_int );
            break;
        case VLC_VAR_STRING:
            printf( ": \"%s\"", p_var->val.psz_string );
            break;
        case VLC_VAR_FLOAT:
            printf( ": %f", p_var->val.f_float );
            break;
        case VLC_VAR_TIME:
            printf( ": %"PRIi64, (int64_t)p_var->val.i_time );
            break;
        case VLC_VAR_ADDRESS:
            printf( ": %p", p_var->val.p_address );
            break;
        case VLC_VAR_LIST:
            fputs( ": TODO", stdout );
            break;
    }
    fputc( '\n', stdout );
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
    (void)oldval; (void)p_data;
    vlc_object_t *p_object = NULL;

    if( *newval.psz_string )
    {
        /* try using the object's name to find it */
        p_object = vlc_object_find_name( p_this, newval.psz_string,
                                         FIND_ANYWHERE );
        if( !p_object )
        {
            return VLC_ENOOBJ;
        }
    }

    libvlc_lock (p_this->p_libvlc);
    if( *psz_cmd == 't' )
    {
        char psz_foo[2 * MAX_DUMPSTRUCTURE_DEPTH + 1];

        if( !p_object )
            p_object = VLC_OBJECT(p_this->p_libvlc);

        psz_foo[0] = '|';
        DumpStructure( p_object, 0, psz_foo );
    }
    else if( *psz_cmd == 'v' )
    {
        if( !p_object )
            p_object = p_this->p_libvlc ? VLC_OBJECT(p_this->p_libvlc) : p_this;

        PrintObject( p_object, "" );
        vlc_mutex_lock( &vlc_internals( p_object )->var_lock );
        if( vlc_internals( p_object )->var_root == NULL )
            puts( " `-o No variables" );
        else
            twalk( vlc_internals( p_object )->var_root, DumpVariable );
        vlc_mutex_unlock( &vlc_internals( p_object )->var_lock );
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
    int i_index;

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        vlc_object_release( p_list->p_values[i_index].p_object );
    }

    free( p_list->p_values );
    free( p_list );
}

/* Following functions are local */

static vlc_object_t *FindParent (vlc_object_t *p_this, int i_type)
{
    for (vlc_object_t *parent = p_this->p_parent;
         parent != NULL;
         parent = parent->p_parent)
    {
        if (vlc_internals (parent)->i_object_type == i_type)
            return vlc_object_hold (parent);
    }
    return NULL;
}

static vlc_object_t *FindParentName (vlc_object_t *p_this, const char *name)
{
    for (vlc_object_t *parent = p_this->p_parent;
         parent != NULL;
         parent = parent->p_parent)
    {
        if (!objnamecmp (parent, name))
            return vlc_object_hold (parent);
    }
    return NULL;
}

static vlc_object_t *FindChild (vlc_object_t *p_this, int i_type)
{
    for (int i = vlc_internals( p_this )->i_children; i--; )
    {
        vlc_object_t *child = vlc_internals (p_this)->pp_children[i];
        if (vlc_internals (child)->i_object_type == i_type)
            return vlc_object_hold (child);

        child = FindChild (child, i_type);
        if (child != NULL)
            return child;
    }
    return NULL;
}

static vlc_object_t *FindChildName (vlc_object_t *p_this, const char *name)
{
    for (int i = vlc_internals( p_this )->i_children; i--; )
    {
        vlc_object_t *child = vlc_internals (p_this)->pp_children[i];
        if (!objnamecmp (child, name))
            return vlc_object_hold (child);

        child = FindChildName (child, name);
        if (child != NULL)
            return child;
    }
    return NULL;
}

static void PrintObject( vlc_object_t *p_this, const char *psz_prefix )
{
    char psz_children[20], psz_refcount[20], psz_thread[30], psz_name[50],
         psz_parent[20];

    int canc = vlc_savecancel ();
    memset( &psz_name, 0, sizeof(psz_name) );
    char *name = vlc_object_get_name(p_this);
    if( name )
    {
        snprintf( psz_name, 49, " \"%s\"", name );
        free( name );
        if( psz_name[48] )
            psz_name[48] = '\"';
    }

    psz_children[0] = '\0';
    switch( vlc_internals( p_this )->i_children )
    {
        case 0:
            break;
        case 1:
            strcpy( psz_children, ", 1 child" );
            break;
        default:
            snprintf( psz_children, 19, ", %i children",
                      vlc_internals( p_this )->i_children );
            break;
    }

    psz_refcount[0] = '\0';
    if( vlc_internals( p_this )->i_refcount > 0 )
        snprintf( psz_refcount, 19, ", refcount %u",
                  vlc_internals( p_this )->i_refcount );

    psz_thread[0] = '\0';
    if( vlc_internals( p_this )->b_thread )
        snprintf( psz_thread, 29, " (thread %lu)",
                  (unsigned long)vlc_internals( p_this )->thread_id );

    psz_parent[0] = '\0';
    if( p_this->p_parent )
        snprintf( psz_parent, 19, ", parent %p", p_this->p_parent );

    printf( " %so %p %s%s%s%s%s%s\n", psz_prefix,
            p_this, p_this->psz_object_type,
            psz_name, psz_thread, psz_refcount, psz_children,
            psz_parent );
    vlc_restorecancel (canc);
}

static void DumpStructure( vlc_object_t *p_this, int i_level, char *psz_foo )
{
    int i;
    char i_back = psz_foo[i_level];
    psz_foo[i_level] = '\0';

    PrintObject( p_this, psz_foo );

    psz_foo[i_level] = i_back;

    if( i_level / 2 >= MAX_DUMPSTRUCTURE_DEPTH )
    {
        msg_Warn( p_this, "structure tree is too deep" );
        return;
    }

    for( i = 0 ; i < vlc_internals( p_this )->i_children ; i++ )
    {
        if( i_level )
        {
            psz_foo[i_level-1] = ' ';

            if( psz_foo[i_level-2] == '`' )
            {
                psz_foo[i_level-2] = ' ';
            }
        }

        if( i == vlc_internals( p_this )->i_children - 1 )
        {
            psz_foo[i_level] = '`';
        }
        else
        {
            psz_foo[i_level] = '|';
        }

        psz_foo[i_level+1] = '-';
        psz_foo[i_level+2] = '\0';

        DumpStructure( vlc_internals( p_this )->pp_children[i], i_level + 2,
                       psz_foo );
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

static void ListReplace( vlc_list_t *p_list, vlc_object_t *p_object,
                         int i_index )
{
    if( p_list == NULL || i_index >= p_list->i_count )
    {
        return;
    }

    vlc_object_hold( p_object );

    p_list->p_values[i_index].p_object = p_object;

    return;
}

/*static void ListAppend( vlc_list_t *p_list, vlc_object_t *p_object )
{
    if( p_list == NULL )
    {
        return;
    }

    p_list->p_values = realloc_or_free( p_list->p_values,
                              (p_list->i_count + 1) * sizeof( vlc_value_t ) );
    if( p_list->p_values == NULL )
    {
        p_list->i_count = 0;
        return;
    }

    vlc_object_hold( p_object );

    p_list->p_values[p_list->i_count].p_object = p_object;
    p_list->i_count++;

    return;
}*/

static int CountChildren( vlc_object_t *p_this, int i_type )
{
    vlc_object_t *p_tmp;
    int i, i_count = 0;

    for( i = 0; i < vlc_internals( p_this )->i_children; i++ )
    {
        p_tmp = vlc_internals( p_this )->pp_children[i];

        if( vlc_internals( p_tmp )->i_object_type == i_type )
        {
            i_count++;
        }
        i_count += CountChildren( p_tmp, i_type );
    }

    return i_count;
}

static void ListChildren( vlc_list_t *p_list, vlc_object_t *p_this, int i_type )
{
    vlc_object_t *p_tmp;
    int i;

    for( i = 0; i < vlc_internals( p_this )->i_children; i++ )
    {
        p_tmp = vlc_internals( p_this )->pp_children[i];

        if( vlc_internals( p_tmp )->i_object_type == i_type )
            ListReplace( p_list, p_tmp, p_list->i_count++ );

        ListChildren( p_list, p_tmp, i_type );
    }
}
