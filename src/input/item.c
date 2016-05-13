/*****************************************************************************
 * item.c: input_item management
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 * $Id$
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
#include <assert.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_interface.h>
#include <vlc_charset.h>

#include "item.h"
#include "info.h"

struct input_item_opaque
{
    struct input_item_opaque *next;
    void *value;
    char name[1];
};

static int GuessType( const input_item_t *p_item, bool *p_net );

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
void input_item_SignalPreparseEnded( input_item_t *p_i, int status )
{
    vlc_event_t event;
    event.type = vlc_InputItemPreparseEnded;
    event.u.input_item_preparse_ended.new_status = status;
    vlc_event_send( &p_i->event_manager, &event );
}

void input_item_SetPreparsed( input_item_t *p_i, bool b_preparsed )
{
    bool b_send_event = false;

    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    int status = vlc_meta_GetStatus(p_i->p_meta);
    int new_status;
    if( b_preparsed )
        new_status = status | ITEM_PREPARSED;
    else
        new_status = status & ~ITEM_PREPARSED;
    if( status != new_status )
    {
        vlc_meta_SetStatus(p_i->p_meta, new_status);
        b_send_event = true;
    }

    vlc_mutex_unlock( &p_i->lock );

    if( b_send_event )
    {
        vlc_event_t event;
        event.type = vlc_InputItemPreparsedChanged;
        event.u.input_item_preparsed_changed.new_status = new_status;
        vlc_event_send( &p_i->event_manager, &event );
    }
}

void input_item_SetArtNotFound( input_item_t *p_i, bool b_not_found )
{
    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    int status = vlc_meta_GetStatus(p_i->p_meta);

    if( b_not_found )
        status |= ITEM_ART_NOTFOUND;
    else
        status &= ~ITEM_ART_NOTFOUND;

    vlc_meta_SetStatus(p_i->p_meta, status);

    vlc_mutex_unlock( &p_i->lock );
}

void input_item_SetArtFetched( input_item_t *p_i, bool b_art_fetched )
{
    vlc_mutex_lock( &p_i->lock );

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    int status = vlc_meta_GetStatus(p_i->p_meta);

    if( b_art_fetched )
        status |= ITEM_ART_FETCHED;
    else
        status &= ~ITEM_ART_FETCHED;

    vlc_meta_SetStatus(p_i->p_meta, status);

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

void input_item_CopyOptions( input_item_t *p_child,
                             input_item_t *p_parent )
{
    char **optv = NULL;
    uint8_t *flagv = NULL;
    size_t optc;

    vlc_mutex_lock( &p_parent->lock );

    optc = p_parent->i_options;
    if( optc > 0 )
    {
        optv = xmalloc( optc * sizeof (*optv) );
        for( size_t i = 0; i < optc; i++ )
            optv[i] = xstrdup( p_parent->ppsz_options[i] );

        flagv = xmalloc( optc * sizeof (*flagv) );
        memcpy( flagv, p_parent->optflagv, optc * sizeof (*flagv) );
    }

    vlc_mutex_unlock( &p_parent->lock );

    if( optc == 0 )
        return;

    vlc_mutex_lock( &p_child->lock );

    p_child->ppsz_options = xrealloc( p_child->ppsz_options,
                                (p_child->i_options + optc) * sizeof (*optv) );
    memcpy( p_child->ppsz_options + p_child->i_options, optv,
            optc * sizeof (*optv) );
    p_child->i_options += optc;

    p_child->optflagv = xrealloc( p_child->optflagv,
                               (p_child->i_options + optc) * sizeof (*flagv) );
    memcpy( p_child->optflagv + p_child->i_options, flagv,
            optc * sizeof (*flagv) );
    p_child->optflagc += optc;

    vlc_mutex_unlock( &p_child->lock );

    free( flagv );
    free( optv );
}

static void post_subitems( input_item_node_t *p_node )
{
    for( int i = 0; i < p_node->i_children; i++ )
    {
        vlc_event_t event;
        event.type = vlc_InputItemSubItemAdded;
        event.u.input_item_subitem_added.p_new_child = p_node->pp_children[i]->p_item;
        vlc_event_send( &p_node->p_item->event_manager, &event );

        post_subitems( p_node->pp_children[i] );
    }
}

/* This won't hold the item, but can tell to interested third parties
 * Like the playlist, that there is a new sub item. With this design
 * It is not the input item's responsibility to keep all the ref of
 * the input item children. */
