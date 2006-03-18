/*****************************************************************************
 * objects.c: vlc_object_t handling
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
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
#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_STDLIB_H
#   include <stdlib.h>                                          /* realloc() */
#endif

#include "vlc_video.h"
#include "video_output.h"
#include "vlc_spu.h"

#include "audio_output.h"
#include "aout_internal.h"
#include "stream_output.h"

#include "vlc_playlist.h"
#include "vlc_interface.h"
#include "vlc_codec.h"
#include "vlc_filter.h"

#include "vlc_httpd.h"
#include "vlc_vlm.h"
#include "vlc_vod.h"
#include "vlc_tls.h"
#include "vlc_xml.h"
#include "vlc_osd.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DumpCommand( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

static vlc_object_t * FindObject    ( vlc_object_t *, int, int );
static void           DetachObject  ( vlc_object_t * );
static void           PrintObject   ( vlc_object_t *, const char * );
static void           DumpStructure ( vlc_object_t *, int, char * );
static int            FindIndex     ( vlc_object_t *, vlc_object_t **, int );
static void           SetAttachment ( vlc_object_t *, vlc_bool_t );

static vlc_list_t   * NewList       ( int );
static void           ListReplace   ( vlc_list_t *, vlc_object_t *, int );
/*static void           ListAppend    ( vlc_list_t *, vlc_object_t * );*/
static int            CountChildren ( vlc_object_t *, int );
static void           ListChildren  ( vlc_list_t *, vlc_object_t *, int );

/*****************************************************************************
 * Local structure lock
 *****************************************************************************/
static vlc_mutex_t    structure_lock;

/*****************************************************************************
 * vlc_object_create: initialize a vlc object
 *****************************************************************************
 * This function allocates memory for a vlc object and initializes it. If
 * i_type is not a known value such as VLC_OBJECT_ROOT, VLC_OBJECT_VOUT and
 * so on, vlc_object_create will use its value for the object size.
 *****************************************************************************/

/**
 * Initialize a vlc object
 *
 * This function allocates memory for a vlc object and initializes it. If
 * i_type is not a known value such as VLC_OBJECT_ROOT, VLC_OBJECT_VOUT and
 * so on, vlc_object_create will use its value for the object size.
 */
