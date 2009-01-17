/*****************************************************************************
 * item.c: input_item management
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
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
#include <assert.h>

#include <vlc_common.h>
#include "vlc_playlist.h"
#include "vlc_interface.h"

#include "item.h"

static void GuessType( input_item_t *p_item );

/** Stuff moved out of vlc_input.h -- FIXME: should probably not be inline
 * anyway. */
static inline void input_item_Init( vlc_object_t *p_o, input_item_t *p_i )
{
    memset( p_i, 0, sizeof(input_item_t) );

    p_i->psz_name = NULL;
    p_i->psz_uri = NULL;
    TAB_INIT( p_i->i_es, p_i->es );
    TAB_INIT( p_i->i_options, p_i->ppsz_options );
    p_i->optflagv = NULL, p_i->optflagc = 0;
    TAB_INIT( p_i->i_categories, p_i->pp_categories );

    p_i->i_type = ITEM_TYPE_UNKNOWN;
    p_i->b_fixed_name = true;

    p_i->p_stats = NULL;
    p_i->p_meta = NULL;

    vlc_mutex_init( &p_i->lock );
    vlc_event_manager_t * p_em = &p_i->event_manager;
    vlc_event_manager_init( p_em, p_i, p_o );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemMetaChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemSubItemAdded );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemDurationChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemPreparsedChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemNameChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemInfoChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemErrorWhenReadingChanged );
}

static inline void input_item_Clean( input_item_t *p_i )
{
    int i;

    vlc_event_manager_fini( &p_i->event_manager );

    free( p_i->psz_name );
    free( p_i->psz_uri );
    if( p_i->p_stats )
    {
        vlc_mutex_destroy( &p_i->p_stats->lock );
        free( p_i->p_stats );
    }

    if( p_i->p_meta )
        vlc_meta_Delete( p_i->p_meta );

    for( i = 0; i < p_i->i_options; i++ )
        free( p_i->ppsz_options[i] );
    TAB_CLEAN( p_i->i_options, p_i->ppsz_options );
    free( p_i->optflagv);

    for( i = 0; i < p_i->i_es; i++ )
    {
        es_format_Clean( p_i->es[i] );
        free( p_i->es[i] );
    }
    TAB_CLEAN( p_i->i_es, p_i->es );

    for( i = 0; i < p_i->i_categories; i++ )
    {
        info_category_t *p_category = p_i->pp_categories[i];
        int j;

        for( j = 0; j < p_category->i_infos; j++ )
        {
            struct info_t *p_info = p_category->pp_infos[j];

            free( p_info->psz_name);
            free( p_info->psz_value );
            free( p_info );
        }
        TAB_CLEAN( p_category->i_infos, p_category->pp_infos );

        free( p_category->psz_name );
        free( p_category );
    }
    TAB_CLEAN( p_i->i_categories, p_i->pp_categories );

    vlc_mutex_destroy( &p_i->lock );
}
void input_item_SetErrorWhenReading( input_item_t *p_i, bool b_error )
{
    bool b_changed;

    vlc_mutex_lock( &p_i->lock );

    b_changed = p_i->b_error_when_reading != b_error;
    p_i->b_error_when_reading = b_error;

    vlc_mutex_unlock( &p_i->lock );

    if( b_changed )
    {
        vlc_event_t event;

        event.type = vlc_InputItemErrorWhenReadingChanged;
        event.u.input_item_error_when_reading_changed.new_value = b_error;
        vlc_event_send( &p_i->event_manager, &event );
    }
}
void input_item_SetPreparsed( input_item_t *p_i, bool b_preparsed )
{
    bool b_send_event = false;

    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    int i_new_status;
    if( b_preparsed )
        i_new_status = p_i->p_meta->i_status | ITEM_PREPARSED;
    else
        i_new_status = p_i->p_meta->i_status & ~ITEM_PREPARSED;
    if( p_i->p_meta->i_status != i_new_status )
    {
        p_i->p_meta->i_status = i_new_status;
        b_send_event = true;
    }

    vlc_mutex_unlock( &p_i->lock );

    if( b_send_event )
    {
        vlc_event_t event;
        event.type = vlc_InputItemPreparsedChanged;
        event.u.input_item_preparsed_changed.new_status = i_new_status;
        vlc_event_send( &p_i->event_manager, &event );
    }
}
void input_item_SetArtNotFound( input_item_t *p_i, bool b_not_found )
{
    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    if( b_not_found )
        p_i->p_meta->i_status |= ITEM_ART_NOTFOUND;
    else
        p_i->p_meta->i_status &= ~ITEM_ART_NOTFOUND;

    vlc_mutex_unlock( &p_i->lock );
}
void input_item_SetArtFetched( input_item_t *p_i, bool b_art_fetched )
{
    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    if( b_art_fetched )
        p_i->p_meta->i_status |= ITEM_ART_FETCHED;
    else
        p_i->p_meta->i_status &= ~ITEM_ART_FETCHED;

    vlc_mutex_unlock( &p_i->lock );
}

