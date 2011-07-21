/*****************************************************************************
 * objects.c: vlc_object_t handling
 *****************************************************************************
 * Copyright (C) 2004-2008 the VideoLAN team
 * Copyright (C) 2006-2010 Rémi Denis-Courmont
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

#ifdef HAVE_SEARCH_H
# include <search.h>
#endif

#ifndef WIN32
# include <vlc_fs.h>
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

#include <limits.h>
#include <assert.h>

#if defined (HAVE_SYS_EVENTFD_H)
# include <sys/eventfd.h>
# ifndef EFD_CLOEXEC
#  define EFD_CLOEXEC 0
#  warning EFD_CLOEXEC missing. Consider updating libc.
# endif
#endif


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
    priv->pipes[0] = priv->pipes[1] = -1;
    vlc_spin_init (&priv->ref_spin);
    priv->i_refcount = 1;
    priv->pf_destructor = NULL;
    priv->prev = NULL;
    priv->first = NULL;

    vlc_object_t *obj = (vlc_object_t *)(priv + 1);
    obj->psz_object_type = typename;
    obj->psz_header = NULL;
    obj->b_die = false;
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
# define vlc_pipe selectable_pipe
static int selectable_pipe (int fd[2])
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

        /* pipe() is not a cancellation point, but write() is and eventfd() is
         * unspecified (not in POSIX). */
        int canc = vlc_savecancel ();
#if defined (HAVE_SYS_EVENTFD_H)
        internals->pipes[0] = internals->pipes[1] = eventfd (0, EFD_CLOEXEC);
        if (internals->pipes[0] == -1)
#endif
        {
            if (vlc_pipe (internals->pipes))
                internals->pipes[0] = internals->pipes[1] = -1;
        }

        if (internals->pipes[0] != -1 && obj->b_die)
        {   /* Race condition: vlc_object_kill() already invoked! */
            msg_Dbg (obj, "waitpipe: object already dying");
            write (internals->pipes[1], &(uint64_t){ 1 }, sizeof (uint64_t));
        }
        vlc_restorecancel (canc);
    }
    vlc_mutex_unlock (&pipe_lock);
    return internals->pipes[0];
}

#undef vlc_object_kill
/**
 * Requests termination of an object, cancels the object thread, and make the
 * object wait pipe (if it exists) readable. Not a cancellation point.
 */
void vlc_object_kill( vlc_object_t *p_this )
{
    vlc_object_internals_t *priv = vlc_internals( p_this );
    int fd = -1;

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

    /* Reading psz_object_name from a separate inhibits thread-safety.
     * Use a libvlc address variable instead for that sort of things! */
    msg_Err( p_this, "%s(\"%s\") is not safe!", __func__, psz_name );

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

    vlc_spin_lock( &internals->ref_spin );
    /* Avoid obvious freed object uses */
    assert( internals->i_refcount > 0 );
    /* Increment the counter */
    internals->i_refcount++;
    vlc_spin_unlock( &internals->ref_spin );
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
        /* Detach from parent to protect against vlc_object_find_name() */
        parent = p_this->p_parent;
        if (likely(parent))
        {
           /* Unlink */
           if (internals->prev != NULL)
               internals->prev->next = internals->next;
           else
               vlc_internals(parent)->first = internals->next;
           if (internals->next != NULL)
               internals->next->prev = internals->prev;
        }

        /* We have no children */
        assert (internals->first == NULL);
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

#undef vlc_list_children
/**
 * Gets the list of children of an objects, and increment their reference
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
            l->p_values[i++].p_object = vlc_object_hold (vlc_externals (priv));
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
        MYCASE( VARIABLE, "variable" );
        MYCASE( FLOAT, "float" );
        MYCASE( TIME, "time" );
        MYCASE( COORDS, "coords" );
        MYCASE( ADDRESS, "address" );
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
            break;
        case VLC_VAR_BOOL:
            printf( ": %s", p_var->val.b_bool ? "true" : "false" );
            break;
        case VLC_VAR_INTEGER:
            printf( ": %"PRId64, p_var->val.i_int );
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
        case VLC_VAR_COORDS:
            printf( ": %"PRId32"x%"PRId32,
                    p_var->val.coords.x, p_var->val.coords.y );
            break;
        case VLC_VAR_ADDRESS:
            printf( ": %p", p_var->val.p_address );
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

    psz_refcount[0] = '\0';
    if( priv->i_refcount > 0 )
        snprintf( psz_refcount, 19, ", %u refs", priv->i_refcount );

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