void * __vlc_object_create( vlc_object_t *p_this, int i_type )
{
    vlc_object_t * p_new;
    const char   * psz_type;
    size_t         i_size;

    switch( i_type )
    {
        case VLC_OBJECT_ROOT:
            i_size = sizeof(libvlc_t);
            psz_type = "root";
            break;
        case VLC_OBJECT_VLC:
            i_size = sizeof(vlc_t);
            psz_type = "vlc";
            break;
        case VLC_OBJECT_MODULE:
            i_size = sizeof(module_t);
            psz_type = "module";
            break;
        case VLC_OBJECT_INTF:
            i_size = sizeof(intf_thread_t);
            psz_type = "interface";
            break;
        case VLC_OBJECT_DIALOGS:
            i_size = sizeof(intf_thread_t);
            psz_type = "dialogs provider";
            break;
        case VLC_OBJECT_PLAYLIST:
            i_size = sizeof(playlist_t);
            psz_type = "playlist";
            break;
        case VLC_OBJECT_SD:
            i_size = sizeof(services_discovery_t);
            psz_type = "services discovery";
            break;
        case VLC_OBJECT_INPUT:
            i_size = sizeof(input_thread_t);
            psz_type = "input";
            break;
        case VLC_OBJECT_DEMUX:
            i_size = sizeof(demux_t);
            psz_type = "demux";
            break;
        case VLC_OBJECT_STREAM:
            i_size = sizeof(stream_t);
            psz_type = "stream";
            break;
        case VLC_OBJECT_ACCESS:
            i_size = sizeof(access_t);
            psz_type = "access";
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
        case VLC_OBJECT_FILTER:
            i_size = sizeof(filter_t);
            psz_type = "filter";
            break;
        case VLC_OBJECT_VOUT:
            i_size = sizeof(vout_thread_t);
            psz_type = "video output";
            break;
        case VLC_OBJECT_SPU:
            i_size = sizeof(spu_t);
            psz_type = "subpicture unit";
            break;
        case VLC_OBJECT_AOUT:
            i_size = sizeof(aout_instance_t);
            psz_type = "audio output";
            break;
        case VLC_OBJECT_SOUT:
            i_size = sizeof(sout_instance_t);
            psz_type = "stream output";
            break;
        case VLC_OBJECT_HTTPD:
            i_size = sizeof( httpd_t );
            psz_type = "http server";
            break;
        case VLC_OBJECT_HTTPD_HOST:
            i_size = sizeof( httpd_host_t );
            psz_type = "http server";
            break;
        case VLC_OBJECT_VLM:
            i_size = sizeof( vlm_t );
            psz_type = "vlm dameon";
            break;
        case VLC_OBJECT_VOD:
            i_size = sizeof( vod_t );
            psz_type = "vod server";
            break;
        case VLC_OBJECT_TLS:
            i_size = sizeof( tls_t );
            psz_type = "tls";
            break;
        case VLC_OBJECT_XML:
            i_size = sizeof( xml_t );
            psz_type = "xml";
            break;
        case VLC_OBJECT_OPENGL:
            i_size = sizeof( vout_thread_t );
            psz_type = "opengl provider";
            break;
        case VLC_OBJECT_ANNOUNCE:
            i_size = sizeof( announce_handler_t );
            psz_type = "announce handler";
            break;
        case VLC_OBJECT_OSDMENU:
            i_size = sizeof( osd_menu_t );
            psz_type = "osd menu";
            break;
        case VLC_OBJECT_STATS:
            i_size = sizeof( stats_handler_t );
            psz_type = "statistics";
            break;
        default:
            i_size = i_type > (int)sizeof(vlc_object_t)
                         ? i_type : (int)sizeof(vlc_object_t);
            i_type = VLC_OBJECT_GENERIC;
            psz_type = "generic";
            break;
    }

    if( i_type == VLC_OBJECT_ROOT )
    {
        p_new = p_this;
    }
    else
    {
        p_new = malloc( i_size );
        if( !p_new ) return NULL;
        memset( p_new, 0, i_size );
    }

    p_new->i_object_type = i_type;
    p_new->psz_object_type = psz_type;

    p_new->psz_object_name = NULL;

    p_new->b_die = VLC_FALSE;
    p_new->b_error = VLC_FALSE;
    p_new->b_dead = VLC_FALSE;
    p_new->b_attached = VLC_FALSE;
    p_new->b_force = VLC_FALSE;

    p_new->psz_header = NULL;

    p_new->i_flags = 0;
    if( p_this->i_flags & OBJECT_FLAGS_NODBG )
        p_new->i_flags |= OBJECT_FLAGS_NODBG;
    if( p_this->i_flags & OBJECT_FLAGS_QUIET )
        p_new->i_flags |= OBJECT_FLAGS_QUIET;
    if( p_this->i_flags & OBJECT_FLAGS_NOINTERACT )
        p_new->i_flags |= OBJECT_FLAGS_NOINTERACT;

    p_new->i_vars = 0;
    p_new->p_vars = (variable_t *)malloc( 16 * sizeof( variable_t ) );

    if( !p_new->p_vars )
    {
        if( i_type != VLC_OBJECT_ROOT )
            free( p_new );
        return NULL;
    }

    if( i_type == VLC_OBJECT_ROOT )
    {
        /* If i_type is root, then p_new is actually p_libvlc */
        p_new->p_libvlc = (libvlc_t*)p_new;
        p_new->p_vlc = NULL;

        p_new->p_libvlc->i_counter = 0;
        p_new->i_object_id = 0;

        p_new->p_libvlc->i_objects = 1;
        p_new->p_libvlc->pp_objects = malloc( sizeof(vlc_object_t *) );
        p_new->p_libvlc->pp_objects[0] = p_new;
        p_new->b_attached = VLC_TRUE;
    }
    else
    {
        p_new->p_libvlc = p_this->p_libvlc;
        p_new->p_vlc = ( i_type == VLC_OBJECT_VLC ) ? (vlc_t*)p_new
                                                    : p_this->p_vlc;

        vlc_mutex_lock( &structure_lock );

        p_new->p_libvlc->i_counter++;
        p_new->i_object_id = p_new->p_libvlc->i_counter;

        /* Wooohaa! If *this* fails, we're in serious trouble! Anyway it's
         * useless to try and recover anything if pp_objects gets smashed. */
        INSERT_ELEM( p_new->p_libvlc->pp_objects,
                     p_new->p_libvlc->i_objects,
                     p_new->p_libvlc->i_objects,
                     p_new );

        vlc_mutex_unlock( &structure_lock );
    }

    p_new->i_refcount = 0;
    p_new->p_parent = NULL;
    p_new->pp_children = NULL;
    p_new->i_children = 0;

    p_new->p_private = NULL;

    /* Initialize mutexes and condvars */
    vlc_mutex_init( p_new, &p_new->object_lock );
    vlc_cond_init( p_new, &p_new->object_wait );
    vlc_mutex_init( p_new, &p_new->var_lock );

    if( i_type == VLC_OBJECT_ROOT )
    {
        vlc_mutex_init( p_new, &structure_lock );

        var_Create( p_new, "list", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "list", DumpCommand, NULL );
        var_Create( p_new, "tree", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_new, "tree", DumpCommand, NULL );
    }

    return p_new;
}