void input_item_SetMeta( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz_val )
{
    vlc_event_t event;

    vlc_mutex_lock( &p_i->lock );
    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();
    vlc_meta_Set( p_i->p_meta, meta_type, psz_val );
    vlc_mutex_unlock( &p_i->lock );

    /* Notify interested third parties */
    event.type = vlc_InputItemMetaChanged;
    event.u.input_item_meta_changed.meta_type = meta_type;
    vlc_event_send( &p_i->event_manager, &event );
}

/* FIXME GRRRRRRRRRR args should be in the reverse order to be 
 * consistant with (nearly?) all or copy funcs */
void input_item_CopyOptions( input_item_t *p_parent,
                             input_item_t *p_child )
{
    vlc_mutex_lock( &p_parent->lock );

    for( int i = 0 ; i< p_parent->i_options; i++ )
    {
        if( !strcmp( p_parent->ppsz_options[i], "meta-file" ) )
            continue;

        input_item_AddOption( p_child,
                              p_parent->ppsz_options[i],
                              p_parent->optflagv[i] );
    }

    vlc_mutex_unlock( &p_parent->lock );
}

/* This won't hold the item, but can tell to interested third parties
 * Like the playlist, that there is a new sub item. With this design
 * It is not the input item's responsability to keep all the ref of
 * the input item children. */
void input_item_AddSubItem( input_item_t *p_parent, input_item_t *p_child )
{
    vlc_mutex_lock( &p_parent->lock );

    p_parent->i_type = ITEM_TYPE_PLAYLIST;

    vlc_mutex_unlock( &p_parent->lock );

    /* Notify interested third parties */
    vlc_event_t event;

    event.type = vlc_InputItemSubItemAdded;
    event.u.input_item_subitem_added.p_new_child = p_child;
    vlc_event_send( &p_parent->event_manager, &event );
}

bool input_item_HasErrorWhenReading( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );

    bool b_error = p_item->b_error_when_reading;

    vlc_mutex_unlock( &p_item->lock );

    return b_error;
}

bool input_item_MetaMatch( input_item_t *p_i,
                           vlc_meta_type_t meta_type, const char *psz )
{
    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
    {
        vlc_mutex_unlock( &p_i->lock );
        return false;
    }
    const char *psz_meta = vlc_meta_Get( p_i->p_meta, meta_type );
    bool b_ret = psz_meta && strcasestr( psz_meta, psz );

    vlc_mutex_unlock( &p_i->lock );

    return b_ret;
}

char *input_item_GetMeta( input_item_t *p_i, vlc_meta_type_t meta_type )
{
    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
    {
        vlc_mutex_unlock( &p_i->lock );
        return NULL;
    }

    char *psz = NULL;
    if( vlc_meta_Get( p_i->p_meta, meta_type ) )
        psz = strdup( vlc_meta_Get( p_i->p_meta, meta_type ) );

    vlc_mutex_unlock( &p_i->lock );
    return psz;
}

