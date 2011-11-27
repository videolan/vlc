/*****************************************************************************
 * media_library.c: SQL-based media library: ML creators and destructors
 *****************************************************************************
 * Copyright (C) 2009-2010 VLC authors and VideoLAN and AUTHORS
 * $Id$
 *
 * Authors: Srikanth Raju <srikiraju at gmail dot com>
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
#include "config.h"
#endif

#if defined(MEDIA_LIBRARY)

#include <assert.h>
#include <vlc_media_library.h>
#include <vlc_modules.h>
#include "../libvlc.h"

/**
 * @brief Destroy the medialibrary object
 * @param Parent object that holds the media library object
 */
void ml_Destroy( vlc_object_t * p_this )
{
    media_library_t* p_ml = ( media_library_t* )p_this;
    module_unneed( p_ml, p_ml->p_module );
}


/**
 * Atomically set the reference count to 1.
 * @param p_gc reference counted object
 * @param pf_destruct destruction calback
 * @return p_gc.
 */
static void *ml_gc_init (ml_gc_object_t *p_gc, void (*pf_destruct) (ml_gc_object_t *))
{
    /* There is no point in using the GC if there is no destructor... */
    assert (pf_destruct);
    p_gc->pf_destructor = pf_destruct;

    p_gc->pool = false;
    p_gc->refs = 1;
    /* Nobody else can possibly lock the spin - it's there as a barrier */
    vlc_spin_init (&p_gc->spin);
    vlc_spin_lock (&p_gc->spin);
    vlc_spin_unlock (&p_gc->spin);
    return p_gc;
}



/**
 * @brief Create an instance of the media library
 * @param p_this Parent object
 * @param psz_name Name which is passed to module_need (not needed)
 * @return p_ml created and attached, module loaded. NULL if
 * not able to load
 */
media_library_t *ml_Create( vlc_object_t *p_this, char *psz_name )
{
    media_library_t *p_ml;

    p_ml = ( media_library_t * ) vlc_custom_create(
                                p_this, sizeof( media_library_t ),
                                "media-library" );
    if( !p_ml )
    {
        msg_Err( p_this, "unable to create media library object" );
        return NULL;
    }

    p_ml->p_module = module_need( p_ml, "media-library", psz_name, false );
    if( !p_ml->p_module )
    {
        vlc_object_release( p_ml );
        msg_Err( p_this, "Media Library provider not found" );
        return NULL;
    }

    return p_ml;
}

#undef ml_Get
/**
 * @brief Acquire a reference to the media library singleton
 * @param p_this Object that holds the reference
 * @return media_library_t The ml object. NULL if not compiled with
 * media library or if unable to load
 */
media_library_t* ml_Get( vlc_object_t* p_this )
{
    media_library_t* p_ml;
    vlc_mutex_lock( &( libvlc_priv( p_this->p_libvlc )->ml_lock ) );
    p_ml = libvlc_priv (p_this->p_libvlc)->p_ml;
    assert( VLC_OBJECT( p_ml ) != p_this );
    if( p_ml == NULL &&
        !var_GetBool( p_this->p_libvlc, "load-media-library-on-startup" ) )
    {
        libvlc_priv (p_this->p_libvlc)->p_ml
            = ml_Create( VLC_OBJECT( p_this->p_libvlc ), NULL );
        p_ml = libvlc_priv (p_this->p_libvlc)->p_ml;
    }
    vlc_mutex_unlock( &( libvlc_priv( p_this->p_libvlc )->ml_lock ) );
    return p_ml;
}

/**
 * @brief Destructor for ml_media_t
 */
static void media_Destroy( ml_gc_object_t *p_gc )
{
    ml_media_t* p_media = ml_priv( p_gc, ml_media_t );
    vlc_mutex_destroy( &p_media->lock );
    ml_FreeMediaContent( p_media );
    free( p_media );
}

/**
 * @brief Object constructor for ml_media_t
 * @param p_ml The media library object
 * @param id If 0, this item isn't in database. If non zero, it is and
 * it will be a singleton
 * @param select Type of object
 * @param reload Whether to reload from database
 */
ml_media_t* media_New( media_library_t* p_ml, int id,
        ml_select_e select, bool reload )
{
    if( id == 0 )
    {
        ml_media_t* p_media = NULL;
        p_media = ( ml_media_t* )calloc( 1, sizeof( ml_media_t ) );
        ml_gc_init( &p_media->ml_gc_data, media_Destroy );
        vlc_mutex_init( &p_media->lock );
        return p_media;
    }
    else
        return p_ml->functions.pf_GetMedia( p_ml, id, select, reload );
}