void input_item_PostSubItem( input_item_t *p_parent, input_item_t *p_child )
{
    input_item_node_t *p_node = input_item_node_Create( p_parent );
    input_item_node_AppendItem( p_node, p_child );
    input_item_node_PostAndDelete( p_node );
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

/* Get the title of a given item or fallback to the name if the title is empty */
char *input_item_GetTitleFbName( input_item_t *p_item )
{
    char *psz_ret;
    vlc_mutex_lock( &p_item->lock );

    if( !p_item->p_meta )
    {
        psz_ret = p_item->psz_name ? strdup( p_item->psz_name ) : NULL;
        vlc_mutex_unlock( &p_item->lock );
        return psz_ret;
    }

    const char *psz_title = vlc_meta_Get( p_item->p_meta, vlc_meta_Title );
    if( !EMPTY_STR( psz_title ) )
        psz_ret = strdup( psz_title );
    else
        psz_ret = p_item->psz_name ? strdup( p_item->psz_name ) : NULL;

    vlc_mutex_unlock( &p_item->lock );
    return psz_ret;
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
    assert( psz_uri );
#ifndef NDEBUG
    if( !strstr( psz_uri, "://" )
     || strchr( psz_uri, ' ' ) || strchr( psz_uri, '"' ) )
        fprintf( stderr, "Warning: %s(\"%s\"): file path instead of URL.\n",
                 __func__, psz_uri );
#endif
    vlc_mutex_lock( &p_i->lock );
    free( p_i->psz_uri );
    p_i->psz_uri = strdup( psz_uri );

    p_i->i_type = GuessType( p_i, &p_i->b_net );

    if( p_i->psz_name )
        ;
    else
    if( p_i->i_type == ITEM_TYPE_FILE || p_i->i_type == ITEM_TYPE_DIRECTORY )
    {
        const char *psz_filename = strrchr( p_i->psz_uri, '/' );

        if( psz_filename && *psz_filename == '/' )
            psz_filename++;
        if( psz_filename && *psz_filename )
            p_i->psz_name = strdup( psz_filename );

        /* Make the name more readable */
        if( p_i->psz_name )
        {
            vlc_uri_decode( p_i->psz_name );
            EnsureUTF8( p_i->psz_name );
        }
    }
    else
    {   /* Strip login and password from title */
        int r;
        vlc_url_t url;

        vlc_UrlParse( &url, psz_uri );
        if( url.psz_protocol )
        {
            if( url.i_port > 0 )
                r=asprintf( &p_i->psz_name, "%s://%s:%d%s", url.psz_protocol,
                          url.psz_host, url.i_port,
                          url.psz_path ? url.psz_path : "" );
            else
                r=asprintf( &p_i->psz_name, "%s://%s%s", url.psz_protocol,
                          url.psz_host ? url.psz_host : "",
                          url.psz_path ? url.psz_path : "" );
        }
        else
        {
            if( url.i_port > 0 )
                r=asprintf( &p_i->psz_name, "%s:%d%s", url.psz_host, url.i_port,
                          url.psz_path ? url.psz_path : "" );
            else
                r=asprintf( &p_i->psz_name, "%s%s", url.psz_host,
                          url.psz_path ? url.psz_path : "" );
        }
        vlc_UrlClean( &url );
        if( -1==r )
            p_i->psz_name=NULL; /* recover from undefined value */
    }

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

char *input_item_GetNowPlayingFb( input_item_t *p_item )
{
    char *psz_meta = input_item_GetMeta( p_item, vlc_meta_NowPlaying );
    if( !psz_meta || strlen( psz_meta ) == 0 )
    {
        free( psz_meta );
        return input_item_GetMeta( p_item, vlc_meta_ESNowPlaying );
    }

    return psz_meta;
}

bool input_item_IsPreparsed( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    bool b_preparsed = p_item->p_meta ? ( vlc_meta_GetStatus(p_item->p_meta) & ITEM_PREPARSED ) != 0 : false;
    vlc_mutex_unlock( &p_item->lock );

    return b_preparsed;
}

bool input_item_IsArtFetched( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    bool b_fetched = p_item->p_meta ? ( vlc_meta_GetStatus(p_item->p_meta) & ITEM_ART_FETCHED ) != 0 : false;
    vlc_mutex_unlock( &p_item->lock );

    return b_fetched;
}

bool input_item_ShouldPreparseSubItems( input_item_t *p_item )
{
    bool b_ret;

    vlc_mutex_lock( &p_item->lock );
    b_ret = p_item->i_preparse_depth == -1 ? true : p_item->i_preparse_depth > 0;
    vlc_mutex_unlock( &p_item->lock );

    return b_ret;
}

input_item_t *input_item_Hold( input_item_t *p_item )
{
    input_item_owner_t *owner = item_owner(p_item);

    atomic_fetch_add( &owner->refs, 1 );
    return p_item;
}

void input_item_Release( input_item_t *p_item )
{
    input_item_owner_t *owner = item_owner(p_item);

    if( atomic_fetch_sub(&owner->refs, 1) != 1 )
        return;

    vlc_event_manager_fini( &p_item->event_manager );

    free( p_item->psz_name );
    free( p_item->psz_uri );
    if( p_item->p_stats != NULL )
    {
        vlc_mutex_destroy( &p_item->p_stats->lock );
        free( p_item->p_stats );
    }

    if( p_item->p_meta != NULL )
        vlc_meta_Delete( p_item->p_meta );

    for( input_item_opaque_t *o = p_item->opaques, *next; o != NULL; o = next )
    {
        next = o->next;
        free( o );
    }

    for( int i = 0; i < p_item->i_options; i++ )
        free( p_item->ppsz_options[i] );
    TAB_CLEAN( p_item->i_options, p_item->ppsz_options );
    free( p_item->optflagv );

    for( int i = 0; i < p_item->i_es; i++ )
    {
        es_format_Clean( p_item->es[i] );
        free( p_item->es[i] );
    }
    TAB_CLEAN( p_item->i_es, p_item->es );

    for( int i = 0; i < p_item->i_epg; i++ )
        vlc_epg_Delete( p_item->pp_epg[i] );
    TAB_CLEAN( p_item->i_epg, p_item->pp_epg );

    for( int i = 0; i < p_item->i_categories; i++ )
        info_category_Delete( p_item->pp_categories[i] );
    TAB_CLEAN( p_item->i_categories, p_item->pp_categories );

    vlc_mutex_destroy( &p_item->lock );
    free( owner );
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

int input_item_AddOptions( input_item_t *p_item, int i_options,
                           const char *const *ppsz_options,
                           unsigned i_flags )
{
    int i_ret = VLC_SUCCESS;
    for( int i = 0; i < i_options && i_ret == VLC_SUCCESS; i++ )
        i_ret = input_item_AddOption( p_item, ppsz_options[i], i_flags );
    return i_ret;
}

int input_item_AddOpaque(input_item_t *item, const char *name, void *value)
{
    assert(name != NULL);

    size_t namelen = strlen(name);
    input_item_opaque_t *entry = malloc(sizeof (*entry) + namelen);
    if (unlikely(entry == NULL))
        return VLC_ENOMEM;

    memcpy(entry->name, name, namelen + 1);
    entry->value = value;

    vlc_mutex_lock(&item->lock);
    entry->next = item->opaques;
    item->opaques = entry;
    vlc_mutex_unlock(&item->lock);
    return VLC_SUCCESS;
}

void input_item_ApplyOptions(vlc_object_t *obj, input_item_t *item)
{
    vlc_mutex_lock(&item->lock);
    assert(item->optflagc == (unsigned)item->i_options);

    for (unsigned i = 0; i < (unsigned)item->i_options; i++)
        var_OptionParse(obj, item->ppsz_options[i],
                        !!(item->optflagv[i] & VLC_INPUT_OPTION_TRUSTED));

    for (const input_item_opaque_t *o = item->opaques; o != NULL; o = o->next)
    {
        var_Create(obj, o->name, VLC_VAR_ADDRESS);
        var_SetAddress(obj, o->name, o->value);
    }

    vlc_mutex_unlock(&item->lock);
}

static info_category_t *InputItemFindCat( input_item_t *p_item,
                                          int *pi_index, const char *psz_cat )
{
    vlc_assert_locked( &p_item->lock );
    for( int i = 0; i < p_item->i_categories && psz_cat; i++ )
    {
        info_category_t *p_cat = p_item->pp_categories[i];

        if( !strcmp( p_cat->psz_name, psz_cat ) )
        {
            if( pi_index )
                *pi_index = i;
            return p_cat;
        }
    }
    return NULL;
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

    const info_category_t *p_cat = InputItemFindCat( p_i, NULL, psz_cat );
    if( p_cat )
    {
        info_t *p_info = info_category_FindInfo( p_cat, NULL, psz_name );
        if( p_info && p_info->psz_value )
        {
            char *psz_ret = strdup( p_info->psz_value );
            vlc_mutex_unlock( &p_i->lock );
            return psz_ret;
        }
    }
    vlc_mutex_unlock( &p_i->lock );
    return strdup( "" );
}

static int InputItemVaAddInfo( input_item_t *p_i,
                               const char *psz_cat,
                               const char *psz_name,
                               const char *psz_format, va_list args )
{
    vlc_assert_locked( &p_i->lock );

    info_category_t *p_cat = InputItemFindCat( p_i, NULL, psz_cat );
    if( !p_cat )
    {
        p_cat = info_category_New( psz_cat );
        if( !p_cat )
            return VLC_ENOMEM;
        INSERT_ELEM( p_i->pp_categories, p_i->i_categories, p_i->i_categories,
                     p_cat );
    }
    info_t *p_info = info_category_VaAddInfo( p_cat, psz_name, psz_format, args );
    if( !p_info || !p_info->psz_value )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int InputItemAddInfo( input_item_t *p_i,
                             const char *psz_cat,
                             const char *psz_name,
                             const char *psz_format, ... )
{
    va_list args;

    va_start( args, psz_format );
    const int i_ret = InputItemVaAddInfo( p_i, psz_cat, psz_name, psz_format, args );
    va_end( args );

    return i_ret;
}

int input_item_AddInfo( input_item_t *p_i,
                        const char *psz_cat,
                        const char *psz_name,
                        const char *psz_format, ... )
{
    va_list args;

    vlc_mutex_lock( &p_i->lock );

    va_start( args, psz_format );
    const int i_ret = InputItemVaAddInfo( p_i, psz_cat, psz_name, psz_format, args );
    va_end( args );

    vlc_mutex_unlock( &p_i->lock );


    if( !i_ret )
    {
        vlc_event_t event;

        event.type = vlc_InputItemInfoChanged;
        vlc_event_send( &p_i->event_manager, &event );
    }
    return i_ret;
}

int input_item_DelInfo( input_item_t *p_i,
                        const char *psz_cat,
                        const char *psz_name )
{
    vlc_mutex_lock( &p_i->lock );
    int i_cat;
    info_category_t *p_cat = InputItemFindCat( p_i, &i_cat, psz_cat );
    if( !p_cat )
    {
        vlc_mutex_unlock( &p_i->lock );
        return VLC_EGENERIC;
    }

    if( psz_name )
    {
        /* Remove a specific info */
        int i_ret = info_category_DeleteInfo( p_cat, psz_name );
        if( i_ret )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_EGENERIC;
        }
    }
    else
    {
        /* Remove the complete categorie */
        info_category_Delete( p_cat );
        REMOVE_ELEM( p_i->pp_categories, p_i->i_categories, i_cat );
    }
    vlc_mutex_unlock( &p_i->lock );


    vlc_event_t event;
    event.type = vlc_InputItemInfoChanged;
    vlc_event_send( &p_i->event_manager, &event );

    return VLC_SUCCESS;
}
void input_item_ReplaceInfos( input_item_t *p_item, info_category_t *p_cat )
{
    vlc_mutex_lock( &p_item->lock );
    int i_cat;
    info_category_t *p_old = InputItemFindCat( p_item, &i_cat, p_cat->psz_name );
    if( p_old )
    {
        info_category_Delete( p_old );
        p_item->pp_categories[i_cat] = p_cat;
    }
    else
    {
        INSERT_ELEM( p_item->pp_categories, p_item->i_categories, p_item->i_categories,
                     p_cat );
    }
    vlc_mutex_unlock( &p_item->lock );


    vlc_event_t event;
    event.type = vlc_InputItemInfoChanged;
    vlc_event_send( &p_item->event_manager, &event );
}
void input_item_MergeInfos( input_item_t *p_item, info_category_t *p_cat )
{
    vlc_mutex_lock( &p_item->lock );
    info_category_t *p_old = InputItemFindCat( p_item, NULL, p_cat->psz_name );
    if( p_old )
    {
        for( int i = 0; i < p_cat->i_infos; i++ )
            info_category_ReplaceInfo( p_old, p_cat->pp_infos[i] );
        TAB_CLEAN( p_cat->i_infos, p_cat->pp_infos );
        info_category_Delete( p_cat );
    }
    else
    {
        INSERT_ELEM( p_item->pp_categories, p_item->i_categories, p_item->i_categories,
                     p_cat );
    }
    vlc_mutex_unlock( &p_item->lock );


    vlc_event_t event;
    event.type = vlc_InputItemInfoChanged;
    vlc_event_send( &p_item->event_manager, &event );
}

#define EPG_DEBUG
void input_item_SetEpg( input_item_t *p_item, const vlc_epg_t *p_update )
{
    vlc_mutex_lock( &p_item->lock );

    /* */
    vlc_epg_t *p_epg = NULL;
    for( int i = 0; i < p_item->i_epg; i++ )
    {
        vlc_epg_t *p_tmp = p_item->pp_epg[i];

        if( (p_tmp->psz_name == NULL) != (p_update->psz_name == NULL) )
            continue;
        if( p_tmp->psz_name && p_update->psz_name && strcmp(p_tmp->psz_name, p_update->psz_name) )
            continue;

        p_epg = p_tmp;
        break;
    }

    /* */
    if( !p_epg )
    {
        p_epg = vlc_epg_New( p_update->psz_name );
        if( p_epg )
            TAB_APPEND( p_item->i_epg, p_item->pp_epg, p_epg );
    }
    if( p_epg )
        vlc_epg_Merge( p_epg, p_update );

    vlc_mutex_unlock( &p_item->lock );

    if( !p_epg )
        return;

#ifdef EPG_DEBUG
    char *psz_epg;
    if( asprintf( &psz_epg, "EPG %s", p_epg->psz_name ? p_epg->psz_name : "unknown" ) < 0 )
        goto signal;

    input_item_DelInfo( p_item, psz_epg, NULL );

    vlc_mutex_lock( &p_item->lock );
    for( int i = 0; i < p_epg->i_event; i++ )
    {
        const vlc_epg_event_t *p_evt = p_epg->pp_event[i];
        time_t t_start = (time_t)p_evt->i_start;
        struct tm tm_start;
        char psz_start[128];

        localtime_r( &t_start, &tm_start );

        snprintf( psz_start, sizeof(psz_start), "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
                  1900 + tm_start.tm_year, 1 + tm_start.tm_mon, tm_start.tm_mday,
                  tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec );
        if( p_evt->psz_short_description || p_evt->psz_description )
            InputItemAddInfo( p_item, psz_epg, psz_start, "%s (%2.2d:%2.2d) - %s %s",
                              p_evt->psz_name,
                              p_evt->i_duration/60/60, (p_evt->i_duration/60)%60,
                              p_evt->psz_short_description ? p_evt->psz_short_description : "" ,
                              p_evt->psz_description ? p_evt->psz_description : "" );
        else
            InputItemAddInfo( p_item, psz_epg, psz_start, "%s (%2.2d:%2.2d)",
                              p_evt->psz_name,
                              p_evt->i_duration/60/60, (p_evt->i_duration/60)%60 );
    }
    vlc_mutex_unlock( &p_item->lock );
    free( psz_epg );
signal:
#endif

    if( p_epg->i_event > 0 )
    {
        vlc_event_t event = { .type = vlc_InputItemInfoChanged, };
        vlc_event_send( &p_item->event_manager, &event );
    }
}

void input_item_SetEpgOffline( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    for( int i = 0; i < p_item->i_epg; i++ )
        vlc_epg_SetCurrent( p_item->pp_epg[i], -1 );
    vlc_mutex_unlock( &p_item->lock );

#ifdef EPG_DEBUG
    vlc_mutex_lock( &p_item->lock );
    const int i_epg_info = p_item->i_epg;
    if( i_epg_info > 0 )
    {
        char *ppsz_epg_info[i_epg_info];
        for( int i = 0; i < p_item->i_epg; i++ )
        {
            const vlc_epg_t *p_epg = p_item->pp_epg[i];
            if( asprintf( &ppsz_epg_info[i], "EPG %s", p_epg->psz_name ? p_epg->psz_name : "unknown" ) < 0 )
                ppsz_epg_info[i] = NULL;
        }
        vlc_mutex_unlock( &p_item->lock );

        for( int i = 0; i < i_epg_info; i++ )
        {
            if( !ppsz_epg_info[i] )
                continue;
            input_item_DelInfo( p_item, ppsz_epg_info[i], NULL );
            free( ppsz_epg_info[i] );
        }
    }
    else
        vlc_mutex_unlock( &p_item->lock );
#endif

    vlc_event_t event = { .type = vlc_InputItemInfoChanged, };
    vlc_event_send( &p_item->event_manager, &event );
}

input_item_t *
input_item_NewExt( const char *psz_uri, const char *psz_name,
                   mtime_t duration, int type, enum input_item_net_type i_net )
{
    static atomic_uint last_input_id = ATOMIC_VAR_INIT(0);

    input_item_owner_t *owner = calloc( 1, sizeof( *owner ) );
    if( unlikely(owner == NULL) )
        return NULL;

    atomic_init( &owner->refs, 1 );

    input_item_t *p_input = &owner->item;
    vlc_event_manager_t * p_em = &p_input->event_manager;

    p_input->i_id = atomic_fetch_add(&last_input_id, 1);
    vlc_mutex_init( &p_input->lock );

    p_input->psz_name = NULL;
    if( psz_name )
        input_item_SetName( p_input, psz_name );

    p_input->psz_uri = NULL;
    if( psz_uri )
        input_item_SetURI( p_input, psz_uri );
    else
    {
        p_input->i_type = ITEM_TYPE_UNKNOWN;
        p_input->b_net = false;
    }

    TAB_INIT( p_input->i_options, p_input->ppsz_options );
    p_input->optflagc = 0;
    p_input->optflagv = NULL;
    p_input->opaques = NULL;

    p_input->i_duration = duration;
    TAB_INIT( p_input->i_categories, p_input->pp_categories );
    TAB_INIT( p_input->i_es, p_input->es );
    p_input->p_stats = NULL;
    p_input->p_meta = NULL;
    TAB_INIT( p_input->i_epg, p_input->pp_epg );

    vlc_event_manager_init( p_em, p_input );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemMetaChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemSubItemAdded );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemSubItemTreeAdded );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemDurationChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemPreparsedChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemNameChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemInfoChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemErrorWhenReadingChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputItemPreparseEnded );

    if( type != ITEM_TYPE_UNKNOWN )
        p_input->i_type = type;
    p_input->b_error_when_reading = false;

    if( i_net != ITEM_NET_UNKNOWN )
        p_input->b_net = i_net == ITEM_NET;
    return p_input;
}