char *input_item_GetName( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );

    char *psz_name = p_item->psz_name ? strdup( p_item->psz_name ) : NULL;

    vlc_mutex_unlock( &p_item->lock );
    return psz_name;
}
void input_item_SetName( input_item_t *p_item, const char *psz_name )
{
    vlc_mutex_lock( &p_item->lock );

    free( p_item->psz_name );
    p_item->psz_name = strdup( psz_name );

    vlc_mutex_unlock( &p_item->lock );
}

char *input_item_GetURI( input_item_t *p_i )
{
    vlc_mutex_lock( &p_i->lock );

    char *psz_s = p_i->psz_uri ? strdup( p_i->psz_uri ) : NULL;

    vlc_mutex_unlock( &p_i->lock );
    return psz_s;
}
void input_item_SetURI( input_item_t *p_i, const char *psz_uri )
{
    vlc_mutex_lock( &p_i->lock );

    free( p_i->psz_uri );
    p_i->psz_uri = strdup( psz_uri );

    GuessType( p_i );

    if( !p_i->psz_name && p_i->i_type == ITEM_TYPE_FILE )
    {
        const char *psz_filename = strrchr( p_i->psz_uri, DIR_SEP_CHAR );
        if( psz_filename && *psz_filename == DIR_SEP_CHAR )
            psz_filename++;
        if( psz_filename && *psz_filename )
            p_i->psz_name = strdup( psz_filename );
    }

    if( !p_i->psz_name )
        p_i->psz_name = strdup( p_i->psz_uri );

    vlc_mutex_unlock( &p_i->lock );
}

mtime_t input_item_GetDuration( input_item_t *p_i )
{
    vlc_mutex_lock( &p_i->lock );

    mtime_t i_duration = p_i->i_duration;

    vlc_mutex_unlock( &p_i->lock );
    return i_duration;
}

void input_item_SetDuration( input_item_t *p_i, mtime_t i_duration )
{
    bool b_send_event = false;

    vlc_mutex_lock( &p_i->lock );
    if( p_i->i_duration != i_duration )
    {
        p_i->i_duration = i_duration;
        b_send_event = true;
    }
    vlc_mutex_unlock( &p_i->lock );

    if( b_send_event )
    {
        vlc_event_t event;

        event.type = vlc_InputItemDurationChanged;
        event.u.input_item_duration_changed.new_duration = i_duration;
        vlc_event_send( &p_i->event_manager, &event );
    }
}


bool input_item_IsPreparsed( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    bool b_preparsed = p_item->p_meta ? ( p_item->p_meta->i_status & ITEM_PREPARSED ) != 0 : false;
    vlc_mutex_unlock( &p_item->lock );

    return b_preparsed;
}

bool input_item_IsArtFetched( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    bool b_fetched = p_item->p_meta ? ( p_item->p_meta->i_status & ITEM_ART_FETCHED ) != 0 : false;
    vlc_mutex_unlock( &p_item->lock );

    return b_fetched;
}

/**
 * Get a info item from a given category in a given input item.
 *
 * \param p_i The input item to get info from
 * \param psz_cat String representing the category for the info
 * \param psz_name String representing the name of the desired info
 * \return A pointer to the string with the given info if found, or an
 *         empty string otherwise. The caller should free the returned
 *         pointer.
 */
char *input_item_GetInfo( input_item_t *p_i,
                          const char *psz_cat,
                          const char *psz_name )
{
    vlc_mutex_lock( &p_i->lock );

    for( int i = 0; i< p_i->i_categories; i++ )
    {
        const info_category_t *p_cat = p_i->pp_categories[i];

        if( !psz_cat || strcmp( p_cat->psz_name, psz_cat ) )
            continue;

        for( int j = 0; j < p_cat->i_infos; j++ )
        {
            if( !strcmp( p_cat->pp_infos[j]->psz_name, psz_name ) )
            {
                char *psz_ret = strdup( p_cat->pp_infos[j]->psz_value );
                vlc_mutex_unlock( &p_i->lock );
                return psz_ret;
            }
        }
    }
    vlc_mutex_unlock( &p_i->lock );
    return strdup( "" );
}

static void input_item_Destroy ( gc_object_t *p_gc )
{
    input_item_t *p_item = vlc_priv( p_gc, input_item_t );

    input_item_Clean( p_item );
    free( p_item );
}