#undef ml_UpdateSimple
/**
 * @brief Update a given table
 * @param p_media_library The media library object
 * @param selected_type The table to update
 * @param psz_lvalue The role of the person if selected_type = ML_PEOPLE
 * @param id The id of the row to update
 * @param ... The update data. [SelectType [RoleType] Value] ... ML_END
 */
int ml_UpdateSimple( media_library_t *p_media_library,
                                     ml_select_e selected_type,
                                     const char* psz_lvalue,
                                     int id, ... )
{
    ml_element_t *update;
    vlc_array_t *array = vlc_array_new();
    int i_ret = VLC_SUCCESS;

    va_list args;
    va_start( args, id );

    ml_select_e sel;
    do {
        update = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) );
        sel = ( ml_select_e ) va_arg( args, int );
        update->criteria = sel;
        if( sel == ML_PEOPLE )
        {
            update->lvalue.str = va_arg( args, char* );
            update->value.str = va_arg( args, char* );
            vlc_array_append( array, update );
        }
        else if( sel == ML_PEOPLE_ID )
        {
           update->lvalue.str = va_arg( args, char* );
           update->value.i = va_arg( args, int );
           vlc_array_append( array, update );
        }
        else if( sel == ML_PEOPLE_ROLE )
        {
#ifndef NDEBUG
            msg_Dbg( p_media_library,
                     "this argument is invalid for Update: %d",
                     (int)sel );
#endif
        }
        else
        {
            switch( ml_AttributeIsString( sel ) )
            {
                case -1:
                    if( sel != ML_END )
                    {
#ifndef NDEBUG
                        msg_Dbg( p_media_library,
                                 "this argument is invalid for Update: %d",
                                 (int)sel );
#endif
                        i_ret = VLC_EGENERIC;
                    }
                    else if( sel == ML_END )
                        vlc_array_append( array, update );
                    break;
                case 0:
                    update->value.str = va_arg( args, char* );
                    vlc_array_append( array, update );
                    break;
                case 1:
                    update->value.i = va_arg( args, int );
                    vlc_array_append( array, update );
                    break;
            }
        }
    } while( sel != ML_END );

    va_end( args );

    ml_ftree_t* p_where = NULL;
    ml_ftree_t* find = ( ml_ftree_t* ) calloc( 1, sizeof( ml_ftree_t ) );
    find->criteria = ML_ID;
    find->value.i = id ;
    find->comp = ML_COMP_EQUAL;
    p_where = ml_FtreeFastAnd( p_where, find );

    /* Let's update the database ! */
    if( i_ret == VLC_SUCCESS )
        i_ret = ml_Update( p_media_library, selected_type, psz_lvalue,
                            p_where, array );

    /* Destroying array */
    for( int i = 0; i < vlc_array_count( array ); i++ )
    {
        free( vlc_array_item_at_index( array, i ) );
    }
    vlc_array_destroy( array );
    ml_FreeFindTree( p_where );

    return i_ret;
}

/**
 * @brief Connect up a find tree
 * @param op operator to connect with
 * If op = ML_OP_NONE, then you are connecting to a tree consisting of
 * only SPECIAL nodes.
 * If op = ML_OP_NOT, then right MUST be NULL
 * op must not be ML_OP_SPECIAL, @see ml_FtreeSpec
 * @param left part of the tree
 * @param right part of the tree
 * @return Pointer to new tree
 * @note Use the helpers!
 */
ml_ftree_t* ml_OpConnectChilds( ml_op_e op, ml_ftree_t* left,
        ml_ftree_t* right )
{
    /* Use this Op for fresh trees (with only special nodes/none at all!) */
    if( op == ML_OP_NONE )
    {
        assert( ml_FtreeHasOp( left ) == 0 );
        if( left == NULL )
            return right;
        /* Percolate down tree only for special nodes */
        assert( left->op == ML_OP_SPECIAL );
        if( left->left == NULL )
        {
            left->left = right;
            return left;
        }
        else
        {
            return ml_OpConnectChilds( ML_OP_NONE, left->left, right );
        }
    }
    else if( op == ML_OP_NOT )
    {
        assert( right == NULL && left != NULL );
        assert( ml_FtreeHasOp( left ) > 0 );
    }
    else if( op == ML_OP_SPECIAL )
    {
        assert( 0 );
    }
    else
    {
        assert( right != NULL && left != NULL );
        assert( ml_FtreeHasOp( left ) > 0 );
        assert( ml_FtreeHasOp( right ) > 0 );
    }
    ml_ftree_t* p_parent = (ml_ftree_t *) calloc( 1, sizeof( ml_ftree_t ) );
    p_parent->op = op;
    p_parent->left = left;
    p_parent->right = right;
    return p_parent;
}

