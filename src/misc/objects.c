/*****************************************************************************
 * objects.c: vlc_object_t handling
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: objects.c,v 1.1 2002/06/01 12:32:01 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <vlc/vlc.h>

#ifdef HAVE_STDLIB_H
#   include <stdlib.h>                                          /* realloc() */
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "vdec_ext-plugins.h"

#include "audio_output.h"

#include "playlist.h"
#include "interface.h"

static void vlc_dumpstructure_inner( vlc_object_t *, int, char * );
static vlc_object_t * vlc_object_find_inner( vlc_object_t *, int, int );
static void vlc_object_unlink_inner( vlc_object_t *, vlc_object_t * );

#define MAX_TREE_DEPTH 100

void __vlc_dumpstructure( vlc_object_t *p_this )
{
    char psz_foo[2 * MAX_TREE_DEPTH + 1];

    vlc_mutex_lock( &p_this->p_vlc->structure_lock );
    psz_foo[0] = '|';
    vlc_dumpstructure_inner( p_this, 0, psz_foo );
    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}

static void vlc_dumpstructure_inner( vlc_object_t *p_this,
                                     int i_level, char *psz_foo )
{
    int i;
    char i_back = psz_foo[i_level];
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
            snprintf( psz_children, 20, ", %i children", p_this->i_children );
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
        snprintf( psz_thread, 20, " (thread %d)", p_this->thread_id );
        psz_thread[19] = '\0';
    }

    psz_foo[i_level] = '\0';
    msg_Dbg( p_this, "%so %s %p%s%s%s%s", psz_foo, p_this->psz_object_type,
             p_this, psz_name, psz_thread, psz_refcount, psz_children );
    psz_foo[i_level] = i_back;

    if( i_level / 2 >= MAX_TREE_DEPTH )
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

        vlc_dumpstructure_inner( p_this->pp_children[i], i_level + 2, psz_foo );
    }
}

/* vlc_object_create: initialize a vlc object and set its parent */
void * __vlc_object_create( vlc_object_t *p_this, int i_type )
{
    vlc_object_t * p_new;
    char *         psz_type;
    size_t         i_size;

    switch( i_type )
    {
        case VLC_OBJECT_ROOT:
            i_size = sizeof(vlc_t);
            psz_type = "root";
            break;
        case VLC_OBJECT_MODULE:
            i_size = sizeof(module_t);
            psz_type = "module";
            break;
        case VLC_OBJECT_INTF:
            i_size = sizeof(intf_thread_t);
            psz_type = "interface";
            break;
        case VLC_OBJECT_PLAYLIST:
            i_size = sizeof(playlist_t);
            psz_type = "playlist";
            break;
        case VLC_OBJECT_INPUT:
            i_size = sizeof(input_thread_t);
            psz_type = "input";
            break;
        case VLC_OBJECT_DECODER:
            i_size = sizeof(decoder_fifo_t);
            psz_type = "decoder";
            break;
        case VLC_OBJECT_VOUT:
            i_size = sizeof(vout_thread_t);
            psz_type = "video output";
            break;
        case VLC_OBJECT_AOUT:
            i_size = sizeof(aout_thread_t);
            psz_type = "audio output";
            break;
        default:
            i_size = i_type;
            i_type = VLC_OBJECT_PRIVATE;
            psz_type = "private";
            break;
    }

    p_new = malloc( i_size );

    if( !p_new )
    {
        return NULL;
    }

    memset( p_new, 0, i_size );

    p_new->i_object_type = i_type;
    p_new->psz_object_type = psz_type;

    p_new->psz_object_name = NULL;

    p_new->i_refcount = 0;
    p_new->b_die = 0;
    p_new->b_error = 0;

    p_new->p_this = p_new;

    /* If i_type is root, then p_new is our own p_vlc */
    if( i_type == VLC_OBJECT_ROOT )
    {
        p_new->p_vlc = (vlc_t*)p_new;
        p_new->p_vlc->i_counter = 0;
        p_new->i_object_id = 0;
    }
    else
    {
        p_new->p_vlc = p_this->p_vlc;

        vlc_mutex_lock( &p_this->p_vlc->structure_lock );
        p_new->p_vlc->i_counter++;
        p_new->i_object_id = p_new->p_vlc->i_counter;
        vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
    }

    p_new->pp_parents = NULL;
    p_new->i_parents = 0;
    p_new->pp_children = NULL;
    p_new->i_children = 0;

    //msg_Dbg( p_new, "created object" );

    return p_new;
}

/* vlc_object_destroy: initialize a vlc object and set its parent */
void __vlc_object_destroy( vlc_object_t *p_this )
{
    if( p_this->i_refcount )
    {
        msg_Err( p_this, "refcount is %i", p_this->i_refcount );
        vlc_dumpstructure( p_this );
    }

    if( p_this->i_children )
    {
        msg_Err( p_this, "object still has children" );
        vlc_dumpstructure( p_this );
    }

    if( p_this->i_parents )
    {
        msg_Err( p_this, "object still has parents" );
        vlc_dumpstructure( p_this );
    }

    //msg_Dbg( p_this, "destroyed object" );

    free( p_this );
}

