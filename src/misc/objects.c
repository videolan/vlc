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
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "../libvlc.h"
#include <vlc_vout.h>
#include <vlc_aout.h>
#include "audio_output/aout_internal.h"

#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_stream.h>

#include <vlc_sout.h>
#include "stream_output/stream_output.h"

#include "vlc_interface.h"
#include "vlc_codec.h"
#include "vlc_filter.h"

#include "variables.h"
#ifndef WIN32
# include <unistd.h>
#else
# include <io.h>
# include <fcntl.h>
# include <errno.h> /* ENOSYS */
#endif
#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DumpCommand( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

static vlc_object_t * FindObject    ( vlc_object_t *, int, int );
static vlc_object_t * FindObjectName( vlc_object_t *, const char *, int );
static void           PrintObject   ( vlc_object_t *, const char * );
static void           DumpStructure ( vlc_object_t *, int, char * );

static vlc_list_t   * NewList       ( int );
static void           ListReplace   ( vlc_list_t *, vlc_object_t *, int );
/*static void           ListAppend    ( vlc_list_t *, vlc_object_t * );*/
static int            CountChildren ( vlc_object_t *, int );
static void           ListChildren  ( vlc_list_t *, vlc_object_t *, int );

static void vlc_object_destroy( vlc_object_t *p_this );
static void vlc_object_detach_unlocked (vlc_object_t *p_this);

#ifdef LIBVLC_REFCHECK
static vlc_threadvar_t held_objects;
typedef struct held_list_t
{
    struct held_list_t *next;
    vlc_object_t *obj;
} held_list_t;
static void held_objects_destroy (void *);
#endif

/*****************************************************************************
 * Local structure lock
 *****************************************************************************/
static vlc_mutex_t structure_lock;
static unsigned    object_counter = 0;

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

    p_new->i_object_type = i_type;
    p_new->psz_object_type = psz_type;
    p_new->psz_object_name = NULL;

    p_new->b_die = false;
    p_new->b_error = false;
    p_new->b_dead = false;
    p_new->b_force = false;

    p_new->psz_header = NULL;

    if (p_this)
        p_new->i_flags = p_this->i_flags
            & (OBJECT_FLAGS_NODBG|OBJECT_FLAGS_QUIET|OBJECT_FLAGS_NOINTERACT);

    p_priv->p_vars = calloc( sizeof( variable_t ), 16 );

    if( !p_priv->p_vars )
    {
        free( p_priv );
        return NULL;
    }

    libvlc_global_data_t *p_libvlc_global;
    if( p_this == NULL )
    {
        /* Only the global root object is created out of the blue */
        p_libvlc_global = (libvlc_global_data_t *)p_new;
        p_new->p_libvlc = NULL;

        object_counter = 0; /* reset */
        p_priv->next = p_priv->prev = p_new;
        vlc_mutex_init( &structure_lock );
#ifdef LIBVLC_REFCHECK
        /* TODO: use the destruction callback to track ref leaks */
        vlc_threadvar_create( &held_objects, held_objects_destroy );
#endif
    }
    else
    {
        p_libvlc_global = vlc_global();
        if( i_type == VLC_OBJECT_LIBVLC )
            p_new->p_libvlc = (libvlc_int_t*)p_new;
        else
            p_new->p_libvlc = p_this->p_libvlc;
    }

    vlc_spin_init( &p_priv->ref_spin );
    p_priv->i_refcount = 1;
    p_priv->pf_destructor = NULL;
    p_priv->b_thread = false;
    p_new->p_parent = NULL;
    p_priv->pp_children = NULL;
    p_priv->i_children = 0;

    p_new->p_private = NULL;

    /* Initialize mutexes and condvars */
    vlc_mutex_init( &p_priv->lock );
    vlc_cond_init( p_new, &p_priv->wait );
    vlc_mutex_init( &p_priv->var_lock );
    vlc_spin_init( &p_priv->spin );
    p_priv->pipes[0] = p_priv->pipes[1] = -1;

    p_priv->next = VLC_OBJECT (p_libvlc_global);
#if !defined (LIBVLC_REFCHECK)
    /* ... */
#elif defined (LIBVLC_USE_PTHREAD)
    p_priv->creator_id = pthread_self ();
#elif defined (WIN32)
    p_priv->creator_id = GetCurrentThreadId ();
