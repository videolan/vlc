/*****************************************************************************
 * meta.c : Metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org
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

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_arrays.h>
#include <vlc_modules.h>

#include "input_internal.h"
#include "../preparser/art.h"
#include <vlc_charset.h>

struct vlc_meta_value
{
    vlc_meta_priority_t priority;
    char *value;
};

struct vlc_meta_t
{
    struct vlc_meta_value meta[VLC_META_TYPE_COUNT];

    vlc_dictionary_t extra_tags;

    int i_status;
};

const char *vlc_meta_TypeToString(vlc_meta_type_t meta_type)
{
    static const char posix_names[VLC_META_TYPE_COUNT][18] =
    {
        [vlc_meta_Title]       = N_("Title"),
        [vlc_meta_Artist]      = N_("Artist"),
        [vlc_meta_Genre]       = N_("Genre"),
        [vlc_meta_Copyright]   = N_("Copyright"),
        [vlc_meta_Album]       = N_("Album"),
        [vlc_meta_TrackNumber] = N_("Track number"),
        [vlc_meta_Description] = N_("Description"),
        [vlc_meta_Rating]      = N_("Rating"),
        [vlc_meta_Date]        = N_("Date"),
        [vlc_meta_Setting]     = N_("Setting"),
        [vlc_meta_URL]         = N_("URL"),
        [vlc_meta_Language]    = N_("Language"),
        [vlc_meta_ESNowPlaying]= N_("Now Playing"),
        [vlc_meta_NowPlaying]  = N_("Now Playing"),
        [vlc_meta_Publisher]   = N_("Publisher"),
        [vlc_meta_EncodedBy]   = N_("Encoded by"),
        [vlc_meta_ArtworkURL]  = N_("Artwork URL"),
        [vlc_meta_TrackID]     = N_("Track ID"),
        [vlc_meta_TrackTotal]  = N_("Number of Tracks"),
        [vlc_meta_Director]    = N_("Director"),
        [vlc_meta_Season]      = N_("Season"),
        [vlc_meta_Episode]     = N_("Episode"),
        [vlc_meta_ShowName]    = N_("Show Name"),
        [vlc_meta_Actors]      = N_("Actors"),
        [vlc_meta_AlbumArtist] = N_("Album Artist"),
        [vlc_meta_DiscNumber]  = N_("Disc number"),
        [vlc_meta_DiscTotal]   = N_("Total disc number")
    };

    assert(meta_type < ARRAY_SIZE(posix_names));
    assert(posix_names[meta_type][0] != '\0');
    return posix_names[meta_type];
}

/* FIXME bad name convention */
const char * vlc_meta_TypeToLocalizedString( vlc_meta_type_t meta_type )
{
    return vlc_gettext(vlc_meta_TypeToString(meta_type));
}


/**
 * vlc_meta constructor.
 * vlc_meta_Delete() will free the returned pointer.
 */
vlc_meta_t *vlc_meta_New( void )
{
    vlc_meta_t *m = (vlc_meta_t*)malloc( sizeof(*m) );
    if( !m )
        return NULL;
    for( int i = 0; i < VLC_META_TYPE_COUNT ; i++ )
    {
        m->meta[i].value = NULL;
        m->meta[i].priority = VLC_META_PRIORITY_BASIC;
    }
    m->i_status = 0;
    vlc_dictionary_init( &m->extra_tags, 0 );
    return m;
}

/* Allocate and insert the new value in the dictionary key `psz_name` */
static void vlc_meta_InsertExtra( vlc_meta_t *m, const char *psz_name, const char *psz_value, vlc_meta_priority_t priority )
{
    struct vlc_meta_value *meta_value = malloc( sizeof(*meta_value) );
    if( meta_value )
    {
        meta_value->value = strdup(psz_value);
        meta_value->priority = priority;
        vlc_dictionary_insert( &m->extra_tags, psz_name, meta_value );
    }
}

/* Free a dictionary key allocated by strdup() in vlc_meta_SetExtraWithPriority() */
static void vlc_meta_FreeExtraKey( void *p_data, void *p_obj )
{
    VLC_UNUSED( p_obj );
    struct vlc_meta_value *meta_value = p_data;
    free( meta_value->value );
    free( meta_value );
}

void vlc_meta_Delete( vlc_meta_t *m )
{
    for( int i = 0; i < VLC_META_TYPE_COUNT ; i++ )
        free( m->meta[i].value );
    vlc_dictionary_clear( &m->extra_tags, vlc_meta_FreeExtraKey, NULL );
    free( m );
}

/**
 * vlc_meta has two kinds of meta, the one in a table, and the one in a
 * dictionary.
 * FIXME - Why don't we merge those two?
 */

void vlc_meta_SetWithPriority( vlc_meta_t *p_meta, vlc_meta_type_t meta_type, const char *psz_val, vlc_meta_priority_t priority )
{
    free( p_meta->meta[meta_type].value );
    assert( psz_val == NULL || IsUTF8( psz_val ) );
    p_meta->meta[meta_type].value = psz_val ? strdup( psz_val ) : NULL;
    p_meta->meta[meta_type].priority = priority;
}

const char *vlc_meta_Get( const vlc_meta_t *p_meta, vlc_meta_type_t meta_type )
{
    return p_meta->meta[meta_type].value;
}

