/*****************************************************************************
 * item.c: input_item management
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
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
#include <limits.h>
#include <ctype.h>

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_interface.h>
#include <vlc_charset.h>
#include <vlc_strings.h>

#include "item.h"
#include "info.h"
#include "input_internal.h"

struct input_item_opaque
{
    struct input_item_opaque *next;
    void *value;
    char name[1];
};

static enum input_item_type_e GuessType( const input_item_t *p_item, bool *p_net );

void input_item_SetErrorWhenReading( input_item_t *p_i, bool b_error )
{
    bool b_changed;

    vlc_mutex_lock( &p_i->lock );

    b_changed = p_i->b_error_when_reading != b_error;
    p_i->b_error_when_reading = b_error;

    vlc_mutex_unlock( &p_i->lock );

    if( b_changed )
    {
        vlc_event_send( &p_i->event_manager, &(vlc_event_t) {
            .type = vlc_InputItemErrorWhenReadingChanged,
            .u.input_item_error_when_reading_changed.new_value = b_error } );
    }
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
        vlc_event_send( &p_i->event_manager, &(vlc_event_t) {
            .type = vlc_InputItemPreparsedChanged,
            .u.input_item_preparsed_changed.new_status = new_status } );
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
    vlc_mutex_lock( &p_i->lock );
    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();
    vlc_meta_Set( p_i->p_meta, meta_type, psz_val );
    vlc_mutex_unlock( &p_i->lock );

    /* Notify interested third parties */
    vlc_event_send( &p_i->event_manager, &(vlc_event_t) {
        .type = vlc_InputItemMetaChanged,
        .u.input_item_meta_changed.meta_type = meta_type } );
}