input_item_t *input_item_Copy( input_item_t *p_input )
{
    vlc_meta_t *meta = NULL;
    input_item_t *item;
    bool b_net;

    vlc_mutex_lock( &p_input->lock );

    item = input_item_NewExt( p_input->psz_uri, p_input->psz_name,
                              p_input->i_duration, p_input->i_type,
                              ITEM_NET_UNKNOWN );
    if( likely(item != NULL) && p_input->p_meta != NULL )
    {
        meta = vlc_meta_New();
        vlc_meta_Merge( meta, p_input->p_meta );
    }
    b_net = p_input->b_net;
    vlc_mutex_unlock( &p_input->lock );

    if( likely(item != NULL) )
    {   /* No need to lock; no other thread has seen this new item yet. */
        input_item_CopyOptions( item, p_input );
        item->p_meta = meta;
        item->b_net = b_net;
    }

    return item;
}

struct item_type_entry
{
    const char *psz_scheme;
    uint8_t    i_type;
    bool       b_net;
};

static int typecmp( const void *key, const void *entry )
{
    const struct item_type_entry *type = entry;
    const char *uri = key, *scheme = type->psz_scheme;

    return strncmp( uri, scheme, strlen( scheme ) );
}

/* Guess the type of the item using the beginning of the mrl */
static int GuessType( const input_item_t *p_item, bool *p_net )
{
    static const struct item_type_entry tab[] =
    {   /* /!\ Alphabetical order /!\ */
        /* Short match work, not just exact match */
        { "alsa",   ITEM_TYPE_CARD, false },
        { "atsc",   ITEM_TYPE_CARD, false },
        { "bd",     ITEM_TYPE_DISC, false },
        { "bluray", ITEM_TYPE_DISC, false },
        { "cable",  ITEM_TYPE_CARD, false },
        { "cdda",   ITEM_TYPE_DISC, false },
        { "cqam",   ITEM_TYPE_CARD, false },
        { "dc1394", ITEM_TYPE_CARD, false },
        { "dccp",   ITEM_TYPE_STREAM, true },
        { "deckli", ITEM_TYPE_CARD, false }, /* decklink */
        { "dir",    ITEM_TYPE_DIRECTORY, false },
        { "dshow",  ITEM_TYPE_CARD, false },
        { "dtv",    ITEM_TYPE_CARD, false },
        { "dvb",    ITEM_TYPE_CARD, false },
        { "dvd",    ITEM_TYPE_DISC, false },
        { "eyetv",  ITEM_TYPE_CARD, false },
        { "fd",     ITEM_TYPE_UNKNOWN, false },
        { "file",   ITEM_TYPE_FILE, false },
        { "ftp",    ITEM_TYPE_FILE, true },
        { "http",   ITEM_TYPE_FILE, true },
        { "icyx",   ITEM_TYPE_STREAM, true },
        { "imem",   ITEM_TYPE_UNKNOWN, false },
        { "isdb-",  ITEM_TYPE_CARD, false },
        { "itpc",   ITEM_TYPE_PLAYLIST, true },
        { "jack",   ITEM_TYPE_CARD, false },
        { "linsys", ITEM_TYPE_CARD, false },
        { "live",   ITEM_TYPE_STREAM, true }, /* livedotcom */
        { "mms",    ITEM_TYPE_STREAM, true },
        { "mtp",    ITEM_TYPE_DISC, false },
        { "nfs",    ITEM_TYPE_FILE, true },
        { "ofdm",   ITEM_TYPE_CARD, false },
        { "oss",    ITEM_TYPE_CARD, false },
        { "pnm",    ITEM_TYPE_STREAM, true },
        { "pulse",  ITEM_TYPE_CARD, false },
        { "qam",    ITEM_TYPE_CARD, false },
        { "qpsk",   ITEM_TYPE_CARD, false },
        { "qtcapt", ITEM_TYPE_CARD, false }, /* qtcapture */
        { "qtsound",ITEM_TYPE_CARD, false },
        { "raw139", ITEM_TYPE_CARD, false }, /* raw1394 */
        { "rt",     ITEM_TYPE_STREAM, true }, /* rtp, rtsp, rtmp */
        { "satell", ITEM_TYPE_CARD, false }, /* satellite */
        { "screen", ITEM_TYPE_CARD, false },
        { "sdp",    ITEM_TYPE_STREAM, true },
        { "sftp",   ITEM_TYPE_FILE, true },
        { "shm",    ITEM_TYPE_CARD, false },
        { "smb",    ITEM_TYPE_FILE, true },
        { "stream", ITEM_TYPE_STREAM, false },
        { "svcd",   ITEM_TYPE_DISC, false },
        { "tcp",    ITEM_TYPE_STREAM, true },
        { "terres", ITEM_TYPE_CARD, false }, /* terrestrial */
        { "udp",    ITEM_TYPE_STREAM, true },  /* udplite too */
        { "unsv",   ITEM_TYPE_STREAM, true },
        { "upnp",   ITEM_TYPE_FILE, true },
        { "usdigi", ITEM_TYPE_CARD, false }, /* usdigital */
        { "v4l",    ITEM_TYPE_CARD, false },
        { "vcd",    ITEM_TYPE_DISC, false },
        { "vdr",    ITEM_TYPE_STREAM, true },
        { "wasapi", ITEM_TYPE_CARD, false },
        { "window", ITEM_TYPE_CARD, false },
    };

#ifndef NDEBUG
    for( size_t i = 1; i < ARRAY_SIZE( tab ); i++ )
        assert( typecmp( (tab + i)->psz_scheme, tab + i - 1 ) > 0 );
#endif

    *p_net = false;

    if( strstr( p_item->psz_uri, "://" ) == NULL )
        return ITEM_TYPE_UNKNOWN; /* invalid URI */

    const struct item_type_entry *e =
        bsearch( p_item->psz_uri, tab, ARRAY_SIZE( tab ),
                 sizeof( tab[0] ), typecmp );
    if( e == NULL )
        return ITEM_TYPE_UNKNOWN;

    *p_net = e->b_net;
    return e->i_type;
}