int input_item_AddOption( input_item_t *p_input, const char *psz_option,
                          unsigned flags )
{
    int err = VLC_SUCCESS;

    if( psz_option == NULL )
        return VLC_EGENERIC;

    vlc_mutex_lock( &p_input->lock );
    if (flags & VLC_INPUT_OPTION_UNIQUE)
    {
        for (int i = 0 ; i < p_input->i_options; i++)
            if( !strcmp( p_input->ppsz_options[i], psz_option ) )
                goto out;
    }

    uint8_t *flagv = realloc (p_input->optflagv, p_input->optflagc + 1);
    if (flagv == NULL)
    {
        err = VLC_ENOMEM;
        goto out;
    }
    p_input->optflagv = flagv;
    flagv[p_input->optflagc++] = flags;

    INSERT_ELEM( p_input->ppsz_options, p_input->i_options,
                 p_input->i_options, strdup( psz_option ) );
out:
    vlc_mutex_unlock( &p_input->lock );
    return err;
}

int input_item_AddInfo( input_item_t *p_i,
                        const char *psz_cat,
                        const char *psz_name,
                        const char *psz_format, ... )
{
    va_list args;
    int i;
    info_t *p_info = NULL;
    info_category_t *p_cat = NULL ;

    vlc_mutex_lock( &p_i->lock );

    for( i = 0 ; i < p_i->i_categories ; i ++ )
    {
        if( !strcmp( p_i->pp_categories[i]->psz_name, psz_cat ) )
        {
            p_cat = p_i->pp_categories[i];
            break;
        }
    }
    if( !p_cat )
    {
        if( !(p_cat = (info_category_t *)malloc( sizeof(info_category_t) )) )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_ENOMEM;
        }
        p_cat->psz_name = strdup( psz_cat );
        p_cat->i_infos = 0;
        p_cat->pp_infos = 0;
        INSERT_ELEM( p_i->pp_categories, p_i->i_categories, p_i->i_categories,
                     p_cat );
    }

    for( i = 0; i< p_cat->i_infos; i++ )
    {
        if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
        {
            p_info = p_cat->pp_infos[i];
            break;
        }
    }

    if( !p_info )
    {
        if( ( p_info = (info_t *)malloc( sizeof( info_t ) ) ) == NULL )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_ENOMEM;
        }
        INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos, p_cat->i_infos, p_info );
        p_info->psz_name = strdup( psz_name );
    }
    else
    {
        free( p_info->psz_value );
    }

    va_start( args, psz_format );
    if( vasprintf( &p_info->psz_value, psz_format, args) == -1 )
        p_info->psz_value = NULL;
    va_end( args );

    vlc_mutex_unlock( &p_i->lock );

    if( p_info->psz_value )
    {
        vlc_event_t event;

        event.type = vlc_InputItemInfoChanged;
        vlc_event_send( &p_i->event_manager, &event );
    }
    return p_info->psz_value ? VLC_SUCCESS : VLC_ENOMEM;
}
int input_item_DelInfo( input_item_t *p_i,
                        const char *psz_cat,
                        const char *psz_name )
{
    info_category_t *p_cat = NULL;
    int i_cat;
    int i;

    vlc_mutex_lock( &p_i->lock );
    for( i_cat = 0; i_cat < p_i->i_categories; i_cat++ )
    {
        if( !strcmp( p_i->pp_categories[i_cat]->psz_name,
                     psz_cat ) )
        {
            p_cat = p_i->pp_categories[i_cat];
            break;
        }
    }
    if( p_cat == NULL )
    {
        vlc_mutex_unlock( &p_i->lock );
        return VLC_EGENERIC;
    }

    if( psz_name )
    {
        /* Remove a specific info */
        for( i = 0; i < p_cat->i_infos; i++ )
        {
            if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
            {
                free( p_cat->pp_infos[i]->psz_name );
                free( p_cat->pp_infos[i]->psz_value );
                free( p_cat->pp_infos[i] );
                REMOVE_ELEM( p_cat->pp_infos, p_cat->i_infos, i );
                break;
            }
        }
        if( i >= p_cat->i_infos )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_EGENERIC;
        }
    }
    else
    {
        /* Remove the complete categorie */
        for( i = 0; i < p_cat->i_infos; i++ )
        {
            free( p_cat->pp_infos[i]->psz_name );
            free( p_cat->pp_infos[i]->psz_value );
            free( p_cat->pp_infos[i] );
        }
        free( p_cat->pp_infos );
        REMOVE_ELEM( p_i->pp_categories, p_i->i_categories, i_cat );
    }

    // Free the category
    free( p_cat->psz_name );
    free( p_cat );
    vlc_mutex_unlock( &p_i->lock );


    vlc_event_t event;
    event.type = vlc_InputItemInfoChanged;
    vlc_event_send( &p_i->event_manager, &event );

    return VLC_SUCCESS;
}