/**
 ****************************************************************************
 * Destroy a vlc object
 *
 * This function destroys an object that has been previously allocated with
 * vlc_object_create. The object's refcount must be zero and it must not be
 * attached to other objects in any way.
 *****************************************************************************/
void __vlc_object_destroy( vlc_object_t *p_this )
{
    int i_delay = 0;

    if( p_this->i_children )
    {
        msg_Err( p_this, "cannot delete object (%i, %s) with children" ,
                 p_this->i_object_id, p_this->psz_object_name );
        return;
    }

    if( p_this->p_parent )
    {
        msg_Err( p_this, "cannot delete object (%i, %s) with a parent",
                 p_this->i_object_id, p_this->psz_object_name );
        return;
    }

    while( p_this->i_refcount )
    {
        i_delay++;

        /* Don't warn immediately ... 100ms seems OK */
        if( i_delay == 2 )
        {
            msg_Warn( p_this,
                  "refcount is %i, delaying before deletion (id=%d,type=%d)",
                  p_this->i_refcount, p_this->i_object_id,
                  p_this->i_object_type );
        }
        else if( i_delay == 10 )
        {
            msg_Err( p_this,
                  "refcount is %i, delaying again (id=%d,type=%d)",
                  p_this->i_refcount, p_this->i_object_id,
                  p_this->i_object_type );
        }
        else if( i_delay == 20 )
        {
            msg_Err( p_this,
                  "we waited too long, cancelling destruction (id=%d,type=%d)",
                  p_this->i_object_id, p_this->i_object_type );
            return;
        }

        msleep( 100000 );
    }

    /* Destroy the associated variables, starting from the end so that
     * no memmove calls have to be done. */
    while( p_this->i_vars )
    {
        var_Destroy( p_this, p_this->p_vars[p_this->i_vars - 1].psz_name );
    }

    free( p_this->p_vars );
    vlc_mutex_destroy( &p_this->var_lock );

    if( p_this->psz_header ) free( p_this->psz_header );

    if( p_this->i_object_type == VLC_OBJECT_ROOT )
    {
        /* We are the root object ... no need to lock. */
        free( p_this->p_libvlc->pp_objects );
        p_this->p_libvlc->pp_objects = NULL;
        p_this->p_libvlc->i_objects--;

        vlc_mutex_destroy( &structure_lock );
    }
    else
    {
        int i_index;

        vlc_mutex_lock( &structure_lock );

        /* Wooohaa! If *this* fails, we're in serious trouble! Anyway it's
         * useless to try and recover anything if pp_objects gets smashed. */
        i_index = FindIndex( p_this, p_this->p_libvlc->pp_objects,
                             p_this->p_libvlc->i_objects );
        REMOVE_ELEM( p_this->p_libvlc->pp_objects,
                     p_this->p_libvlc->i_objects, i_index );

        vlc_mutex_unlock( &structure_lock );
    }

    vlc_mutex_destroy( &p_this->object_lock );
    vlc_cond_destroy( &p_this->object_wait );

    /* root is not dynamically allocated by vlc_object_create */
    if( p_this->i_object_type != VLC_OBJECT_ROOT )
        free( p_this );
}

/**
 * find an object given its ID
 *
 * This function looks for the object whose i_object_id field is i_id. We
 * use a dichotomy so that lookups are in log2(n).
 *****************************************************************************/