/* vlc_object_find: find a typed object and increment its refcount */
void * __vlc_object_find( vlc_object_t *p_this, int i_type, int i_mode )
{
    vlc_object_t *p_found;

    vlc_mutex_lock( &p_this->p_vlc->structure_lock );

    /* If we are of the requested type ourselves, don't look further */
    if( !(i_mode & FIND_STRICT) && p_this->i_object_type == i_type )
    {
        p_this->i_refcount++;
        vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
        return p_this;
    }

    /* Otherwise, recursively look for the object */
    p_found = vlc_object_find_inner( p_this, i_type, i_mode );

    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );

    return p_found;
}

static vlc_object_t * vlc_object_find_inner( vlc_object_t *p_this,
                                             int i_type, int i_mode )
{
    int i;
    vlc_object_t *p_tmp;

    switch( i_mode & 0x000f )
    {
    case FIND_PARENT:
        for( i = p_this->i_parents; i--; )
        {
            p_tmp = p_this->pp_parents[i];
            if( p_tmp->i_object_type == i_type )
            {
                p_tmp->i_refcount++;
                return p_tmp;
            }
            else if( p_tmp->i_parents )
            {
                p_tmp = vlc_object_find_inner( p_tmp, i_type, i_mode );
                if( p_tmp )
                {
                    return p_tmp;
                }
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
                p_tmp = vlc_object_find_inner( p_tmp, i_type, i_mode );
                if( p_tmp )
                {
                    return p_tmp;
                }
            }
        }
        break;

    case FIND_ANYWHERE:
        /* FIXME: unimplemented */
        break;
    }

    return NULL;
}

/* vlc_object_yield: increment an object refcount */
void __vlc_object_yield( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->p_vlc->structure_lock );
    p_this->i_refcount++;
    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}

/* vlc_object_release: decrement an object refcount */
void __vlc_object_release( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->p_vlc->structure_lock );
    p_this->i_refcount--;
    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}

/* vlc_object_unlink: detach object from its parents */
void __vlc_object_unlink_all( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->p_vlc->structure_lock );

    /* FIXME: BORK ! BORK ! BORK !!! THIS STUFF IS BORKED !! FIXME */
    while( p_this->i_parents )
    {
        /* Not very effective because we know the index, but we'd have to
         * parse p_parent->pp_children anyway. Plus, we remove duplicates
         * by not using the object's index */
        vlc_object_unlink_inner( p_this, p_this->pp_parents[0] );
    }

    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}

/* vlc_object_unlink: remove a parent/child link */
void __vlc_object_unlink( vlc_object_t *p_this, vlc_object_t *p_parent )
{
    vlc_mutex_lock( &p_this->p_vlc->structure_lock );
    vlc_object_unlink_inner( p_this, p_parent );
    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}

static void vlc_object_unlink_inner( vlc_object_t *p_this,
                                     vlc_object_t *p_parent )
{
    int i_index, i;

    /* Remove all of p_this's parents which are p_parent */
    for( i_index = p_this->i_parents ; i_index-- ; )
    {
        if( p_this->pp_parents[i_index] == p_parent )
        {
            p_this->i_parents--;
            for( i = i_index ; i < p_this->i_parents ; i++ )
            {
                p_this->pp_parents[i] = p_this->pp_parents[i+1];
            }
        }
    }

    if( p_this->i_parents )
    {
        p_this->pp_parents = (vlc_object_t **)realloc( p_this->pp_parents,
                                p_this->i_parents * sizeof(vlc_object_t *) );
    }
    else
    {
        free( p_this->pp_parents );
        p_this->pp_parents = NULL;
    }

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

/* vlc_object_attach: attach object to a parent object */
void __vlc_object_attach( vlc_object_t *p_this, vlc_object_t *p_parent )
{
    vlc_mutex_lock( &p_this->p_vlc->structure_lock );

    p_this->i_parents++;
    p_this->pp_parents = (vlc_object_t **)realloc( p_this->pp_parents,
                            p_this->i_parents * sizeof(vlc_object_t *) );
    p_this->pp_parents[p_this->i_parents - 1] = p_parent;

    p_parent->i_children++;
    p_parent->pp_children = (vlc_object_t **)realloc( p_parent->pp_children,
                               p_parent->i_children * sizeof(vlc_object_t *) );
    p_parent->pp_children[p_parent->i_children - 1] = p_this;

    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}

#if 0 /* UNUSED */
/* vlc_object_setchild: attach a child object */
void __vlc_object_setchild( vlc_object_t *p_this, vlc_object_t *p_child )
{
    vlc_mutex_lock( &p_this->p_vlc->structure_lock );

    p_this->i_children++;
    p_this->pp_children = (vlc_object_t **)realloc( p_this->pp_children,
                             p_this->i_children * sizeof(vlc_object_t *) );
    p_this->pp_children[p_this->i_children - 1] = p_child;

    p_child->i_parents++;
    p_child->pp_parents = (vlc_object_t **)realloc( p_child->pp_parents,
                             p_child->i_parents * sizeof(vlc_object_t *) );
    p_child->pp_parents[p_child->i_parents - 1] = p_this;

    vlc_mutex_unlock( &p_this->p_vlc->structure_lock );
}
#endif