input_item_t *__input_item_NewExt( vlc_object_t *p_obj, const char *psz_uri,
                                  const char *psz_name,
                                  int i_options,
                                  const char *const *ppsz_options,
                                  unsigned i_option_flags,
                                  mtime_t i_duration )
{
    return input_item_NewWithType( p_obj, psz_uri, psz_name,
                                  i_options, ppsz_options, i_option_flags,
                                  i_duration, ITEM_TYPE_UNKNOWN );
}


input_item_t *input_item_NewWithType( vlc_object_t *p_obj, const char *psz_uri,
                                const char *psz_name,
                                int i_options,
                                const char *const *ppsz_options,
                                unsigned i_option_flags,
                                mtime_t i_duration,
                                int i_type )
{
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);

    input_item_t* p_input = malloc( sizeof(input_item_t ) );
    if( !p_input )
        return NULL;

    input_item_Init( p_obj, p_input );
    vlc_gc_init( p_input, input_item_Destroy );

    vlc_object_lock( p_obj->p_libvlc );
    p_input->i_id = ++priv->i_last_input_id;
    vlc_object_unlock( p_obj->p_libvlc );

    p_input->b_fixed_name = false;

    p_input->i_type = i_type;
    p_input->b_prefers_tree = false;

    if( psz_uri )
        input_item_SetURI( p_input, psz_uri );

    if( i_type != ITEM_TYPE_UNKNOWN )
        p_input->i_type = i_type;

    if( psz_name )
        input_item_SetName( p_input, psz_name );

    p_input->i_duration = i_duration;

    for( int i = 0; i < i_options; i++ )
        input_item_AddOption( p_input, ppsz_options[i], i_option_flags );
    return p_input;
}

/* Guess the type of the item using the beginning of the mrl */
static void GuessType( input_item_t *p_item)
{
    int i;
    static struct { const char *psz_search; int i_type; }  types_array[] =
    {
        { "http", ITEM_TYPE_NET },
        { "dvd", ITEM_TYPE_DISC },
        { "cdda", ITEM_TYPE_CDDA },
        { "mms", ITEM_TYPE_NET },
        { "rtsp", ITEM_TYPE_NET },
        { "udp", ITEM_TYPE_NET },
        { "rtp", ITEM_TYPE_NET },
        { "vcd", ITEM_TYPE_DISC },
        { "v4l", ITEM_TYPE_CARD },
        { "dshow", ITEM_TYPE_CARD },
        { "pvr", ITEM_TYPE_CARD },
        { "dvb", ITEM_TYPE_CARD },
        { "qpsk", ITEM_TYPE_CARD },
        { "sdp", ITEM_TYPE_NET },
        { NULL, 0 }
    };

    if( !p_item->psz_uri )
    {
        p_item->i_type = ITEM_TYPE_FILE;
        return;
    }

    for( i = 0; types_array[i].psz_search != NULL; i++ )
    {
        if( !strncmp( p_item->psz_uri, types_array[i].psz_search,
                      strlen( types_array[i].psz_search ) ) )
        {
            p_item->i_type = types_array[i].i_type;
            return;
        }
    }
    p_item->i_type = ITEM_TYPE_FILE;
}