input_item_node_t *input_item_node_Create( input_item_t *p_input )
{
    input_item_node_t* p_node = malloc( sizeof( input_item_node_t ) );
    if( !p_node )
        return NULL;

    assert( p_input );

    p_node->p_item = p_input;
    vlc_gc_incref( p_input );

    p_node->p_parent = NULL;
    p_node->i_children = 0;
    p_node->pp_children = NULL;
    p_node->b_can_loop = false;

    return p_node;
}

static void RecursiveNodeDelete( input_item_node_t *p_node )
{
    for( int i = 0; i < p_node->i_children; i++ )
        RecursiveNodeDelete( p_node->pp_children[i] );

    vlc_gc_decref( p_node->p_item );
    free( p_node->pp_children );
    free( p_node );
}

void input_item_node_Delete( input_item_node_t *p_node )
{
    if( p_node->p_parent )
        for( int i = 0; i < p_node->p_parent->i_children; i++ )
            if( p_node->p_parent->pp_children[i] == p_node )
            {
                REMOVE_ELEM( p_node->p_parent->pp_children,
                        p_node->p_parent->i_children,
                        i );
                break;
            }

    RecursiveNodeDelete( p_node );
}

input_item_node_t *input_item_node_AppendItem( input_item_node_t *p_node, input_item_t *p_item )
{
    int i_preparse_depth;
    input_item_node_t *p_new_child = input_item_node_Create( p_item );
    if( !p_new_child ) return NULL;

    vlc_mutex_lock( &p_node->p_item->lock );
    i_preparse_depth = p_node->p_item->i_preparse_depth;
    vlc_mutex_unlock( &p_node->p_item->lock );

    vlc_mutex_lock( &p_item->lock );
    p_item->i_preparse_depth = i_preparse_depth > 0 ?
                               i_preparse_depth -1 :
                               i_preparse_depth;
    vlc_mutex_unlock( &p_item->lock );

    input_item_node_AppendNode( p_node, p_new_child );
    return p_new_child;
}