void * __vlc_object_get( vlc_object_t *p_this, int i_id )
{
    int i_max, i_middle;
    vlc_object_t **pp_objects;

    vlc_mutex_lock( &structure_lock );

    pp_objects = p_this->p_libvlc->pp_objects;

    /* Perform our dichotomy */
    for( i_max = p_this->p_libvlc->i_objects - 1 ; ; )
    {
        i_middle = i_max / 2;

        if( pp_objects[i_middle]->i_object_id > i_id )
        {
            i_max = i_middle;
        }
        else if( pp_objects[i_middle]->i_object_id < i_id )
        {
            if( i_middle )
            {
                pp_objects += i_middle;
                i_max -= i_middle;
            }
            else
            {
                /* This happens when there are only two remaining objects */
                if( pp_objects[i_middle+1]->i_object_id == i_id )
                {
                    vlc_mutex_unlock( &structure_lock );
                    pp_objects[i_middle+1]->i_refcount++;
                    return pp_objects[i_middle+1];
                }
                break;
            }
        }
        else
        {
            vlc_mutex_unlock( &structure_lock );
            pp_objects[i_middle]->i_refcount++;
            return pp_objects[i_middle];
        }

        if( i_max == 0 )
        {
            /* this means that i_max == i_middle, and since we have already
             * tested pp_objects[i_middle]), p_found is properly set. */
            break;
        }
    }

    vlc_mutex_unlock( &structure_lock );
    return NULL;
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

    vlc_mutex_lock( &structure_lock );

    /* If we are of the requested type ourselves, don't look further */
    if( !(i_mode & FIND_STRICT) && p_this->i_object_type == i_type )
    {
        p_this->i_refcount++;
        vlc_mutex_unlock( &structure_lock );
        return p_this;
    }

    /* Otherwise, recursively look for the object */
    if( (i_mode & 0x000f) == FIND_ANYWHERE )
    {
        vlc_object_t *p_root = p_this;

        /* Find the root */
        while( p_root->p_parent != NULL &&
               p_root != VLC_OBJECT( p_this->p_vlc ) )
        {
            p_root = p_root->p_parent;
        }

        p_found = FindObject( p_root, i_type, (i_mode & ~0x000f)|FIND_CHILD );
        if( p_found == NULL && p_root != VLC_OBJECT( p_this->p_vlc ) )
        {
            p_found = FindObject( VLC_OBJECT( p_this->p_vlc ),
                                  i_type, (i_mode & ~0x000f)|FIND_CHILD );
        }
    }
    else
    {
        p_found = FindObject( p_this, i_type, i_mode );
    }

    vlc_mutex_unlock( &structure_lock );

    return p_found;
}

/**
 ****************************************************************************
 * increment an object refcount
 *****************************************************************************/
void __vlc_object_yield( vlc_object_t *p_this )
{
    vlc_mutex_lock( &structure_lock );
    p_this->i_refcount++;
    vlc_mutex_unlock( &structure_lock );
}

/**
 ****************************************************************************
 * decrement an object refcount
 *****************************************************************************/
void __vlc_object_release( vlc_object_t *p_this )
{
    vlc_mutex_lock( &structure_lock );
    p_this->i_refcount--;
    vlc_mutex_unlock( &structure_lock );
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
    vlc_mutex_lock( &structure_lock );

    /* Attach the parent to its child */
    p_this->p_parent = p_parent;

    /* Attach the child to its parent */
    INSERT_ELEM( p_parent->pp_children, p_parent->i_children,
                 p_parent->i_children, p_this );

    /* Climb up the tree to see whether we are connected with the root */
    if( p_parent->b_attached )
    {
        SetAttachment( p_this, VLC_TRUE );
    }

    vlc_mutex_unlock( &structure_lock );
}

/**
 ****************************************************************************
 * detach object from its parent
 *****************************************************************************
 * This function removes all links between an object and its parent.
 *****************************************************************************/