#endif
    vlc_mutex_lock( &structure_lock );
    p_priv->prev = vlc_internals (p_libvlc_global)->prev;
    vlc_internals (p_libvlc_global)->prev = p_new;
    vlc_internals (p_priv->prev)->next = p_new;
    p_new->i_object_id = object_counter++; /* fetch THEN increment */
    vlc_mutex_unlock( &structure_lock );

    if( i_type == VLC_OBJECT_LIBVLC )
    {
        var_Create( p_new, "list", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "list", DumpCommand, NULL );
        var_Create( p_new, "tree", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "tree", DumpCommand, NULL );
        var_Create( p_new, "vars", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "vars", DumpCommand, NULL );
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
        case VLC_OBJECT_INTF:
            i_size = sizeof(intf_thread_t);
            psz_type = "interface";
            break;
        case VLC_OBJECT_DECODER:
            i_size = sizeof(decoder_t);
            psz_type = "decoder";
            break;
        case VLC_OBJECT_PACKETIZER:
            i_size = sizeof(decoder_t);
            psz_type = "packetizer";
            break;
        case VLC_OBJECT_ENCODER:
            i_size = sizeof(encoder_t);
            psz_type = "encoder";
            break;
        case VLC_OBJECT_AOUT:
            i_size = sizeof(aout_instance_t);
            psz_type = "audio output";
            break;
        case VLC_OBJECT_OPENGL:
            i_size = sizeof( vout_thread_t );
            psz_type = "opengl";
            break;
        case VLC_OBJECT_ANNOUNCE:
            i_size = sizeof( announce_handler_t );
            psz_type = "announce";
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
    p_priv->pf_destructor = pf_destructor;
}

/**
 ****************************************************************************
 * Destroy a vlc object (Internal)
 *
 * This function destroys an object that has been previously allocated with
 * vlc_object_create. The object's refcount must be zero and it must not be
 * attached to other objects in any way.
 *****************************************************************************/
static void vlc_object_destroy( vlc_object_t *p_this )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    /* Objects are always detached beforehand */
    assert( !p_this->p_parent );

    /* Send a kill to the object's thread if applicable */
    vlc_object_kill( p_this );

    /* If we are running on a thread, wait until it ends */
    if( p_priv->b_thread )
    {
        msg_Warn (p_this->p_libvlc, /* do NOT use a dead object for logging! */
                  "%s %d destroyed while thread alive (VLC might crash)",
                  p_this->psz_object_type, p_this->i_object_id);
        vlc_thread_join( p_this );
    }

    /* Call the custom "subclass" destructor */
    if( p_priv->pf_destructor )
        p_priv->pf_destructor( p_this );

    /* Destroy the associated variables, starting from the end so that
     * no memmove calls have to be done. */
    while( p_priv->i_vars )
    {
        var_Destroy( p_this, p_priv->p_vars[p_priv->i_vars - 1].psz_name );
    }

    free( p_priv->p_vars );
    vlc_mutex_destroy( &p_priv->var_lock );

    free( p_this->psz_header );

    if( p_this->p_libvlc == NULL )
    {
#ifndef NDEBUG
        libvlc_global_data_t *p_global = (libvlc_global_data_t *)p_this;

        assert( p_global == vlc_global() );
        /* Test for leaks */
        if (p_priv->next != p_this)
        {
            vlc_object_t *leaked = p_priv->next, *first = leaked;
            do
            {
                /* We are leaking this object */
                fprintf( stderr,
                         "ERROR: leaking object (id:%i, type:%s, name:%s)\n",
                         leaked->i_object_id, leaked->psz_object_type,
                         leaked->psz_object_name );
                /* Dump libvlc object to ease debugging */
                vlc_object_dump( leaked );
                fflush(stderr);
                leaked = vlc_internals (leaked)->next;
            }
            while (leaked != first);

            /* Dump global object to ease debugging */
            vlc_object_dump( p_this );
            /* Strongly abort, cause we want these to be fixed */
            abort();
        }
#endif

        /* We are the global object ... no need to lock. */
        vlc_mutex_destroy( &structure_lock );
#ifdef LIBVLC_REFCHECK
        held_objects_destroy( vlc_threadvar_get( &held_objects ) );
        vlc_threadvar_delete( &held_objects );
#endif
    }

    FREENULL( p_this->psz_object_name );