void input_item_node_AppendNode( input_item_node_t *p_parent, input_item_node_t *p_child )
{
    assert( p_parent && p_child && p_child->p_parent == NULL );
    INSERT_ELEM( p_parent->pp_children,
                 p_parent->i_children,
                 p_parent->i_children,
                 p_child );
    p_child->p_parent = p_parent;
}

static int compar_node( const void *p1, const void *p2 )
{
    input_item_node_t *p_node1 = *((input_item_node_t **) p1);
    input_item_node_t *p_node2 = *((input_item_node_t **) p2);

    assert( p_node1->p_parent && p_node1->p_parent == p_node2->p_parent &&
            p_node1->p_parent->compar_cb );

    input_item_compar_cb compar_cb = p_node1->p_parent->compar_cb;
    return compar_cb( p_node1->p_item, p_node2->p_item );
}

static void sort_subitems( input_item_node_t *p_node,
                           input_item_compar_cb compar_cb )
{
    if( p_node->i_children <= 0 || !compar_cb )
        return;

    p_node->compar_cb = compar_cb;

    /* Lock first all children. This avoids to lock/unlock them from each
     * compar callback call */
    for( int i = 0; i < p_node->i_children; i++ )
        vlc_mutex_lock( &p_node->pp_children[i]->p_item->lock );

    /* Sort current node */
    qsort( p_node->pp_children, p_node->i_children,
           sizeof(input_item_node_t *), compar_node );

    /* Unlock all children */
    for( int i = 0; i < p_node->i_children; i++ )
        vlc_mutex_unlock( &p_node->pp_children[i]->p_item->lock );

    p_node->compar_cb = NULL;

    /* Sort all children */
    for( int i = 0; i < p_node->i_children; i++ )
        sort_subitems( p_node->pp_children[i], compar_cb );
}