void vlc_meta_SetExtraWithPriority( vlc_meta_t *m, const char *psz_name, const char *psz_value, vlc_meta_priority_t priority )
{
    assert( psz_name );
    struct vlc_meta_value *old_meta_value = vlc_dictionary_value_for_key( &m->extra_tags, psz_name );
    if( old_meta_value != kVLCDictionaryNotFound )
        vlc_dictionary_remove_value_for_key( &m->extra_tags, psz_name,
                                            vlc_meta_FreeExtraKey, NULL );
    if ( psz_value )
        vlc_meta_InsertExtra( m, psz_name, psz_value, priority );
}

const char * vlc_meta_GetExtra( const vlc_meta_t *m, const char *psz_name )
{
    struct vlc_meta_value *meta_value = vlc_dictionary_value_for_key(&m->extra_tags, psz_name);
    if( !meta_value )
        return NULL;
    return meta_value->value;
}

unsigned vlc_meta_GetExtraCount( const vlc_meta_t *m )
{
    return vlc_dictionary_keys_count(&m->extra_tags);
}

char** vlc_meta_CopyExtraNames( const vlc_meta_t *m )
{
    return vlc_dictionary_all_keys(&m->extra_tags);
}

/**
 * vlc_meta status (see vlc_meta_status_e)
 */
int vlc_meta_GetStatus( vlc_meta_t *m )
{
    return m->i_status;
}

void vlc_meta_SetStatus( vlc_meta_t *m, int status )
{
    m->i_status = status;
}


/**
 * Merging meta
 */
void vlc_meta_Merge( vlc_meta_t *dst, const vlc_meta_t *src )
{
    if( !dst || !src )
        return;

    for( int i = 0; i < VLC_META_TYPE_COUNT; i++ )
    {
        /* overwrite metadata only when priority of src is 
           greater than or equal to the priority of dst */
        if( src->meta[i].value && src->meta[i].priority >= dst->meta[i].priority )
        {
            free( dst->meta[i].value );
            dst->meta[i].value = strdup( src->meta[i].value );
            dst->meta[i].priority = src->meta[i].priority;
        }
    }

    /* XXX: If speed up are needed, it is possible */
    char **ppsz_all_keys = vlc_dictionary_all_keys( &src->extra_tags );
    for( int i = 0; ppsz_all_keys && ppsz_all_keys[i]; i++ )
    {
        /* Always try to remove the previous value */
        struct vlc_meta_value *dst_meta_value = vlc_dictionary_value_for_key( &dst->extra_tags, ppsz_all_keys[i] );
        struct vlc_meta_value *src_meta_value = vlc_dictionary_value_for_key( &src->extra_tags, ppsz_all_keys[i] );
        if( dst_meta_value )
        {
            /* overwrite metadata only when priority of src is 
               greater than or equal to the priority of dst */
            if( src_meta_value->priority < dst_meta_value->priority )
                continue;

            vlc_dictionary_remove_value_for_key( &dst->extra_tags, ppsz_all_keys[i], vlc_meta_FreeExtraKey, NULL );
        }

        vlc_meta_InsertExtra( dst, ppsz_all_keys[i], src_meta_value->value, src_meta_value->priority );

        free( ppsz_all_keys[i] );
    }
    free( ppsz_all_keys );
}


void input_ExtractAttachmentAndCacheArt( input_thread_t *p_input,
                                         const char *name )
{
    input_item_t *p_item = input_priv(p_input)->p_item;

    if( input_item_IsArtFetched( p_item ) )
    {   /* XXX Weird, we should not end up with attachment:// art URL
         * unless there is a race condition */
        msg_Warn( p_input, "art already fetched" );
        if( likely(input_FindArtInCache( p_item ) == VLC_SUCCESS) )
            return;
    }

    /* */
    input_attachment_t *p_attachment = input_GetAttachment( p_input, name );
    if( !p_attachment )
    {
        msg_Warn( p_input, "art attachment %s not found", name );
        return;
    }

    /* */
    const char *psz_type = NULL;

    if( !strcmp( p_attachment->psz_mime, "image/jpeg" ) )
        psz_type = ".jpg";
    else if( !strcmp( p_attachment->psz_mime, "image/png" ) )
        psz_type = ".png";
    else if( !strcmp( p_attachment->psz_mime, "image/x-pict" ) )
        psz_type = ".pct";

    input_SaveArt( VLC_OBJECT(p_input), p_item,
                   p_attachment->p_data, p_attachment->i_data, psz_type );
    vlc_input_attachment_Release( p_attachment );
}

int input_item_WriteMeta( vlc_object_t *obj, input_item_t *p_item )
{
    meta_export_t *p_export =
        vlc_custom_create( obj, sizeof( *p_export ), "meta writer" );
    if( p_export == NULL )
        return VLC_ENOMEM;
    p_export->p_item = p_item;

    enum input_item_type_e type;
    vlc_mutex_lock( &p_item->lock );
    type = p_item->i_type;
    vlc_mutex_unlock( &p_item->lock );
    if( type != ITEM_TYPE_FILE )
        goto error;

    char *psz_uri = input_item_GetURI( p_item );
    p_export->psz_file = vlc_uri2path( psz_uri );
    if( p_export->psz_file == NULL )
        msg_Err( p_export, "cannot write meta to remote media %s", psz_uri );
    free( psz_uri );
    if( p_export->psz_file == NULL )
        goto error;

    module_t *p_mod = module_need( p_export, "meta writer", NULL, false );
    if( !p_mod )
      goto error;

    module_unneed( p_export, p_mod );
    vlc_object_delete(p_export);
    return VLC_SUCCESS;

error:
    vlc_object_delete(p_export);
    return VLC_EGENERIC;
}
