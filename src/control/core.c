/*****************************************************************************
 * core.c: Core libvlc new API functions : initialization, exceptions handling
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libvlc_internal.h"
#include <vlc/libvlc.h>

#include <vlc_interface.h>
#include <vlc_vlm.h>

#include <stdarg.h>
#include <limits.h>
#include <assert.h>

static const char nomemstr[] = "Insufficient memory";

/*************************************************************************
 * Exceptions handling
 *************************************************************************/
void libvlc_exception_init( libvlc_exception_t *p_exception )
{
    p_exception->b_raised = 0;
}

void libvlc_exception_clear( libvlc_exception_t *p_exception )
{
    if( NULL == p_exception )
        return;
    p_exception->b_raised = 0;
    libvlc_clearerr ();
}

int libvlc_exception_raised( const libvlc_exception_t *p_exception )
{
    return (NULL != p_exception) && p_exception->b_raised;
}

static void libvlc_exception_not_handled( const char *psz )
{
    fprintf( stderr, "*** LibVLC Exception not handled: %s\nSet a breakpoint in '%s' to debug.\n",
             psz, __func__ );
    abort();
}

void libvlc_exception_raise( libvlc_exception_t *p_exception,
                             const char *psz_format, ... )
{
    va_list args;

    /* Make sure that there is no unnoticed previous exception */
    if( p_exception && p_exception->b_raised )
    {
        libvlc_exception_not_handled( libvlc_errmsg() );
        libvlc_exception_clear( p_exception );
    }

    /* Unformat-ize the message */
    va_start( args, psz_format );
    libvlc_vprinterr( psz_format, args );
    va_end( args );

    /* Does caller care about exceptions ? */
    if( p_exception == NULL ) {
        /* Print something, so that lazy third-parties can easily
         * notice that something may have gone unnoticedly wrong */
        libvlc_exception_not_handled( libvlc_errmsg() );
        return;
    }

    p_exception->b_raised = 1;
}

libvlc_instance_t * libvlc_new( int argc, const char *const *argv,
                                libvlc_exception_t *p_e )
{
    libvlc_instance_t *p_new;
    int i_ret;

    libvlc_init_threads ();

    libvlc_int_t *p_libvlc_int = libvlc_InternalCreate();
    if( !p_libvlc_int )
    {
        libvlc_deinit_threads ();
        RAISENULL( "VLC initialization failed" );
    }

    p_new = malloc( sizeof( libvlc_instance_t ) );
    if( !p_new )
    {
        libvlc_deinit_threads ();
        RAISENULL( "Out of memory" );
    }

    const char *my_argv[argc + 2];

    my_argv[0] = "libvlc"; /* dummy arg0, skipped by getopt() et al */
    for( int i = 0; i < argc; i++ )
         my_argv[i + 1] = argv[i];
    my_argv[argc + 1] = NULL; /* C calling conventions require a NULL */

    /** \todo Look for interface settings. If we don't have any, add -I dummy */
    /* Because we probably don't want a GUI by default */

    i_ret = libvlc_InternalInit( p_libvlc_int, argc + 1, my_argv );
    if( i_ret )
    {
        libvlc_InternalDestroy( p_libvlc_int );
        free( p_new );
        libvlc_deinit_threads ();

        if( i_ret == VLC_EEXITSUCCESS )
            return NULL;
        else
            RAISENULL( "VLC initialization failed" );
    }

    p_new->p_libvlc_int = p_libvlc_int;
    p_new->libvlc_vlm.p_vlm = NULL;
    p_new->libvlc_vlm.p_event_manager = NULL;
    p_new->libvlc_vlm.pf_release = NULL;
    p_new->ref_count = 1;
    p_new->verbosity = 1;
    p_new->p_callback_list = NULL;
    vlc_mutex_init(&p_new->instance_lock);

    return p_new;
}

void libvlc_retain( libvlc_instance_t *p_instance )
{
    assert( p_instance != NULL );
    assert( p_instance->ref_count < UINT_MAX );

    vlc_mutex_lock( &p_instance->instance_lock );
    p_instance->ref_count++;
    vlc_mutex_unlock( &p_instance->instance_lock );
}

void libvlc_release( libvlc_instance_t *p_instance )
{
    vlc_mutex_t *lock = &p_instance->instance_lock;
    int refs;

    vlc_mutex_lock( lock );
    assert( p_instance->ref_count > 0 );
    refs = --p_instance->ref_count;
    vlc_mutex_unlock( lock );

    if( refs == 0 )
    {
        vlc_mutex_destroy( lock );
        if( p_instance->libvlc_vlm.pf_release )
            p_instance->libvlc_vlm.pf_release( p_instance );
        libvlc_InternalCleanup( p_instance->p_libvlc_int );
        libvlc_InternalDestroy( p_instance->p_libvlc_int );
        free( p_instance );
        libvlc_deinit_threads ();
    }
}

int libvlc_add_intf( libvlc_instance_t *p_i, const char *name,
                      libvlc_exception_t *p_e )
{
    if( libvlc_InternalAddIntf( p_i->p_libvlc_int, name ) )
    {
        libvlc_exception_raise( p_e, "Interface initialization failed" );
        return -1;
    }
    return 0;
}

void libvlc_wait( libvlc_instance_t *p_i )
{
    libvlc_int_t *p_libvlc = p_i->p_libvlc_int;
    libvlc_InternalWait( p_libvlc );
}

const char * libvlc_get_version(void)
{
    return VLC_Version();
}

const char * libvlc_get_compiler(void)
{
    return VLC_Compiler();
}

const char * libvlc_get_changeset(void)
{
    extern const char psz_vlc_changeset[];
    return psz_vlc_changeset;
}

/* export internal libvlc_instance for ugly hacks with libvlccore */
vlc_object_t *libvlc_get_vlc_instance( libvlc_instance_t* p_instance )
{
    vlc_object_hold( p_instance->p_libvlc_int ) ;
    return (vlc_object_t*) p_instance->p_libvlc_int ;
}

void libvlc_free( void *ptr )
{
    free( ptr );
}