void input_item_CopyOptions( input_item_t *p_child,
                             input_item_t *p_parent )
{
    char **optv = NULL;
    uint8_t *flagv = NULL;
    int optc = 0;
    char **optv_realloc = NULL;
    uint8_t *flagv_realloc = NULL;

    vlc_mutex_lock( &p_parent->lock );

    if( p_parent->i_options > 0 )
    {
        optv = vlc_alloc( p_parent->i_options, sizeof (*optv) );
        if( likely(optv) )
            flagv = vlc_alloc( p_parent->i_options, sizeof (*flagv) );

        if( likely(flagv) )
        {
            for( int i = 0; i < p_parent->i_options; i++ )
            {
                char *psz_dup = strdup( p_parent->ppsz_options[i] );
                if( likely(psz_dup) )
                {
                    flagv[optc] = p_parent->optflagv[i];
                    optv[optc++] = psz_dup;
                }
            }
        }
    }

    vlc_mutex_unlock( &p_parent->lock );

    if( likely(optv && flagv && optc ) )
    {
        vlc_mutex_lock( &p_child->lock );

        if( INT_MAX - p_child->i_options >= optc &&
            SIZE_MAX / sizeof (*flagv) >= (size_t) (p_child->i_options + optc) )
            flagv_realloc = realloc( p_child->optflagv,
                                    (p_child->i_options + optc) * sizeof (*flagv) );
        if( likely(flagv_realloc) )
        {
            p_child->optflagv = flagv_realloc;
            if( SIZE_MAX / sizeof (*optv) >= (size_t) (p_child->i_options + optc) )
                optv_realloc = realloc( p_child->ppsz_options,
                                       (p_child->i_options + optc) * sizeof (*optv) );
            if( likely(optv_realloc) )
            {
                p_child->ppsz_options = optv_realloc;
                memcpy( p_child->ppsz_options + p_child->i_options, optv,
                        optc * sizeof (*optv) );
                memcpy( p_child->optflagv + p_child->i_options, flagv,
                        optc * sizeof (*flagv) );
                p_child->i_options += optc;
                p_child->optflagc += optc;
            }
        }

        vlc_mutex_unlock( &p_child->lock );
    }

    if( unlikely(!flagv_realloc || !optv_realloc) )
    {
        /* Didn't copy pointers, so need to free the strdup() */
        for( int i=0; i<optc; i++ )
            free( optv[i] );
    }

    free( flagv );
    free( optv );
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

const char *input_item_GetMetaLocked(input_item_t *item,
                                     vlc_meta_type_t meta_type)
{
    vlc_mutex_assert(&item->lock);

    if (!item->p_meta)
        return NULL;

    return vlc_meta_Get(item->p_meta, meta_type);
}

char *input_item_GetMeta( input_item_t *p_i, vlc_meta_type_t meta_type )
{
    vlc_mutex_lock( &p_i->lock );
    const char *value = input_item_GetMetaLocked( p_i, meta_type );
    char *psz = value ? strdup( value ) : NULL;
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
                r=asprintf( &p_i->psz_name, "%s://%s:%u%s", url.psz_protocol,
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
                r=asprintf( &p_i->psz_name, "%s:%u%s", url.psz_host, url.i_port,
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

vlc_tick_t input_item_GetDuration( input_item_t *p_i )
{
    vlc_mutex_lock( &p_i->lock );

    vlc_tick_t i_duration = p_i->i_duration;

    vlc_mutex_unlock( &p_i->lock );
    if (i_duration == INPUT_DURATION_INDEFINITE)
        i_duration = 0;
    else if (i_duration == INPUT_DURATION_UNSET)
        i_duration = 0;
    return i_duration;
}

void input_item_SetDuration( input_item_t *p_i, vlc_tick_t i_duration )
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
        vlc_event_send( &p_i->event_manager, &(vlc_event_t) {
            .type = vlc_InputItemDurationChanged,
            .u.input_item_duration_changed.new_duration = i_duration } );
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

    vlc_atomic_rc_inc( &owner->rc );
    return p_item;
}

void input_item_Release( input_item_t *p_item )
{
    input_item_owner_t *owner = item_owner(p_item);

    if( !vlc_atomic_rc_dec( &owner->rc ) )
        return;

    vlc_event_manager_fini( &p_item->event_manager );

    free( p_item->psz_name );
    free( p_item->psz_uri );
    free( p_item->p_stats );

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

    for( int i = 0; i < p_item->i_slaves; i++ )
        input_item_slave_Delete( p_item->pp_slaves[i] );
    TAB_CLEAN( p_item->i_slaves, p_item->pp_slaves );

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

    char* psz_option_dup = strdup( psz_option );
    if( unlikely( !psz_option_dup ) )
    {
        err = VLC_ENOMEM;
        goto out;
    }

    TAB_APPEND(p_input->i_options, p_input->ppsz_options, psz_option_dup);

    flagv[p_input->optflagc++] = flags;

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

static int bsearch_strcmp_cb(const void *a, const void *b)
{
    const char *const *entry = b;
    return strcasecmp(a, *entry);
}

static bool input_item_IsMaster(const char *psz_filename)
{
    static const char *const ppsz_master_exts[] = { MASTER_EXTENSIONS };

    const char *psz_ext = strrchr(psz_filename, '.');
    if (psz_ext == NULL || *(++psz_ext) == '\0')
        return false;

    return bsearch(psz_ext, ppsz_master_exts, ARRAY_SIZE(ppsz_master_exts),
                   sizeof(const char *), bsearch_strcmp_cb) != NULL;
}

bool input_item_slave_GetType(const char *psz_filename,
                              enum slave_type *p_slave_type)
{
    static const char *const ppsz_sub_exts[] = { SLAVE_SPU_EXTENSIONS };
    static const char *const ppsz_audio_exts[] = { SLAVE_AUDIO_EXTENSIONS };

    static struct {
        enum slave_type i_type;
        const char *const *ppsz_exts;
        size_t nmemb;
    } p_slave_list[] = {
        { SLAVE_TYPE_SPU, ppsz_sub_exts, ARRAY_SIZE(ppsz_sub_exts) },
        { SLAVE_TYPE_AUDIO, ppsz_audio_exts, ARRAY_SIZE(ppsz_audio_exts) },
    };

    const char *psz_ext = strrchr(psz_filename, '.');
    if (psz_ext == NULL || *(++psz_ext) == '\0')
        return false;

    for (unsigned int i = 0; i < sizeof(p_slave_list) / sizeof(*p_slave_list); ++i)
    {
        if (bsearch(psz_ext, p_slave_list[i].ppsz_exts, p_slave_list[i].nmemb,
                    sizeof(const char *), bsearch_strcmp_cb))
        {
            *p_slave_type = p_slave_list[i].i_type;
            return true;
        }
    }
    return false;
}

input_item_slave_t *input_item_slave_New(const char *psz_uri, enum slave_type i_type,
                                       enum slave_priority i_priority)
{
    if( !psz_uri )
        return NULL;

    input_item_slave_t *p_slave = malloc( sizeof( *p_slave ) + strlen( psz_uri ) + 1 );
    if( !p_slave )
        return NULL;

    p_slave->i_type = i_type;
    p_slave->i_priority = i_priority;
    p_slave->b_forced = false;
    strcpy( p_slave->psz_uri, psz_uri );

    return p_slave;
}

int input_item_AddSlave(input_item_t *p_item, input_item_slave_t *p_slave)
{
    if( p_item == NULL || p_slave == NULL
     || p_slave->i_priority < SLAVE_PRIORITY_MATCH_NONE )
        return VLC_EGENERIC;

    vlc_mutex_lock( &p_item->lock );

    TAB_APPEND(p_item->i_slaves, p_item->pp_slaves, p_slave);

    vlc_mutex_unlock( &p_item->lock );
    return VLC_SUCCESS;
}

static info_category_t *InputItemFindCat( input_item_t *p_item,
                                          int *pi_index, const char *psz_cat )
{
    vlc_mutex_assert( &p_item->lock );
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
        info_t *p_info = info_category_FindInfo( p_cat, psz_name );
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
    vlc_mutex_assert( &p_i->lock );

    info_category_t *p_cat = InputItemFindCat( p_i, NULL, psz_cat );
    if( !p_cat )
    {
        p_cat = info_category_New( psz_cat );
        if( !p_cat )
            return VLC_ENOMEM;
        TAB_APPEND(p_i->i_categories, p_i->pp_categories, p_cat);
    }
    info_t *p_info = info_category_VaAddInfo( p_cat, psz_name, psz_format, args );
    if( !p_info || !p_info->psz_value )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
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
        vlc_event_send( &p_i->event_manager, &(vlc_event_t) {
            .type = vlc_InputItemInfoChanged } );

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
        TAB_ERASE(p_i->i_categories, p_i->pp_categories, i_cat);
    }
    vlc_mutex_unlock( &p_i->lock );

    vlc_event_send( &p_i->event_manager,
                    &(vlc_event_t) { .type = vlc_InputItemInfoChanged } );

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
        TAB_APPEND(p_item->i_categories, p_item->pp_categories, p_cat);
    vlc_mutex_unlock( &p_item->lock );

    vlc_event_send( &p_item->event_manager,
                    &(vlc_event_t) { .type = vlc_InputItemInfoChanged } );
}

void input_item_MergeInfos( input_item_t *p_item, info_category_t *p_cat )
{
    vlc_mutex_lock( &p_item->lock );
    info_category_t *p_old = InputItemFindCat( p_item, NULL, p_cat->psz_name );
    if( p_old )
    {
        info_t *info;

        info_foreach(info, &p_cat->infos)
            info_category_ReplaceInfo( p_old, info );
        vlc_list_init( &p_cat->infos );
        info_category_Delete( p_cat );
    }
    else
        TAB_APPEND(p_item->i_categories, p_item->pp_categories, p_cat);
    vlc_mutex_unlock( &p_item->lock );

    vlc_event_send( &p_item->event_manager,
                    &(vlc_event_t) { .type = vlc_InputItemInfoChanged } );
}

void input_item_SetEpgEvent( input_item_t *p_item, const vlc_epg_event_t *p_epg_evt )
{
    bool b_changed = false;
    vlc_mutex_lock( &p_item->lock );

    for( int i = 0; i < p_item->i_epg; i++ )
    {
        vlc_epg_t *p_epg = p_item->pp_epg[i];
        for( size_t j = 0; j < p_epg->i_event; j++ )
        {
            /* Same event can exist in more than one table */
            if( p_epg->pp_event[j]->i_id == p_epg_evt->i_id )
            {
                vlc_epg_event_t *p_dup = vlc_epg_event_Duplicate( p_epg_evt );
                if( p_dup )
                {
                    if( p_epg->p_current == p_epg->pp_event[j] )
                        p_epg->p_current = p_dup;
                    vlc_epg_event_Delete( p_epg->pp_event[j] );
                    p_epg->pp_event[j] = p_dup;
                    b_changed = true;
                }
                break;
            }
        }
    }
    vlc_mutex_unlock( &p_item->lock );

    if ( b_changed )
    {
        vlc_event_send( &p_item->event_manager,
                        &(vlc_event_t) { .type = vlc_InputItemInfoChanged } );
    }
}

//#define EPG_DEBUG
#ifdef EPG_DEBUG
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
#endif

void input_item_SetEpg( input_item_t *p_item, const vlc_epg_t *p_update, bool b_current_source )
{
    vlc_epg_t *p_epg = vlc_epg_Duplicate( p_update );
    if( !p_epg )
        return;

    vlc_mutex_lock( &p_item->lock );

    /* */
    vlc_epg_t **pp_epg = NULL;
    for( int i = 0; i < p_item->i_epg; i++ )
    {
        if( p_item->pp_epg[i]->i_source_id == p_update->i_source_id &&
            p_item->pp_epg[i]->i_id == p_update->i_id )
        {
            pp_epg = &p_item->pp_epg[i];
            break;
        }
    }

    /* replace with new version */
    if( pp_epg )
    {
        vlc_epg_Delete( *pp_epg );
        if( *pp_epg == p_item->p_epg_table ) /* current table can have changed */
            p_item->p_epg_table = NULL;
        *pp_epg = p_epg;
    }
    else
    {
        TAB_APPEND( p_item->i_epg, p_item->pp_epg, p_epg );
    }

    if( b_current_source && p_epg->b_present )
        p_item->p_epg_table = p_epg;

    vlc_mutex_unlock( &p_item->lock );

#ifdef EPG_DEBUG
    char *psz_epg;
    if( asprintf( &psz_epg, "EPG %s", p_epg->psz_name ? p_epg->psz_name : "unknown" ) < 0 )
        goto signal;

    input_item_DelInfo( p_item, psz_epg, NULL );

    vlc_mutex_lock( &p_item->lock );
    for( size_t i = 0; i < p_epg->i_event; i++ )
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
    vlc_event_send( &p_item->event_manager,
                    &(vlc_event_t){ .type = vlc_InputItemInfoChanged, } );
}

void input_item_ChangeEPGSource( input_item_t *p_item, int i_source_id )
{
    vlc_mutex_lock( &p_item->lock );
    p_item->p_epg_table = NULL;
    if( i_source_id > 0 )
    {
        /* Update pointer to current/next table in the full schedule */
        for( int i = 0; i < p_item->i_epg; i++ )
        {
            if( p_item->pp_epg[i]->i_source_id == i_source_id &&
                p_item->pp_epg[i]->b_present )
            {
                p_item->p_epg_table = p_item->pp_epg[i];
                break;
            }
        }
    }
    vlc_mutex_unlock( &p_item->lock );
}

void input_item_SetEpgTime( input_item_t *p_item, int64_t i_time )
{
    vlc_mutex_lock( &p_item->lock );
    p_item->i_epg_time = i_time;
    vlc_mutex_unlock( &p_item->lock );
}

void input_item_SetEpgOffline( input_item_t *p_item )
{
    input_item_ChangeEPGSource( p_item, -1 );

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

    vlc_event_send( &p_item->event_manager,
                    &(vlc_event_t) { .type = vlc_InputItemInfoChanged } );
}

input_item_t *
input_item_NewExt( const char *psz_uri, const char *psz_name,
                   vlc_tick_t duration, enum input_item_type_e type, enum input_item_net_type i_net )
{
    input_item_owner_t *owner = calloc( 1, sizeof( *owner ) );
    if( unlikely(owner == NULL) )
        return NULL;

    vlc_atomic_rc_init( &owner->rc );

    input_item_t *p_input = &owner->item;
    vlc_event_manager_t * p_em = &p_input->event_manager;

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
    TAB_INIT( p_input->i_slaves, p_input->pp_slaves );

    vlc_event_manager_init( p_em, p_input );

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

    if( likely(item != NULL) && p_input->i_slaves > 0 )
    {
        for( int i = 0; i < p_input->i_slaves; i++ )
        {
            input_item_slave_t* slave = input_item_slave_New(
                        p_input->pp_slaves[i]->psz_uri,
                        p_input->pp_slaves[i]->i_type,
                        p_input->pp_slaves[i]->i_priority);
            if( unlikely(slave != NULL) )
            {
                TAB_APPEND(item->i_slaves, item->pp_slaves, slave);
            }
        }
    }

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
    enum input_item_type_e i_type;
    bool       b_net;
};

static int typecmp( const void *key, const void *entry )
{
    const struct item_type_entry *type = entry;
    const char *uri = key, *scheme = type->psz_scheme;

    return strncmp( uri, scheme, strlen( scheme ) );
}

/* Guess the type of the item using the beginning of the mrl */
static enum input_item_type_e GuessType( const input_item_t *p_item, bool *p_net )
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
        { "gopher", ITEM_TYPE_STREAM, true },
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
        { "satip",  ITEM_TYPE_STREAM, true }, /* satellite over ip */
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
    input_item_Hold( p_input );

    p_node->i_children = 0;
    p_node->pp_children = NULL;

    return p_node;
}

void input_item_node_Delete( input_item_node_t *p_node )
{
    for( int i = 0; i < p_node->i_children; i++ )
        input_item_node_Delete( p_node->pp_children[i] );

    input_item_Release( p_node->p_item );
    free( p_node->pp_children );
    free( p_node );
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

void input_item_node_AppendNode( input_item_node_t *p_parent,
                                 input_item_node_t *p_child )
{
    assert(p_parent != NULL);
    assert(p_child != NULL);
    TAB_APPEND(p_parent->i_children, p_parent->pp_children, p_child);
}

void input_item_node_RemoveNode( input_item_node_t *parent,
                                 input_item_node_t *child )
{
    TAB_REMOVE(parent->i_children, parent->pp_children, child);
}

/* Called by es_out when a new Elementary Stream is added or updated. */
void input_item_UpdateTracksInfo(input_item_t *item, const es_format_t *fmt)
{
    int i;
    es_format_t *fmt_copy = malloc(sizeof *fmt_copy);
    if (!fmt_copy)
        return;

    es_format_Copy(fmt_copy, fmt);

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

char *input_item_CreateFilename(input_item_t *item,
                                const char *dir, const char *filenamefmt,
                                const char *ext)
{
    char *path;
    char *filename = str_format(NULL, item, filenamefmt);
    if (unlikely(filename == NULL))
        return NULL;

    filename_sanitize(filename);

    if (((ext != NULL)
            ? asprintf(&path, "%s"DIR_SEP"%s.%s", dir, filename, ext)
            : asprintf(&path, "%s"DIR_SEP"%s", dir, filename)) < 0)
        path = NULL;

    free(filename);
    return path;
}

struct input_item_parser_id_t
{
    input_thread_t *input;
    input_state_e state;
    const input_item_parser_cbs_t *cbs;
    void *userdata;
};

static void
input_item_parser_InputEvent(input_thread_t *input,
                             const struct vlc_input_event *event, void *parser_)
{
    input_item_parser_id_t *parser = parser_;

    switch (event->type)
    {
        case INPUT_EVENT_TIMES:
            input_item_SetDuration(input_GetItem(input), event->times.length);
            break;
        case INPUT_EVENT_STATE:
            parser->state = event->state.value;
            break;
        case INPUT_EVENT_DEAD:
        {
            int status = parser->state == END_S ? VLC_SUCCESS : VLC_EGENERIC;
            parser->cbs->on_ended(input_GetItem(input), status, parser->userdata);
            break;
        }
        case INPUT_EVENT_SUBITEMS:
            if (parser->cbs->on_subtree_added)
                parser->cbs->on_subtree_added(input_GetItem(input),
                                              event->subitems, parser->userdata);
            break;
        default:
            break;
    }
}

input_item_parser_id_t *
input_item_Parse(input_item_t *item, vlc_object_t *obj,
                 const input_item_parser_cbs_t *cbs, void *userdata)
{
    assert(cbs && cbs->on_ended);
    input_item_parser_id_t *parser = malloc(sizeof(*parser));
    if (!parser)
        return NULL;

    parser->state = INIT_S;
    parser->cbs = cbs;
    parser->userdata = userdata;
    parser->input = input_CreatePreparser(obj, input_item_parser_InputEvent,
                                          parser, item);
    if (!parser->input || input_Start(parser->input))
    {
        if (parser->input)
            input_Close(parser->input);
        free(parser);
        return NULL;
    }
    return parser;
}

void
input_item_parser_id_Interrupt(input_item_parser_id_t *parser)
{
    input_Stop(parser->input);
}

void
input_item_parser_id_Release(input_item_parser_id_t *parser)
{
    input_item_parser_id_Interrupt(parser);
    input_Close(parser->input);
    free(parser);
}

static int rdh_compar_type(input_item_t *p1, input_item_t *p2)
{
    if (p1->i_type != p2->i_type)
    {
        if (p1->i_type == ITEM_TYPE_DIRECTORY)
            return -1;
        if (p2->i_type == ITEM_TYPE_DIRECTORY)
            return 1;
    }
    return 0;
}

static int rdh_compar_filename(const void *a, const void *b)
{
    input_item_node_t *const *na = a, *const *nb = b;
    input_item_t *ia = (*na)->p_item, *ib = (*nb)->p_item;

    int i_ret = rdh_compar_type(ia, ib);
    if (i_ret != 0)
        return i_ret;

    return vlc_filenamecmp(ia->psz_name, ib->psz_name);
}

static void rdh_sort(input_item_node_t *p_node)
{
    if (p_node->i_children <= 0)
        return;

    /* Sort current node */
    qsort(p_node->pp_children, p_node->i_children,
          sizeof(input_item_node_t *), rdh_compar_filename);

    /* Sort all children */
    for (int i = 0; i < p_node->i_children; i++)
        rdh_sort(p_node->pp_children[i]);
}

/**
 * Does the provided file name has one of the extension provided ?
 */
static bool rdh_file_has_ext(const char *psz_filename,
                             const char *psz_ignored_exts)
{
    if (psz_ignored_exts == NULL)
        return false;

    const char *ext = strrchr(psz_filename, '.');
    if (ext == NULL)
        return false;

    size_t extlen = strlen(++ext);

    for (const char *type = psz_ignored_exts, *end; type[0]; type = end + 1)
    {
        end = strchr(type, ',');
        if (end == NULL)
            end = type + strlen(type);

        if (type + extlen == end && !strncasecmp(ext, type, extlen))
            return true;

        if (*end == '\0')
            break;
    }

    return false;
}

static bool rdh_file_is_ignored(struct vlc_readdir_helper *p_rdh,
                                const char *psz_filename)
{
    return (psz_filename[0] == '\0'
         || strcmp(psz_filename, ".") == 0
         || strcmp(psz_filename, "..") == 0
         || (!p_rdh->b_show_hiddenfiles && psz_filename[0] == '.')
         || rdh_file_has_ext(psz_filename, p_rdh->psz_ignored_exts));
}

struct rdh_slave
{
    input_item_slave_t *p_slave;
    char *psz_filename;
    input_item_node_t *p_node;
};

struct rdh_dir
{
    input_item_node_t *p_node;
    char psz_path[];
};

static char *rdh_name_from_filename(const char *psz_filename)
{
    /* remove leading white spaces */
    while (*psz_filename != '\0' && *psz_filename == ' ')
        psz_filename++;

    char *psz_name = strdup(psz_filename);
    if (!psz_name)
        return NULL;

    /* remove extension */
    char *psz_ptr = strrchr(psz_name, '.');
    if (psz_ptr && psz_ptr != psz_name)
        *psz_ptr = '\0';

    /* remove trailing white spaces */
    int i = strlen(psz_name) - 1;
    while (psz_name[i] == ' ' && i >= 0)
        psz_name[i--] = '\0';

    /* convert to lower case */
    psz_ptr = psz_name;
    while (*psz_ptr != '\0')
    {
        *psz_ptr = tolower(*psz_ptr);
        psz_ptr++;
    }

    return psz_name;
}

static uint8_t rdh_get_slave_priority(input_item_t *p_item,
                                      input_item_slave_t *p_slave,
                                      const char *psz_slave_filename)
{
    uint8_t i_priority = SLAVE_PRIORITY_MATCH_NONE;
    char *psz_item_name = rdh_name_from_filename(p_item->psz_name);
    char *psz_slave_name = rdh_name_from_filename(psz_slave_filename);

    if (!psz_item_name || !psz_slave_name)
        goto done;

    size_t i_item_len = strlen(psz_item_name);
    size_t i_slave_len = strlen(psz_slave_name);

    /* The slave name len should not be twice longer than the item name len. */
    if (i_item_len > i_slave_len || i_slave_len > 2 * i_item_len)
        goto done;

    /* check if the names match exactly */
    if (!strcmp(psz_item_name, psz_slave_name))
    {
        i_priority = SLAVE_PRIORITY_MATCH_ALL;
        goto done;
    }

    /* "cdg" slaves have to be a full match */
    if (p_slave->i_type == SLAVE_TYPE_SPU)
    {
        char *psz_ext = strrchr(psz_slave_name, '.');
        if (psz_ext != NULL && strcasecmp(++psz_ext, "cdg") == 0)
            goto done;
    }

    /* check if the item name is a substring of the slave name */
    const char *psz_sub = strstr(psz_slave_name, psz_item_name);

    if (psz_sub)
    {
        /* check if the item name was found at the end of the slave name */
        if (strlen(psz_sub + strlen(psz_item_name)) == 0)
        {
            i_priority = SLAVE_PRIORITY_MATCH_RIGHT;
            goto done;
        }
        else
        {
            i_priority = SLAVE_PRIORITY_MATCH_LEFT;
            goto done;
        }
    }

done:
    free(psz_item_name);
    free(psz_slave_name);
    return i_priority;
}

static int rdh_should_match_idx(struct vlc_readdir_helper *p_rdh,
                                struct rdh_slave *p_rdh_sub)
{
    char *psz_ext = strrchr(p_rdh_sub->psz_filename, '.');
    if (!psz_ext)
        return false;
    psz_ext++;

    if (strcasecmp(psz_ext, "sub") != 0)
        return false;

    for (size_t i = 0; i < p_rdh->i_slaves; i++)
    {
        struct rdh_slave *p_rdh_slave = p_rdh->pp_slaves[i];

        if (p_rdh_slave == NULL || p_rdh_slave == p_rdh_sub)
            continue;

        /* check that priorities match */
        if (p_rdh_slave->p_slave->i_priority !=
            p_rdh_sub->p_slave->i_priority)
            continue;

        /* check that the filenames without extension match */
        if (strncasecmp(p_rdh_sub->psz_filename, p_rdh_slave->psz_filename,
                        strlen(p_rdh_sub->psz_filename) - 3 ) != 0)
            continue;

        /* check that we have an idx file */
        char *psz_ext_idx = strrchr(p_rdh_slave->psz_filename, '.');
        if (psz_ext_idx == NULL)
            continue;
        psz_ext_idx++;
        if (strcasecmp(psz_ext_idx, "idx" ) == 0)
            return true;
    }
    return false;
}

static void rdh_attach_slaves(struct vlc_readdir_helper *p_rdh,
                              input_item_node_t *p_parent_node)
{
    if (p_rdh->i_sub_autodetect_fuzzy == 0)
        return;

    /* Try to match slaves for each items of the node */
    for (int i = 0; i < p_parent_node->i_children; i++)
    {
        input_item_node_t *p_node = p_parent_node->pp_children[i];
        input_item_t *p_item = p_node->p_item;

        enum slave_type unused;
        if (!input_item_IsMaster(p_item->psz_name)
         || input_item_slave_GetType(p_item->psz_name, &unused))
            continue; /* don't match 2 possible slaves between each others */

        for (size_t j = 0; j < p_rdh->i_slaves; j++)
        {
            struct rdh_slave *p_rdh_slave = p_rdh->pp_slaves[j];

            /* Don't try to match slaves with themselves or slaves already
             * attached with the higher priority */
            if (p_rdh_slave->p_node == p_node
             || p_rdh_slave->p_slave->i_priority == SLAVE_PRIORITY_MATCH_ALL)
                continue;

            uint8_t i_priority =
                rdh_get_slave_priority(p_item, p_rdh_slave->p_slave,
                                         p_rdh_slave->psz_filename);

            if (i_priority < p_rdh->i_sub_autodetect_fuzzy)
                continue;

            /* Drop the ".sub" slave if a ".idx" slave matches */
            if (p_rdh_slave->p_slave->i_type == SLAVE_TYPE_SPU
             && rdh_should_match_idx(p_rdh, p_rdh_slave))
                continue;

            input_item_slave_t *p_slave =
                input_item_slave_New(p_rdh_slave->p_slave->psz_uri,
                                     p_rdh_slave->p_slave->i_type,
                                     i_priority);
            if (p_slave == NULL)
                break;

            if (input_item_AddSlave(p_item, p_slave) != VLC_SUCCESS)
            {
                input_item_slave_Delete(p_slave);
                break;
            }

            /* Remove the corresponding node if any: This slave won't be
             * added in the parent node */
            if (p_rdh_slave->p_node != NULL)
            {
                input_item_node_RemoveNode(p_parent_node, p_rdh_slave->p_node);
                input_item_node_Delete(p_rdh_slave->p_node);
                p_rdh_slave->p_node = NULL;
            }

            p_rdh_slave->p_slave->i_priority = i_priority;
        }
    }

    /* Attach all children */
    for (int i = 0; i < p_parent_node->i_children; i++)
        rdh_attach_slaves(p_rdh, p_parent_node->pp_children[i]);
}

static int rdh_unflatten(struct vlc_readdir_helper *p_rdh,
                         input_item_node_t **pp_node, const char *psz_path,
                         int i_net)
{
    /* Create an input input for each sub folders that is contained in the full
     * path. Update pp_node to point to the direct parent of the future item to
     * add. */

    assert(psz_path != NULL);
    const char *psz_subpaths = psz_path;

    while ((psz_subpaths = strchr(psz_subpaths, '/')))
    {
        input_item_node_t *p_subnode = NULL;

        /* Check if this sub folder item was already added */
        for (size_t i = 0; i < p_rdh->i_dirs && p_subnode == NULL; i++)
        {
            struct rdh_dir *rdh_dir = p_rdh->pp_dirs[i];
            if (!strncmp(rdh_dir->psz_path, psz_path, psz_subpaths - psz_path))
                p_subnode = rdh_dir->p_node;
        }

        /* The sub folder item doesn't exist, so create it */
        if (p_subnode == NULL)
        {
            size_t i_sub_path_len = psz_subpaths - psz_path;
            struct rdh_dir *p_rdh_dir =
                malloc(sizeof(struct rdh_dir) + 1 + i_sub_path_len);
            if (p_rdh_dir == NULL)
                return VLC_ENOMEM;
            strncpy(p_rdh_dir->psz_path, psz_path, i_sub_path_len);
            p_rdh_dir->psz_path[i_sub_path_len] = 0;

            const char *psz_subpathname = strrchr(p_rdh_dir->psz_path, '/');
            if (psz_subpathname != NULL)
                ++psz_subpathname;
            else
                psz_subpathname = p_rdh_dir->psz_path;

            input_item_t *p_item =
                input_item_NewExt(INPUT_ITEM_URI_NOP, psz_subpathname, INPUT_DURATION_UNSET,
                                  ITEM_TYPE_DIRECTORY, i_net);
            if (p_item == NULL)
            {
                free(p_rdh_dir);
                return VLC_ENOMEM;
            }
            input_item_CopyOptions(p_item, (*pp_node)->p_item);
            *pp_node = input_item_node_AppendItem(*pp_node, p_item);
            input_item_Release(p_item);
            if (*pp_node == NULL)
            {
                free(p_rdh_dir);
                return VLC_ENOMEM;
            }
            p_rdh_dir->p_node = *pp_node;
            TAB_APPEND(p_rdh->i_dirs, p_rdh->pp_dirs, p_rdh_dir);
        }
        else
            *pp_node = p_subnode;
        psz_subpaths++;
    }
    return VLC_SUCCESS;
}

#undef vlc_readdir_helper_init
void vlc_readdir_helper_init(struct vlc_readdir_helper *p_rdh,
                             vlc_object_t *p_obj, input_item_node_t *p_node)
{
    /* Read options from the parent item. This allows vlc_stream_ReadDir()
     * users to specify options without affecting any exisitng vlc_object_t.
     * Apply options on a temporary object in order to not apply options (which
     * can be insecure) to the current object. */
    vlc_object_t *p_var_obj = vlc_object_create(p_obj, sizeof(vlc_object_t));
    if (p_var_obj != NULL)
    {
        input_item_ApplyOptions(p_var_obj, p_node->p_item);
        p_obj = p_var_obj;
    }

    p_rdh->p_node = p_node;
    p_rdh->b_show_hiddenfiles = var_InheritBool(p_obj, "show-hiddenfiles");
    p_rdh->psz_ignored_exts = var_InheritString(p_obj, "ignore-filetypes");
    bool b_autodetect = var_InheritBool(p_obj, "sub-autodetect-file");
    p_rdh->i_sub_autodetect_fuzzy = !b_autodetect ? 0 :
        var_InheritInteger(p_obj, "sub-autodetect-fuzzy");
    p_rdh->b_flatten = var_InheritBool(p_obj, "extractor-flatten");
    TAB_INIT(p_rdh->i_slaves, p_rdh->pp_slaves);
    TAB_INIT(p_rdh->i_dirs, p_rdh->pp_dirs);

    if (p_var_obj != NULL)
        vlc_object_delete(p_var_obj);
}

void vlc_readdir_helper_finish(struct vlc_readdir_helper *p_rdh, bool b_success)
{
    if (b_success)
    {
        rdh_sort(p_rdh->p_node);
        rdh_attach_slaves(p_rdh, p_rdh->p_node);
    }
    free(p_rdh->psz_ignored_exts);

    /* Remove unmatched slaves */
    for (size_t i = 0; i < p_rdh->i_slaves; i++)
    {
        struct rdh_slave *p_rdh_slave = p_rdh->pp_slaves[i];
        if (p_rdh_slave != NULL)
        {
            input_item_slave_Delete(p_rdh_slave->p_slave);
            free(p_rdh_slave->psz_filename);
            free(p_rdh_slave);
        }
    }
    TAB_CLEAN(p_rdh->i_slaves, p_rdh->pp_slaves);

    for (size_t i = 0; i < p_rdh->i_dirs; i++)
        free(p_rdh->pp_dirs[i]);
    TAB_CLEAN(p_rdh->i_dirs, p_rdh->pp_dirs);
}

int vlc_readdir_helper_additem(struct vlc_readdir_helper *p_rdh,
                               const char *psz_uri, const char *psz_flatpath,
                               const char *psz_filename, int i_type, int i_net)
{
    enum slave_type i_slave_type;
    struct rdh_slave *p_rdh_slave = NULL;
    assert(psz_flatpath || psz_filename);

    if (!p_rdh->b_flatten)
    {
        if (psz_filename == NULL)
        {
            psz_filename = strrchr(psz_flatpath, '/');
            if (psz_filename != NULL)
                ++psz_filename;
            else
                psz_filename = psz_flatpath;
        }
    }
    else
    {
        if (psz_filename == NULL)
            psz_filename = psz_flatpath;
        psz_flatpath = NULL;
    }

    if (p_rdh->i_sub_autodetect_fuzzy != 0
     && input_item_slave_GetType(psz_filename, &i_slave_type))
    {
        p_rdh_slave = malloc(sizeof(*p_rdh_slave));
        if (!p_rdh_slave)
            return VLC_ENOMEM;

        p_rdh_slave->p_node = NULL;
        p_rdh_slave->psz_filename = strdup(psz_filename);
        p_rdh_slave->p_slave = input_item_slave_New(psz_uri, i_slave_type,
                                                      SLAVE_PRIORITY_MATCH_NONE);
        if (!p_rdh_slave->p_slave || !p_rdh_slave->psz_filename)
        {
            free(p_rdh_slave->psz_filename);
            free(p_rdh_slave);
            return VLC_ENOMEM;
        }

        TAB_APPEND(p_rdh->i_slaves, p_rdh->pp_slaves, p_rdh_slave);
    }

    if (rdh_file_is_ignored(p_rdh, psz_filename))
        return VLC_SUCCESS;

    input_item_node_t *p_node = p_rdh->p_node;

    if (psz_flatpath != NULL)
    {
        int i_ret = rdh_unflatten(p_rdh, &p_node, psz_flatpath, i_net);
        if (i_ret != VLC_SUCCESS)
            return i_ret;
    }

    input_item_t *p_item = input_item_NewExt(psz_uri, psz_filename, INPUT_DURATION_UNSET, i_type,
                                             i_net);
    if (p_item == NULL)
        return VLC_ENOMEM;

    input_item_CopyOptions(p_item, p_node->p_item);
    p_node = input_item_node_AppendItem(p_node, p_item);
    input_item_Release(p_item);
    if (p_node == NULL)
        return VLC_ENOMEM;

    /* A slave can also be an item. If there is a match, this item will be
     * removed from the parent node. This is not a common case, since most
     * slaves will be ignored by rdh_file_is_ignored() */
    if (p_rdh_slave != NULL)
        p_rdh_slave->p_node = p_node;
    return VLC_SUCCESS;
}