void __vlc_object_detach( vlc_object_t *p_this )
{
    vlc_mutex_lock( &structure_lock );
    if( !p_this->p_parent )
    {
        msg_Err( p_this, "object is not attached" );
        vlc_mutex_unlock( &structure_lock );
        return;
    }

    /* Climb up the tree to see whether we are connected with the root */
    if( p_this->p_parent->b_attached )
    {
        SetAttachment( p_this, VLC_FALSE );
    }

    DetachObject( p_this );
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
    vlc_object_t **pp_current, **pp_end;
    int i_count = 0, i_index = 0;

    vlc_mutex_lock( &structure_lock );

    /* Look for the objects */
    switch( i_mode & 0x000f )
    {
    case FIND_ANYWHERE:
        pp_current = p_this->p_libvlc->pp_objects;
        pp_end = pp_current + p_this->p_libvlc->i_objects;

        for( ; pp_current < pp_end ; pp_current++ )
        {
            if( (*pp_current)->b_attached
                 && (*pp_current)->i_object_type == i_type )
            {
                i_count++;
            }
        }

        p_list = NewList( i_count );
        pp_current = p_this->p_libvlc->pp_objects;

        for( ; pp_current < pp_end ; pp_current++ )
        {
            if( (*pp_current)->b_attached
                 && (*pp_current)->i_object_type == i_type )
            {
                ListReplace( p_list, *pp_current, i_index );
                if( i_index < i_count ) i_index++;
            }
        }
    break;

    case FIND_CHILD:
        i_count = CountChildren( p_this, i_type );
        p_list = NewList( i_count );

        /* Check allocation was successful */
        if( p_list->i_count != i_count )
        {
            msg_Err( p_this, "list allocation failed!" );
            p_list->i_count = 0;
            break;
        }

        p_list->i_count = 0;
        ListChildren( p_list, p_this, i_type );
        break;

    default:
        msg_Err( p_this, "unimplemented!" );
        p_list = NewList( 0 );
        break;
    }

    vlc_mutex_unlock( &structure_lock );

    return p_list;
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
    if( *psz_cmd == 't' )
    {
        char psz_foo[2 * MAX_DUMPSTRUCTURE_DEPTH + 1];
        vlc_object_t *p_object;

        if( *newval.psz_string )
        {
            p_object = vlc_object_get( p_this, atoi(newval.psz_string) );

            if( !p_object )
            {
                return VLC_ENOOBJ;
            }
        }
        else
        {
            p_object = p_this->p_vlc ? VLC_OBJECT(p_this->p_vlc) : p_this;
        }

        vlc_mutex_lock( &structure_lock );

        psz_foo[0] = '|';
        DumpStructure( p_object, 0, psz_foo );

        vlc_mutex_unlock( &structure_lock );

        if( *newval.psz_string )
        {
            vlc_object_release( p_this );
        }
    }
    else if( *psz_cmd == 'l' )
    {
        vlc_object_t **pp_current, **pp_end;

        vlc_mutex_lock( &structure_lock );

        pp_current = p_this->p_libvlc->pp_objects;
        pp_end = pp_current + p_this->p_libvlc->i_objects;

        for( ; pp_current < pp_end ; pp_current++ )
        {
            if( (*pp_current)->b_attached )
            {
                PrintObject( *pp_current, "" );
            }
            else
            {
                printf( " o %.8i %s (not attached)\n",
                        (*pp_current)->i_object_id,
                        (*pp_current)->psz_object_type );
            }
        }

        vlc_mutex_unlock( &structure_lock );
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
        vlc_mutex_lock( &structure_lock );

        p_list->p_values[i_index].p_object->i_refcount--;

        vlc_mutex_unlock( &structure_lock );
    }

    free( p_list->p_values );
    free( p_list );
}

/* Following functions are local */

/*****************************************************************************
 * FindIndex: find the index of an object in an array of objects
 *****************************************************************************
 * This function assumes that p_this can be found in pp_objects. It will not
 * crash if p_this cannot be found, but will return a wrong value. It is your
 * duty to check the return value if you are not certain that the object could
 * be found for sure.
 *****************************************************************************/