#if defined(WIN32) || defined(UNDER_CE)
    /* if object has an associated thread, close it now */
    if( p_priv->thread_id )
       CloseHandle(p_priv->thread_id);
#endif

    vlc_spin_destroy( &p_priv->ref_spin );
    vlc_mutex_destroy( &p_priv->lock );
    vlc_cond_destroy( &p_priv->wait );
    vlc_spin_destroy( &p_priv->spin );
    if( p_priv->pipes[1] != -1 )
        close( p_priv->pipes[1] );
    if( p_priv->pipes[0] != -1 )
        close( p_priv->pipes[0] );

    free( p_priv );
}


/** Inter-object signaling */

void __vlc_object_lock( vlc_object_t *obj )
{
    vlc_mutex_lock( &(vlc_internals(obj)->lock) );
}

void __vlc_object_unlock( vlc_object_t *obj )
{
    vlc_assert_locked( &(vlc_internals(obj)->lock) );
    vlc_mutex_unlock( &(vlc_internals(obj)->lock) );
}

#ifdef WIN32
# include <winsock2.h>
# include <ws2tcpip.h>

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

#undef  read
#define read( a, b, c )  recv (a, b, c, 0)
#undef  write
#define write( a, b, c ) send (a, b, c, 0)
#undef  close
#define close( a )       closesocket (a)
#endif /* WIN32 */

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
int __vlc_object_waitpipe( vlc_object_t *obj )
{
    int pfd[2] = { -1, -1 };
    vlc_object_internals_t *internals = vlc_internals( obj );
    bool killed = false;

    vlc_spin_lock (&internals->spin);
    if (internals->pipes[0] == -1)
    {
        /* This can only ever happen if someone killed us without locking: */
        assert (internals->pipes[1] == -1);
        vlc_spin_unlock (&internals->spin);

        if (pipe (pfd))
            return -1;

        vlc_spin_lock (&internals->spin);
        if (internals->pipes[0] == -1)
        {
            internals->pipes[0] = pfd[0];
            internals->pipes[1] = pfd[1];
            pfd[0] = pfd[1] = -1;
        }
        killed = obj->b_die;
    }
    vlc_spin_unlock (&internals->spin);

    if (killed)
    {
        /* Race condition: vlc_object_kill() already invoked! */
        int fd;

        vlc_spin_lock (&internals->spin);
        fd = internals->pipes[1];
        internals->pipes[1] = -1;
        vlc_spin_unlock (&internals->spin);

        msg_Dbg (obj, "waitpipe: object already dying");
        if (fd != -1)
            close (fd);
    }

    /* Race condition: two threads call pipe() - unlikely */
    if (pfd[0] != -1)
        close (pfd[0]);
    if (pfd[1] != -1)
        close (pfd[1]);

    return internals->pipes[0];
}


/**
 * Waits for the object to be signaled (using vlc_object_signal()).
 * It is assumed that the caller has locked the object. This function will
 * unlock the object, and lock it again before returning.
 * If the object was signaled before the caller locked the object, it is
 * undefined whether the signal will be lost or will wake the process.
 *
 * @return true if the object is dying and should terminate.
 */
void __vlc_object_wait( vlc_object_t *obj )
{
    vlc_object_internals_t *priv = vlc_internals( obj );
    vlc_assert_locked( &priv->lock);
    vlc_cond_wait( &priv->wait, &priv->lock );
}


/**
 * Waits for the object to be signaled (using vlc_object_signal()), or for
 * a timer to expire. It is asserted that the caller holds the object lock.
 *
 * @return 0 if the object was signaled before the timer expiration, or
 * ETIMEDOUT if the timer expired without any signal.
 */
int __vlc_object_timedwait( vlc_object_t *obj, mtime_t deadline )
{
    vlc_object_internals_t *priv = vlc_internals( obj );
    vlc_assert_locked( &priv->lock);
    return vlc_cond_timedwait( &priv->wait, &priv->lock, deadline );
}


/**
 * Signals an object for which the lock is held.
 * At least one thread currently sleeping in vlc_object_wait() or
 * vlc_object_timedwait() will wake up, assuming that there is at least one
 * such thread in the first place. Otherwise, it is undefined whether the
 * signal will be lost or will wake up one or more thread later.
 */