void input_item_node_Sort( input_item_node_t *p_node,
                           input_item_compar_cb compar_cb )
{
    sort_subitems( p_node, compar_cb );
}

void input_item_node_PostAndDelete( input_item_node_t *p_root )
{
    post_subitems( p_root );

    vlc_event_t event;
    event.type = vlc_InputItemSubItemTreeAdded;
    event.u.input_item_subitem_tree_added.p_root = p_root;
    vlc_event_send( &p_root->p_item->event_manager, &event );

    input_item_node_Delete( p_root );
}

/* Called by es_out when a new Elementary Stream is added or updated. */
void input_item_UpdateTracksInfo(input_item_t *item, const es_format_t *fmt)
{
    int i;
    es_format_t *fmt_copy = malloc(sizeof *fmt_copy);
    if (!fmt_copy)
        return;

    es_format_Copy(fmt_copy, fmt);
    /* XXX: we could free p_extra to save memory, we will likely not need
     * the decoder specific data */

    vlc_mutex_lock( &item->lock );

    for( i = 0; i < item->i_es; i++ )
    {
        if (item->es[i]->i_id != fmt->i_id)
            continue;

        /* We've found the right ES, replace it */
        es_format_Clean(item->es[i]);
        free(item->es[i]);
        item->es[i] = fmt_copy;
        vlc_mutex_unlock( &item->lock );
        return;
    }

    /* ES not found, insert it */
    TAB_APPEND(item->i_es, item->es, fmt_copy);
    vlc_mutex_unlock( &item->lock );
}
