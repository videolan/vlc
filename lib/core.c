/*****************************************************************************
 * core.c: Core libvlc new API functions : initialization
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "libvlc_internal.h"
#include <vlc_modules.h>
#include <vlc/vlc.h>

#include <vlc_interface.h>

#include <stdarg.h>
#include <limits.h>
#include <assert.h>


libvlc_instance_t * libvlc_new( int argc, const char *const *argv )
{
    libvlc_threads_init ();

    libvlc_instance_t *p_new = malloc (sizeof (*p_new));
    if (unlikely(p_new == NULL))
        return NULL;

    const char *my_argv[argc + 2];
    my_argv[0] = "libvlc"; /* dummy arg0, skipped by getopt() et al */
    for( int i = 0; i < argc; i++ )
         my_argv[i + 1] = argv[i];
    my_argv[argc + 1] = NULL; /* C calling conventions require a NULL */

    libvlc_int_t *p_libvlc_int = libvlc_InternalCreate();
    if (unlikely (p_libvlc_int == NULL))
        goto error;

    if (libvlc_InternalInit( p_libvlc_int, argc + 1, my_argv ))
    {
        libvlc_InternalDestroy( p_libvlc_int );
        goto error;
    }

    p_new->p_libvlc_int = p_libvlc_int;
    vlc_atomic_rc_init( &p_new->ref_count );
    p_new->p_callback_list = NULL;
    return p_new;

error:
    free (p_new);
    libvlc_threads_deinit ();
    return NULL;
}

void libvlc_retain( libvlc_instance_t *p_instance )
{
    assert( p_instance != NULL );

    vlc_atomic_rc_inc( &p_instance->ref_count );
}

void libvlc_release( libvlc_instance_t *p_instance )
{
    if(vlc_atomic_rc_dec( &p_instance->ref_count ))
    {
        libvlc_Quit( p_instance->p_libvlc_int );
        libvlc_InternalCleanup( p_instance->p_libvlc_int );
        libvlc_InternalDestroy( p_instance->p_libvlc_int );
        free( p_instance );
        libvlc_threads_deinit ();
    }
}

void libvlc_set_exit_handler( libvlc_instance_t *p_i, void (*cb) (void *),
                              void *data )
{
    libvlc_int_t *p_libvlc = p_i->p_libvlc_int;
    libvlc_SetExitHandler( p_libvlc, cb, data );
}

void libvlc_set_user_agent (libvlc_instance_t *p_i,
                            const char *name, const char *http)
{
    libvlc_int_t *p_libvlc = p_i->p_libvlc_int;
    char *str;

    var_SetString (p_libvlc, "user-agent", name);
    if ((http != NULL)
     && (asprintf (&str, "%s LibVLC/"PACKAGE_VERSION, http) != -1))
    {
        var_SetString (p_libvlc, "http-user-agent", str);
        free (str);
    }
}

void libvlc_set_app_id(libvlc_instance_t *p_i, const char *id,
                       const char *version, const char *icon)
{
    libvlc_int_t *p_libvlc = p_i->p_libvlc_int;

    var_SetString(p_libvlc, "app-id", id ? id : "");
    var_SetString(p_libvlc, "app-version", version ? version : "");
    var_SetString(p_libvlc, "app-icon-name", icon ? icon : "");
}

const char * libvlc_get_version(void)
{
    return VERSION_MESSAGE;
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

void libvlc_free( void *ptr )
{
    free( ptr );
}

static libvlc_module_description_t *module_description_list_get(
                libvlc_instance_t *p_instance, const char *capability )
{
    libvlc_module_description_t *p_list = NULL,
                          *p_actual = NULL,
                          *p_previous = NULL;
    size_t count;
    module_t **module_list = module_list_get( &count );

    for (size_t i = 0; i < count; i++)
    {
        module_t *p_module = module_list[i];

        if ( !module_provides( p_module, capability ) )
            continue;

        p_actual = ( libvlc_module_description_t * ) malloc( sizeof( libvlc_module_description_t ) );
        if ( p_actual == NULL )
        {
            libvlc_printerr( "Not enough memory" );
            libvlc_module_description_list_release( p_list );
            module_list_free( module_list );
            return NULL;
        }

        if ( p_list == NULL )
            p_list = p_actual;

        const char* name = module_get_object( p_module );
        const char* shortname = module_get_name( p_module, false );
        const char* longname = module_get_name( p_module, true );
        const char* help = module_get_help( p_module );
        p_actual->psz_name = name ? strdup( name ) : NULL;
        p_actual->psz_shortname = shortname ? strdup( shortname ) : NULL;
        p_actual->psz_longname = longname ? strdup( longname ) : NULL;
        p_actual->psz_help = help ? strdup( help ) : NULL;

        p_actual->p_next = NULL;
        if ( p_previous )
            p_previous->p_next = p_actual;
        p_previous = p_actual;
    }

    module_list_free( module_list );
    VLC_UNUSED( p_instance );
    return p_list;
}

void libvlc_module_description_list_release( libvlc_module_description_t *p_list )
{
    libvlc_module_description_t *p_actual, *p_before;
    p_actual = p_list;

    while ( p_actual )
    {
        free( p_actual->psz_name );
        free( p_actual->psz_shortname );
        free( p_actual->psz_longname );
        free( p_actual->psz_help );
        p_before = p_actual;
        p_actual = p_before->p_next;
        free( p_before );
    }
}

libvlc_module_description_t *libvlc_audio_filter_list_get( libvlc_instance_t *p_instance )
{
    return module_description_list_get( p_instance, "audio filter" );
}

libvlc_module_description_t *libvlc_video_filter_list_get( libvlc_instance_t *p_instance )
{
    return module_description_list_get( p_instance, "video filter" );
}

int64_t libvlc_clock(void)
{
    return US_FROM_VLC_TICK(vlc_tick_now());
}

const char vlc_module_name[] = "libvlc";