void __vlc_object_signal_unlocked( vlc_object_t *obj )
{
    vlc_assert_locked (&(vlc_internals(obj)->lock));
    vlc_cond_signal( &(vlc_internals(obj)->wait) );
}


/**
 * Requests termination of an object.
 * If the object is LibVLC, also request to terminate all its children.
 */
void __vlc_object_kill( vlc_object_t *p_this )
{
    vlc_object_internals_t *priv = vlc_internals( p_this );
    int fd;

    vlc_object_lock( p_this );
    p_this->b_die = true;

    vlc_spin_lock (&priv->spin);
    fd = priv->pipes[1];
    priv->pipes[1] = -1;
    vlc_spin_unlock (&priv->spin);

    if( fd != -1 )
    {
        msg_Dbg (p_this, "waitpipe: object killed");
        close (fd);
    }

    vlc_object_signal_unlocked( p_this );
    /* This also serves as a memory barrier toward vlc_object_alive(): */
    vlc_object_unlock( p_this );
}


/**
 * Find an object given its ID.
 *
 * This function looks for the object whose i_object_id field is i_id.
 * This function is slow, and often used to hide bugs. Do not use it.
 * If you need to retain reference to an object, yield the object pointer with
 * vlc_object_yield(), use the pointer as your reference, and call
 * vlc_object_release() when you're done.
 */
void * vlc_object_get( int i_id )
{
    libvlc_global_data_t *p_libvlc_global = vlc_global();
    vlc_object_t *obj = NULL;
#ifndef NDEBUG
    vlc_object_t *caller = vlc_threadobj ();

    if (caller)
        msg_Dbg (caller, "uses deprecated vlc_object_get(%d)", i_id);
    else
        fprintf (stderr, "main thread uses deprecated vlc_object_get(%d)\n",
                 i_id);
#endif
    vlc_mutex_lock( &structure_lock );

    for( obj = vlc_internals (p_libvlc_global)->next;
         obj != VLC_OBJECT (p_libvlc_global);
         obj = vlc_internals (obj)->next )
    {
        if( obj->i_object_id == i_id )
        {
            vlc_object_yield( obj );
            goto out;
        }
    }
    obj = NULL;
#ifndef NDEBUG
    if (caller)
        msg_Warn (caller, "wants non-existing object %d", i_id);
    else
        fprintf (stderr, "main thread wants non-existing object %d\n", i_id);
#endif
out:
    vlc_mutex_unlock( &structure_lock );
    return obj;
}

/**
 ****************************************************************************
 * find a typed object and increment its refcount
 *****************************************************************************
 * This function recursively looks for a given object type. i_mode can be one
 * of FIND_PARENT, FIND_CHILD or FIND_ANYWHERE.
 *****************************************************************************/
void * __vlc_object_find( vlc_object_t *p_this, int i_type, int i_mode )
{
    vlc_object_t *p_found;

    /* If we are of the requested type ourselves, don't look further */
    if( !(i_mode & FIND_STRICT) && p_this->i_object_type == i_type )
    {
        vlc_object_yield( p_this );
        return p_this;
    }

    /* Otherwise, recursively look for the object */
    if ((i_mode & 0x000f) == FIND_ANYWHERE)
    {
#ifndef NDEBUG
        if (i_type == VLC_OBJECT_PLAYLIST)
	    msg_Err (p_this, "using vlc_object_find(VLC_OBJECT_PLAYLIST) "
                     "instead of pl_Yield()");
#endif
        return vlc_object_find (p_this->p_libvlc, i_type,
                                (i_mode & ~0x000f)|FIND_CHILD);
    }

    vlc_mutex_lock( &structure_lock );
    p_found = FindObject( p_this, i_type, i_mode );
    vlc_mutex_unlock( &structure_lock );
    return p_found;
}

/**
 ****************************************************************************
 * find a named object and increment its refcount
 *****************************************************************************
 * This function recursively looks for a given object name. i_mode can be one
 * of FIND_PARENT, FIND_CHILD or FIND_ANYWHERE.
 *****************************************************************************/