static int FindIndex( vlc_object_t *p_this,
                      vlc_object_t **pp_objects, int i_count )
{
    int i_middle = i_count / 2;

    if( i_count == 0 )
    {
        return 0;
    }

    if( pp_objects[i_middle] == p_this )
    {
        return i_middle;
    }

    if( i_count == 1 )
    {
        return 0;
    }

    /* We take advantage of the sorted array */
    if( pp_objects[i_middle]->i_object_id < p_this->i_object_id )
    {
        return i_middle + FindIndex( p_this, pp_objects + i_middle,
                                             i_count - i_middle );
    }
    else
    {
        return FindIndex( p_this, pp_objects, i_middle );
    }
}

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
                p_tmp->i_refcount++;
                return p_tmp;
            }
            else
            {
                return FindObject( p_tmp, i_type, i_mode );
            }
        }
        break;

    case FIND_CHILD:
        for( i = p_this->i_children; i--; )
        {
            p_tmp = p_this->pp_children[i];
            if( p_tmp->i_object_type == i_type )
            {
                p_tmp->i_refcount++;
                return p_tmp;
            }
            else if( p_tmp->i_children )
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

static void DetachObject( vlc_object_t *p_this )
{
    vlc_object_t *p_parent = p_this->p_parent;
    int i_index, i;

    /* Remove p_this's parent */
    p_this->p_parent = NULL;

    /* Remove all of p_parent's children which are p_this */
    for( i_index = p_parent->i_children ; i_index-- ; )
    {
        if( p_parent->pp_children[i_index] == p_this )
        {
            p_parent->i_children--;
            for( i = i_index ; i < p_parent->i_children ; i++ )
            {
                p_parent->pp_children[i] = p_parent->pp_children[i+1];
            }
        }
    }

    if( p_parent->i_children )
    {
        p_parent->pp_children = (vlc_object_t **)realloc( p_parent->pp_children,
                               p_parent->i_children * sizeof(vlc_object_t *) );
    }
    else
    {
        free( p_parent->pp_children );
        p_parent->pp_children = NULL;
    }
}

/*****************************************************************************
 * SetAttachment: recursively set the b_attached flag of a subtree.
 *****************************************************************************
 * This function is used by the attach and detach functions to propagate
 * the b_attached flag in a subtree.
 *****************************************************************************/
static void SetAttachment( vlc_object_t *p_this, vlc_bool_t b_attached )
{
    int i_index;

    for( i_index = p_this->i_children ; i_index-- ; )
    {
        SetAttachment( p_this->pp_children[i_index], b_attached );
    }

    p_this->b_attached = b_attached;
}

static void PrintObject( vlc_object_t *p_this, const char *psz_prefix )
{
    char psz_children[20], psz_refcount[20], psz_thread[20], psz_name[50];

    psz_name[0] = '\0';
    if( p_this->psz_object_name )
    {
        snprintf( psz_name, 50, " \"%s\"", p_this->psz_object_name );
        psz_name[48] = '\"';
        psz_name[49] = '\0';
    }

    psz_children[0] = '\0';
    switch( p_this->i_children )
    {
        case 0:
            break;
        case 1:
            strcpy( psz_children, ", 1 child" );
            break;
        default:
            snprintf( psz_children, 20,
                      ", %i children", p_this->i_children );
            psz_children[19] = '\0';
            break;
    }

    psz_refcount[0] = '\0';
    if( p_this->i_refcount )
    {
        snprintf( psz_refcount, 20, ", refcount %i", p_this->i_refcount );
        psz_refcount[19] = '\0';
    }

    psz_thread[0] = '\0';
    if( p_this->b_thread )
    {
        snprintf( psz_thread, 20, " (thread %d)", (int)p_this->thread_id );
        psz_thread[19] = '\0';
    }

    printf( " %so %.8i %s%s%s%s%s\n", psz_prefix,
            p_this->i_object_id, p_this->psz_object_type,
            psz_name, psz_thread, psz_refcount, psz_children );
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

    for( i = 0 ; i < p_this->i_children ; i++ )
    {
        if( i_level )
        {
            psz_foo[i_level-1] = ' ';

            if( psz_foo[i_level-2] == '`' )
            {
                psz_foo[i_level-2] = ' ';
            }
        }

        if( i == p_this->i_children - 1 )
        {
            psz_foo[i_level] = '`';
        }
        else
        {
            psz_foo[i_level] = '|';
        }

        psz_foo[i_level+1] = '-';
        psz_foo[i_level+2] = '\0';

        DumpStructure( p_this->pp_children[i], i_level + 2, psz_foo );
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

    p_object->i_refcount++;

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

    p_object->i_refcount++;

    p_list->p_values[p_list->i_count].p_object = p_object;
    p_list->i_count++;

    return;
}*/

static int CountChildren( vlc_object_t *p_this, int i_type )
{
    vlc_object_t *p_tmp;
    int i, i_count = 0;

    for( i = 0; i < p_this->i_children; i++ )
    {
        p_tmp = p_this->pp_children[i];

        if( p_tmp->i_object_type == i_type )
        {
            i_count++;
        }

        if( p_tmp->i_children )
        {
            i_count += CountChildren( p_tmp, i_type );
        }
    }

    return i_count;
}

static void ListChildren( vlc_list_t *p_list, vlc_object_t *p_this, int i_type )
{
    vlc_object_t *p_tmp;
    int i;

    for( i = 0; i < p_this->i_children; i++ )
    {
        p_tmp = p_this->pp_children[i];

        if( p_tmp->i_object_type == i_type )
        {
            ListReplace( p_list, p_tmp, p_list->i_count++ );
        }

        if( p_tmp->i_children )
        {
            ListChildren( p_list, p_tmp, i_type );
        }
    }
}