#undef ml_FtreeSpec
/**
 * @brief Attaches a special node to a tree
 * @param tree Tree to attach special node to
 * @param crit Criteria may be SORT_ASC, SORT_DESC, LIMIT or DISTINCT
 * @param limit Limit used if LIMIT criteria used
 * @param Sort string used if SORT criteria is used
 * @return Pointer to new tree
 * @note Use the helpers
 */
ml_ftree_t* ml_FtreeSpec( ml_ftree_t* tree,
                                          ml_select_e crit,
                                          int limit,
                                          char* sort )
{
    assert( crit == ML_SORT_ASC || crit == ML_LIMIT || crit == ML_SORT_DESC ||
            crit == ML_DISTINCT );
    ml_ftree_t* right = ( ml_ftree_t* ) calloc( 1, sizeof( ml_ftree_t ) );
    right->criteria = crit;
    if( crit == ML_LIMIT )
        right->value.i = limit;
    else if( crit == ML_SORT_ASC || crit == ML_SORT_DESC )
        right->value.str = strdup( sort );
    right->op = ML_OP_NONE;
    ml_ftree_t* p_parent = ( ml_ftree_t* ) calloc( 1, sizeof( ml_ftree_t ) );
    p_parent->right = right;
    p_parent->op = ML_OP_SPECIAL;
    p_parent->left = tree;
    return p_parent;
}


/**
 * @brief Creates and adds the playlist based on a given find tree
 * @param p_ml Media library object
 * @param p_tree Find tree to create SELECT
 */
void ml_PlaySmartPlaylistBasedOn( media_library_t* p_ml,
                                                ml_ftree_t* p_tree )
{
    assert( p_tree );
    vlc_array_t* p_results = vlc_array_new();
    ml_FindAdv( p_ml, p_results, ML_ID, NULL, p_tree );
    playlist_t* p_pl = pl_Get( p_ml );
    playlist_Lock( p_pl );
    playlist_Clear( p_pl, true );
    for( int i = 0; i < vlc_array_count( p_results ); i++ )
    {
        ml_result_t* p_res = ( ml_result_t* ) vlc_array_item_at_index( p_results, i );
        input_item_t* p_item;
        if( p_res )
        {
            p_item = ml_CreateInputItem( p_ml, p_res->value.i );
            playlist_AddInput( p_pl, p_item, PLAYLIST_APPEND,
                           PLAYLIST_END, true, true );
        }
    }
    playlist_Unlock( p_pl );
    ml_DestroyResultArray( p_results );
    vlc_array_destroy( p_results );
}

/**
 * @brief Returns a person list of given type
 * @param p_ml The ML object
 * @param p_media The Media object
 * @param i_type The person type
 * @note This function is thread safe
 */
ml_person_t*  ml_GetPersonsFromMedia( media_library_t* p_ml,
                                                    ml_media_t* p_media,
                                                    const char *psz_role )
{
    VLC_UNUSED( p_ml );
    assert( p_media != NULL );
    ml_person_t* p_return = NULL;
    ml_LockMedia( p_media );
    ml_person_t* p_person = p_media->p_people;
    while( p_person )
    {
        if( strcmp( p_person->psz_role, psz_role ) == 0 )
        {
            int i_ret = ml_CreateAppendPerson( &p_return, p_person );
            if( i_ret != VLC_SUCCESS )
            {
                ml_UnlockMedia( p_media );
                ml_FreePeople( p_return );
                return NULL;
            }
        }
        p_person = p_person->p_next;
    }
    ml_UnlockMedia( p_media );
    //TODO: Fill up empty names + clean up list
    return p_return;
}

/**
 * @brief Delete a certain type of people from a media
 * @param p_media Media to delete from
 * @param i_type Type of person to delete
 * @note This function is threadsafe
 */
void ml_DeletePersonTypeFromMedia( ml_media_t* p_media, const char *psz_role )
{
    assert( p_media );
    ml_LockMedia( p_media );
    ml_person_t* p_prev = NULL;
    ml_person_t* p_person = p_media->p_people;

    while( p_person )
    {
        if( strcmp( p_person->psz_role, psz_role ) == 0 )
        {
            if( p_prev == NULL )
            {
                p_media->p_people = p_person->p_next;
                p_person->p_next = NULL;
                ml_FreePeople( p_person );
                p_person = p_media->p_people;
            }
            else
            {
                p_prev->p_next = p_person->p_next;
                p_person->p_next = NULL;
                ml_FreePeople( p_person );
                p_person = p_prev->p_next;
            }
        }
        else
        {
            p_prev = p_person;
            p_person = p_person->p_next;
        }
    }
    ml_UnlockMedia( p_media );
}

#endif /* MEDIA_LIBRARY */