void * __vlc_object_find_name( vlc_object_t *p_this, const char *psz_name,
                               int i_mode )
{
    vlc_object_t *p_found;

    /* If have the requested name ourselves, don't look further */
    if( !(i_mode & FIND_STRICT)
        && p_this->psz_object_name
        && !strcmp( p_this->psz_object_name, psz_name ) )
    {
        vlc_object_yield( p_this );
        return p_this;
    }

    vlc_mutex_lock( &structure_lock );

    /* Otherwise, recursively look for the object */
    if( (i_mode & 0x000f) == FIND_ANYWHERE )
    {
        vlc_object_t *p_root = p_this;

        /* Find the root */
        while( p_root->p_parent != NULL &&
               p_root != VLC_OBJECT( p_this->p_libvlc ) )
        {
            p_root = p_root->p_parent;
        }

        p_found = FindObjectName( p_root, psz_name,
                                 (i_mode & ~0x000f)|FIND_CHILD );
        if( p_found == NULL && p_root != VLC_OBJECT( p_this->p_libvlc ) )
        {
            p_found = FindObjectName( VLC_OBJECT( p_this->p_libvlc ),
                                      psz_name, (i_mode & ~0x000f)|FIND_CHILD );
        }
    }
    else
    {
        p_found = FindObjectName( p_this, psz_name, i_mode );
    }

    vlc_mutex_unlock( &structure_lock );

    return p_found;
}

/**
 * Increment an object reference counter.
 */
void __vlc_object_yield( vlc_object_t *p_this )
{
    vlc_object_internals_t *internals = vlc_internals( p_this );

    vlc_spin_lock( &internals->ref_spin );
    /* Avoid obvious freed object uses */
    assert( internals->i_refcount > 0 );
    /* Increment the counter */
    internals->i_refcount++;
    vlc_spin_unlock( &internals->ref_spin );
#ifdef LIBVLC_REFCHECK
    /* Update the list of referenced objects */
    /* Using TLS, so no need to lock */
    /* The following line may leak memory if a thread leaks objects. */
    held_list_t *newhead = malloc (sizeof (*newhead));
    held_list_t *oldhead = vlc_threadvar_get (&held_objects);
    newhead->next = oldhead;
    newhead->obj = p_this;
    vlc_threadvar_set (&held_objects, newhead);
#endif
}

/*****************************************************************************
 * decrement an object refcount
 * And destroy the object if its refcount reach zero.
 *****************************************************************************/
void __vlc_object_release( vlc_object_t *p_this )
{
    vlc_object_internals_t *internals = vlc_internals( p_this );
    bool b_should_destroy;

#ifdef LIBVLC_REFCHECK
    /* Update the list of referenced objects */
    /* Using TLS, so no need to lock */
    for (held_list_t *hlcur = vlc_threadvar_get (&held_objects),
                     *hlprev = NULL;
         hlcur != NULL;
         hlprev = hlcur, hlcur = hlcur->next)
    {
        if (hlcur->obj == p_this)
        {
            if (hlprev == NULL)
                vlc_threadvar_set (&held_objects, hlcur->next);
            else
                hlprev->next = hlcur->next;
            free (hlcur);
            break;
        }
    }
    /* TODO: what if releasing without references? */
#endif

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
    vlc_mutex_lock( &structure_lock );
    /* Take the spin again. Note that another thread may have yielded the
     * object in the (very short) mean time. */
    vlc_spin_lock( &internals->ref_spin );
    b_should_destroy = --internals->i_refcount == 0;
    vlc_spin_unlock( &internals->ref_spin );

    if( b_should_destroy )
    {
        /* Remove the object from object list
         * so that it cannot be encountered by vlc_object_get() */
        vlc_internals (internals->next)->prev = internals->prev;
        vlc_internals (internals->prev)->next = internals->next;

        /* Detach from parent to protect against FIND_CHILDREN */
        vlc_object_detach_unlocked (p_this);
        /* Detach from children to protect against FIND_PARENT */
        for (int i = 0; i < internals->i_children; i++)
            internals->pp_children[i]->p_parent = NULL;
    }

    vlc_mutex_unlock( &structure_lock );

    if( b_should_destroy )
    {
        free( internals->pp_children );
        internals->pp_children = NULL;
        internals->i_children = 0;
        vlc_object_destroy( p_this );
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

    vlc_mutex_lock( &structure_lock );

    /* Attach the parent to its child */
    assert (!p_this->p_parent);
    p_this->p_parent = p_parent;

    /* Attach the child to its parent */
    vlc_object_internals_t *priv = vlc_internals( p_parent );
    INSERT_ELEM( priv->pp_children, priv->i_children, priv->i_children,
                 p_this );

    vlc_mutex_unlock( &structure_lock );
}


static void vlc_object_detach_unlocked (vlc_object_t *p_this)
{
    vlc_assert_locked (&structure_lock);

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
        priv->pp_children = (vlc_object_t **)realloc( priv->pp_children,
                               priv->i_children * sizeof(vlc_object_t *) );
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
    if( !p_this ) return;

    vlc_mutex_lock( &structure_lock );
    if( !p_this->p_parent )
        msg_Err( p_this, "object is not attached" );
    else
        vlc_object_detach_unlocked( p_this );
    vlc_mutex_unlock( &structure_lock );
}


/**
 ****************************************************************************
 * find a list typed objects and increment their refcount
 *****************************************************************************
 * This function recursively looks for a given object type. i_mode can be one
 * of FIND_PARENT, FIND_CHILD or FIND_ANYWHERE.
 *****************************************************************************/
vlc_list_t * __vlc_list_find( vlc_object_t *p_this, int i_type, int i_mode )
{
    vlc_list_t *p_list;
    int i_count = 0;

    /* Look for the objects */
    switch( i_mode & 0x000f )
    {
    case FIND_ANYWHERE:
        /* Modules should probably not be object, and the module should perhaps
         * not be shared across LibVLC instances. In the mean time, this ugly
         * hack is brought to you by Courmisch. */
        if (i_type == VLC_OBJECT_MODULE)
            return vlc_list_find ((vlc_object_t *)vlc_global ()->p_module_bank,
                                  i_type, FIND_CHILD);
        return vlc_list_find (p_this->p_libvlc, i_type, FIND_CHILD);

    case FIND_CHILD:
        vlc_mutex_lock( &structure_lock );
        i_count = CountChildren( p_this, i_type );
        p_list = NewList( i_count );

        /* Check allocation was successful */
        if( p_list->i_count != i_count )
        {
            vlc_mutex_unlock( &structure_lock );
            msg_Err( p_this, "list allocation failed!" );
            p_list->i_count = 0;
            break;
        }

        p_list->i_count = 0;
        ListChildren( p_list, p_this, i_type );
        vlc_mutex_unlock( &structure_lock );
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

    vlc_mutex_lock( &structure_lock );
    l = NewList( priv->i_children );
    for (int i = 0; i < l->i_count; i++)
    {
        vlc_object_yield( priv->pp_children[i] );
        l->p_values[i].p_object = priv->pp_children[i];
    }
    vlc_mutex_unlock( &structure_lock );
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
    (void)oldval; (void)p_data;
    if( *psz_cmd == 'l' )
    {
        vlc_object_t *root = VLC_OBJECT (vlc_global ()), *cur = root; 

        vlc_mutex_lock( &structure_lock );
        do
        {
            PrintObject (cur, "");
            cur = vlc_internals (cur)->next;
        }
        while (cur != root);
        vlc_mutex_unlock( &structure_lock );
    }
    else
    {
        vlc_object_t *p_object = NULL;

        if( *newval.psz_string )
        {
            char *end;
            int i_id = strtol( newval.psz_string, &end, 0 );
            if( end != newval.psz_string )
                p_object = vlc_object_get( i_id );
            else
                /* try using the object's name to find it */
                p_object = vlc_object_find_name( p_this, newval.psz_string,
                                                 FIND_ANYWHERE );

            if( !p_object )
            {
                return VLC_ENOOBJ;
            }
        }

        vlc_mutex_lock( &structure_lock );

        if( *psz_cmd == 't' )
        {
            char psz_foo[2 * MAX_DUMPSTRUCTURE_DEPTH + 1];

            if( !p_object )
                p_object = p_this->p_libvlc ? VLC_OBJECT(p_this->p_libvlc) : p_this;

            psz_foo[0] = '|';
            DumpStructure( p_object, 0, psz_foo );
        }
        else if( *psz_cmd == 'v' )
        {
            int i;

            if( !p_object )
                p_object = p_this->p_libvlc ? VLC_OBJECT(p_this->p_libvlc) : p_this;

            PrintObject( p_object, "" );

            if( !vlc_internals( p_object )->i_vars )
                printf( " `-o No variables\n" );
            for( i = 0; i < vlc_internals( p_object )->i_vars; i++ )
            {
                variable_t *p_var = vlc_internals( p_object )->p_vars + i;

                const char *psz_type = "unknown";
                switch( p_var->i_type & VLC_VAR_TYPE )
                {
#define MYCASE( type, nice )                \
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
                printf( " %c-o \"%s\" (%s",
                        i + 1 == vlc_internals( p_object )->i_vars ? '`' : '|',
                        p_var->psz_name, psz_type );
                if( p_var->psz_text )
                    printf( ", %s", p_var->psz_text );
                printf( ")" );
                if( p_var->i_type & VLC_VAR_ISCOMMAND )
                    printf( ", command" );
                if( p_var->i_entries )
                    printf( ", %d callbacks", p_var->i_entries );
                switch( p_var->i_type & 0x00f0 )
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
                        printf( ": TODO" );
                        break;
                }
                printf( "\n" );
            }
        }

        vlc_mutex_unlock( &structure_lock );

        if( *newval.psz_string )
        {
            vlc_object_release( p_object );
        }
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

/*****************************************************************************
 * dump an object. (Debug function)
 *****************************************************************************/
void __vlc_object_dump( vlc_object_t *p_this )
{
    vlc_mutex_lock( &structure_lock );
    char psz_foo[2 * MAX_DUMPSTRUCTURE_DEPTH + 1];
    psz_foo[0] = '|';
    DumpStructure( p_this, 0, psz_foo );
    vlc_mutex_unlock( &structure_lock );
}

/* Following functions are local */

static vlc_object_t * FindObject( vlc_object_t *p_this, int i_type, int i_mode )
{
    int i;
    vlc_object_t *p_tmp;

    switch( i_mode & 0x000f )
    {
    case FIND_PARENT:
        p_tmp = p_this->p_parent;
        if( p_tmp )
        {
            if( p_tmp->i_object_type == i_type )
            {
                vlc_object_yield( p_tmp );
                return p_tmp;
            }
            else
            {
                return FindObject( p_tmp, i_type, i_mode );
            }
        }
        break;

    case FIND_CHILD:
        for( i = vlc_internals( p_this )->i_children; i--; )
        {
            p_tmp = vlc_internals( p_this )->pp_children[i];
            if( p_tmp->i_object_type == i_type )
            {
                vlc_object_yield( p_tmp );
                return p_tmp;
            }
            else if( vlc_internals( p_tmp )->i_children )
            {
                p_tmp = FindObject( p_tmp, i_type, i_mode );
                if( p_tmp )
                {
                    return p_tmp;
                }
            }
        }
        break;

    case FIND_ANYWHERE:
        /* Handled in vlc_object_find */
        break;
    }

    return NULL;
}

static vlc_object_t * FindObjectName( vlc_object_t *p_this,
                                      const char *psz_name,
                                      int i_mode )
{
    int i;
    vlc_object_t *p_tmp;

    switch( i_mode & 0x000f )
    {
    case FIND_PARENT:
        p_tmp = p_this->p_parent;
        if( p_tmp )
        {
            if( p_tmp->psz_object_name
                && !strcmp( p_tmp->psz_object_name, psz_name ) )
            {
                vlc_object_yield( p_tmp );
                return p_tmp;
            }
            else
            {
                return FindObjectName( p_tmp, psz_name, i_mode );
            }
        }
        break;

    case FIND_CHILD:
        for( i = vlc_internals( p_this )->i_children; i--; )
        {
            p_tmp = vlc_internals( p_this )->pp_children[i];
            if( p_tmp->psz_object_name
                && !strcmp( p_tmp->psz_object_name, psz_name ) )
            {
                vlc_object_yield( p_tmp );
                return p_tmp;
            }
            else if( vlc_internals( p_tmp )->i_children )
            {
                p_tmp = FindObjectName( p_tmp, psz_name, i_mode );
                if( p_tmp )
                {
                    return p_tmp;
                }
            }
        }
        break;

    case FIND_ANYWHERE:
        /* Handled in vlc_object_find */
        break;
    }

    return NULL;
}


static void PrintObject( vlc_object_t *p_this, const char *psz_prefix )
{
    char psz_children[20], psz_refcount[20], psz_thread[30], psz_name[50],
         psz_parent[20];

    memset( &psz_name, 0, sizeof(psz_name) );
    if( p_this->psz_object_name )
    {
        snprintf( psz_name, 49, " \"%s\"", p_this->psz_object_name );
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
        snprintf( psz_parent, 19, ", parent %i", p_this->p_parent->i_object_id );

    printf( " %so %.8i %s%s%s%s%s%s\n", psz_prefix,
            p_this->i_object_id, p_this->psz_object_type,
            psz_name, psz_thread, psz_refcount, psz_children,
            psz_parent );
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
    vlc_list_t * p_list = (vlc_list_t *)malloc( sizeof( vlc_list_t ) );
    if( p_list == NULL )
    {
        return NULL;
    }

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

    vlc_object_yield( p_object );

    p_list->p_values[i_index].p_object = p_object;

    return;
}

/*static void ListAppend( vlc_list_t *p_list, vlc_object_t *p_object )
{
    if( p_list == NULL )
    {
        return;
    }

    p_list->p_values = realloc( p_list->p_values, (p_list->i_count + 1)
                                * sizeof( vlc_value_t ) );
    if( p_list->p_values == NULL )
    {
        p_list->i_count = 0;
        return;
    }

    vlc_object_yield( p_object );

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

        if( p_tmp->i_object_type == i_type )
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

        if( p_tmp->i_object_type == i_type )
            ListReplace( p_list, p_tmp, p_list->i_count++ );

        ListChildren( p_list, p_tmp, i_type );
    }
}

#ifdef LIBVLC_REFCHECK
# if defined(HAVE_EXECINFO_H) && defined(HAVE_BACKTRACE)
#  include <execinfo.h>
# endif

void vlc_refcheck (vlc_object_t *obj)
{
    static unsigned errors = 0;
    if (errors > 100)
        return;

    /* Anyone can use the root object (though it should not exist) */
    if (obj == VLC_OBJECT (vlc_global ()))
        return;

    /* Anyone can use its libvlc instance object */
    if (obj == VLC_OBJECT (obj->p_libvlc))
        return;

    /* The thread that created the object holds the initial reference */
    vlc_object_internals_t *priv = vlc_internals (obj);
#if defined (LIBVLC_USE_PTHREAD)
    if (pthread_equal (priv->creator_id, pthread_self ()))
#elif defined WIN32
    if (priv->creator_id == GetCurrentThreadId ())
#else
    if (0)
#endif
        return;

    /* A thread can use its own object without references! */
    vlc_object_t *caller = vlc_threadobj ();
    if (caller == obj)
        return;
#if 0
    /* The calling thread is younger than the object.
     * Access could be valid through cross-thread synchronization;
     * we would need better accounting. */
    if (caller && (caller->i_object_id > obj->i_object_id))
        return;
#endif
    int refs;
    vlc_spin_lock (&priv->ref_spin);
    refs = priv->i_refcount;
    vlc_spin_unlock (&priv->ref_spin);

    for (held_list_t *hlcur = vlc_threadvar_get (&held_objects);
         hlcur != NULL; hlcur = hlcur->next)
        if (hlcur->obj == obj)
            return;

    fprintf (stderr, "The %s %s thread object is accessing...\n"
             "the %s %s object without references.\n",
             caller && caller->psz_object_name
                     ? caller->psz_object_name : "unnamed",
             caller ? caller->psz_object_type : "main",
             obj->psz_object_name ? obj->psz_object_name : "unnamed",
             obj->psz_object_type);
    fflush (stderr);

#ifdef HAVE_BACKTRACE
    void *stack[20];
    int stackdepth = backtrace (stack, sizeof (stack) / sizeof (stack[0]));
    backtrace_symbols_fd (stack, stackdepth, 2);
#endif

    if (++errors == 100)
        fprintf (stderr, "Too many reference errors!\n");
}

static void held_objects_destroy (void *data)
{
    VLC_UNUSED( data );
    held_list_t *hl = vlc_threadvar_get (&held_objects);
    vlc_object_t *caller = vlc_threadobj ();

    while (hl != NULL)
    {
        held_list_t *buf = hl->next;
        vlc_object_t *obj = hl->obj;

        fprintf (stderr, "The %s %s thread object leaked a reference to...\n"
                         "the %s %s object.\n",
                 caller && caller->psz_object_name
                     ? caller->psz_object_name : "unnamed",
                 caller ? caller->psz_object_type : "main",
                 obj->psz_object_name ? obj->psz_object_name : "unnamed",
                 obj->psz_object_type);
        free (hl);
        hl = buf;
    }
}
#endif
