/*****************************************************************************
 * filter_chain.c : Handle chains of filter_t objects.
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * Copyright (C) 2008-2014 Rémi Denis-Courmont
 *
 * Author: Antoine Cellerier <dionoea at videolan dot org>
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

#include <vlc_filter.h>
#include <vlc_modules.h>
#include <vlc_mouse.h>
#include <vlc_spu.h>
#include <libvlc.h>
#include <assert.h>

typedef struct chained_filter_t
{
    /* Public part of the filter structure */
    filter_t filter;
    /* Private filter chain data (shhhh!) */
    struct chained_filter_t *prev, *next;
    vlc_mouse_t *mouse;
    picture_t *pending;
} chained_filter_t;

/* Only use this with filter objects from _this_ C module */
static inline chained_filter_t *chained(filter_t *filter)
{
    return container_of(filter, chained_filter_t, filter);
}

/* */
struct filter_chain_t
{
    filter_owner_t callbacks; /**< Inner callbacks */
    filter_owner_t owner; /**< Owner (downstream) callbacks */

    chained_filter_t *first, *last; /**< List of filters */

    es_format_t fmt_in; /**< Chain input format (constant) */
    es_format_t fmt_out; /**< Chain current output format */
    bool b_allow_fmt_out_change; /**< Can the output format be changed? */
    const char *filter_cap; /**< Filter modules capability */
    const char *conv_cap; /**< Converter modules capability */
};

/**
 * Local prototypes
 */
static void FilterDeletePictures( picture_t * );

static filter_chain_t *filter_chain_NewInner( const filter_owner_t *callbacks,
    const char *cap, const char *conv_cap, bool fmt_out_change,
    const filter_owner_t *owner, enum es_format_category_e cat )
{
    assert( callbacks != NULL && callbacks->sys != NULL );
    assert( cap != NULL );

    filter_chain_t *chain = malloc( sizeof (*chain) );
    if( unlikely(chain == NULL) )
        return NULL;

    chain->callbacks = *callbacks;
    if( owner != NULL )
        chain->owner = *owner;
    chain->first = NULL;
    chain->last = NULL;
    es_format_Init( &chain->fmt_in, cat, 0 );
    es_format_Init( &chain->fmt_out, cat, 0 );
    chain->b_allow_fmt_out_change = fmt_out_change;
    chain->filter_cap = cap;
    chain->conv_cap = conv_cap;
    return chain;
}

#undef filter_chain_New
/**
 * Filter chain initialisation
 */
filter_chain_t *filter_chain_New( vlc_object_t *obj, const char *cap,
                                  enum es_format_category_e cat )
{
    filter_owner_t callbacks = {
        .sys = obj,
    };

    return filter_chain_NewInner( &callbacks, cap, NULL, false, NULL, cat );
}

/** Chained filter picture allocator function */
static picture_t *filter_chain_VideoBufferNew( filter_t *filter )
{
    if( chained(filter)->next != NULL )
    {
        picture_t *pic = picture_NewFromFormat( &filter->fmt_out.video );
        if( pic == NULL )
            msg_Err( filter, "Failed to allocate picture" );
        return pic;
    }
    else
    {
        filter_chain_t *chain = filter->owner.sys;

        /* XXX ugly */
        filter->owner.sys = chain->owner.sys;
        picture_t *pic = chain->owner.video.buffer_new( filter );
        filter->owner.sys = chain;
        return pic;
    }
}

#undef filter_chain_NewVideo
filter_chain_t *filter_chain_NewVideo( vlc_object_t *obj, bool allow_change,
                                       const filter_owner_t *restrict owner )
{
    filter_owner_t callbacks = {
        .sys = obj,
        .video = {
            .buffer_new = filter_chain_VideoBufferNew,
        },
    };

    return filter_chain_NewInner( &callbacks, "video filter",
                                  "video converter", allow_change, owner, VIDEO_ES );
}

/**
 * Filter chain destruction
 */
void filter_chain_Delete( filter_chain_t *p_chain )
{
    while( p_chain->first != NULL )
        filter_chain_DeleteFilter( p_chain, &p_chain->first->filter );

    es_format_Clean( &p_chain->fmt_in );
    es_format_Clean( &p_chain->fmt_out );

    free( p_chain );
}
/**
 * Filter chain reinitialisation
 */
void filter_chain_Reset( filter_chain_t *p_chain, const es_format_t *p_fmt_in,
                         const es_format_t *p_fmt_out )
{
    while( p_chain->first != NULL )
        filter_chain_DeleteFilter( p_chain, &p_chain->first->filter );

    if( p_fmt_in )
    {
        es_format_Clean( &p_chain->fmt_in );
        es_format_Copy( &p_chain->fmt_in, p_fmt_in );
    }
    if( p_fmt_out )
    {
        es_format_Clean( &p_chain->fmt_out );
        es_format_Copy( &p_chain->fmt_out, p_fmt_out );
    }
}

static filter_t *filter_chain_AppendInner( filter_chain_t *chain,
    const char *name, const char *capability, config_chain_t *cfg,
    const es_format_t *fmt_in, const es_format_t *fmt_out )
{
    vlc_object_t *parent = chain->callbacks.sys;
    chained_filter_t *chained =
        vlc_custom_create( parent, sizeof(*chained), "filter" );
    if( unlikely(chained == NULL) )
        return NULL;

    filter_t *filter = &chained->filter;

    if( fmt_in == NULL )
    {
        if( chain->last != NULL )
            fmt_in = &chain->last->filter.fmt_out;
        else
            fmt_in = &chain->fmt_in;
    }

    if( fmt_out == NULL )
        fmt_out = &chain->fmt_out;

    es_format_Copy( &filter->fmt_in, fmt_in );
    es_format_Copy( &filter->fmt_out, fmt_out );
    filter->b_allow_fmt_out_change = chain->b_allow_fmt_out_change;
    filter->p_cfg = cfg;
    filter->psz_name = name;

    filter->owner = chain->callbacks;
    filter->owner.sys = chain;

    assert( capability != NULL );
    if( name != NULL && filter->b_allow_fmt_out_change )
    {
        /* Append the "chain" video filter to the current list.
         * This filter will be used if the requested filter fails to load.
         * It will then try to add a video converter before. */
        char name_chained[strlen(name) + sizeof(",chain")];
        sprintf( name_chained, "%s,chain", name );
        filter->p_module = module_need( filter, capability, name_chained, true );
    }
    else
        filter->p_module = module_need( filter, capability, name, name != NULL );

    if( filter->p_module == NULL )
        goto error;

    if( filter->b_allow_fmt_out_change )
    {
        es_format_Clean( &chain->fmt_out );
        es_format_Copy( &chain->fmt_out, &filter->fmt_out );
    }

    if( chain->last == NULL )
    {
        assert( chain->first == NULL );
        chain->first = chained;
    }
    else
        chain->last->next = chained;
    chained->prev = chain->last;
    chain->last = chained;
    chained->next = NULL;

    vlc_mouse_t *mouse = malloc( sizeof(*mouse) );
    if( likely(mouse != NULL) )
        vlc_mouse_Init( mouse );
    chained->mouse = mouse;
    chained->pending = NULL;

    msg_Dbg( parent, "Filter '%s' (%p) appended to chain",
             (name != NULL) ? name : module_get_name(filter->p_module, false),
             (void *)filter );
    return filter;

error:
    if( name != NULL )
        msg_Err( parent, "Failed to create %s '%s'", capability, name );
    else
        msg_Err( parent, "Failed to create %s", capability );
    es_format_Clean( &filter->fmt_out );
    es_format_Clean( &filter->fmt_in );
    vlc_object_release( filter );
    return NULL;
}

filter_t *filter_chain_AppendFilter( filter_chain_t *chain,
    const char *name, config_chain_t *cfg,
    const es_format_t *fmt_in, const es_format_t *fmt_out )
{
    return filter_chain_AppendInner( chain, name, chain->filter_cap, cfg,
                                     fmt_in, fmt_out );
}

int filter_chain_AppendConverter( filter_chain_t *chain,
    const es_format_t *fmt_in, const es_format_t *fmt_out )
{
    return filter_chain_AppendInner( chain, NULL, chain->conv_cap, NULL,
                                     fmt_in, fmt_out ) != NULL ? 0 : -1;
}

void filter_chain_DeleteFilter( filter_chain_t *chain, filter_t *filter )
{
    vlc_object_t *obj = chain->callbacks.sys;
    chained_filter_t *chained = (chained_filter_t *)filter;

    /* Remove it from the chain */
    if( chained->prev != NULL )
        chained->prev->next = chained->next;
    else
    {
        assert( chained == chain->first );
        chain->first = chained->next;
    }

    if( chained->next != NULL )
        chained->next->prev = chained->prev;
    else
    {
        assert( chained == chain->last );
        chain->last = chained->prev;
    }

    module_unneed( filter, filter->p_module );

    msg_Dbg( obj, "Filter %p removed from chain", (void *)filter );
    FilterDeletePictures( chained->pending );

    free( chained->mouse );
    es_format_Clean( &filter->fmt_out );
    es_format_Clean( &filter->fmt_in );

    vlc_object_release( filter );
    /* FIXME: check fmt_in/fmt_out consitency */
}


int filter_chain_AppendFromString( filter_chain_t *chain, const char *str )
{
    vlc_object_t *obj = chain->callbacks.sys;
    char *buf = NULL;
    int ret = 0;

    while( str != NULL && str[0] != '\0' )
    {
        config_chain_t *cfg;
        char *name;

        char *next = config_ChainCreate( &name, &cfg, str );

        str = next;
        free( buf );
        buf = next;

        filter_t *filter = filter_chain_AppendFilter( chain, name, cfg,
                                                      NULL, NULL );
        if( cfg )
            config_ChainDestroy( cfg );

        if( filter == NULL )
        {
            msg_Err( obj, "Failed to append '%s' to chain", name );
            free( name );
            goto error;
        }

        free( name );
        ret++;
    }

    free( buf );
    return ret;

error:
    while( ret > 0 ) /* Unwind */
    {
        filter_chain_DeleteFilter( chain, &chain->last->filter );
        ret--;
    }
    free( buf );
    return VLC_EGENERIC;
}

int filter_chain_ForEach( filter_chain_t *chain,
                          int (*cb)( filter_t *, void * ), void *opaque )
{
    for( chained_filter_t *f = chain->first; f != NULL; f = f->next )
    {
        int ret = cb( &f->filter, opaque );
        if( ret )
            return ret;
    }
    return VLC_SUCCESS;
}

bool filter_chain_IsEmpty(const filter_chain_t *chain)
{
    return chain->first == NULL;
}

const es_format_t *filter_chain_GetFmtOut( filter_chain_t *p_chain )
{

    if( p_chain->b_allow_fmt_out_change )
        return &p_chain->fmt_out;

    if( p_chain->last != NULL )
        return &p_chain->last->filter.fmt_out;

    /* Unless filter_chain_Reset has been called we are doomed */
    return &p_chain->fmt_out;
}

static picture_t *FilterChainVideoFilter( chained_filter_t *f, picture_t *p_pic )
{
    for( ; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;
        p_pic = p_filter->pf_video_filter( p_filter, p_pic );
        if( !p_pic )
            break;
        if( f->pending )
        {
            msg_Warn( p_filter, "dropping pictures" );
            FilterDeletePictures( f->pending );
        }
        f->pending = p_pic->p_next;
        p_pic->p_next = NULL;
    }
    return p_pic;
}

picture_t *filter_chain_VideoFilter( filter_chain_t *p_chain, picture_t *p_pic )
{
    if( p_pic )
    {
        p_pic = FilterChainVideoFilter( p_chain->first, p_pic );
        if( p_pic )
            return p_pic;
    }
    for( chained_filter_t *b = p_chain->last; b != NULL; b = b->prev )
    {
        p_pic = b->pending;
        if( !p_pic )
            continue;
        b->pending = p_pic->p_next;
        p_pic->p_next = NULL;

        p_pic = FilterChainVideoFilter( b->next, p_pic );
        if( p_pic )
            return p_pic;
    }
    return NULL;
}

void filter_chain_VideoFlush( filter_chain_t *p_chain )
{
    for( chained_filter_t *f = p_chain->first; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;

        FilterDeletePictures( f->pending );
        f->pending = NULL;

        filter_Flush( p_filter );
    }
}

void filter_chain_SubSource( filter_chain_t *p_chain, spu_t *spu,
                             mtime_t display_date )
{
    for( chained_filter_t *f = p_chain->first; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;
        subpicture_t *p_subpic = p_filter->pf_sub_source( p_filter, display_date );
        if( p_subpic )
            spu_PutSubpicture( spu, p_subpic );
    }
}

subpicture_t *filter_chain_SubFilter( filter_chain_t *p_chain, subpicture_t *p_subpic )
{
    for( chained_filter_t *f = p_chain->first; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;

        p_subpic = p_filter->pf_sub_filter( p_filter, p_subpic );

        if( !p_subpic )
            break;
    }
    return p_subpic;
}

int filter_chain_MouseFilter( filter_chain_t *p_chain, vlc_mouse_t *p_dst, const vlc_mouse_t *p_src )
{
    vlc_mouse_t current = *p_src;

    for( chained_filter_t *f = p_chain->last; f != NULL; f = f->prev )
    {
        filter_t *p_filter = &f->filter;
        vlc_mouse_t *p_mouse = f->mouse;

        if( p_filter->pf_video_mouse && p_mouse )
        {
            vlc_mouse_t old = *p_mouse;
            vlc_mouse_t filtered;

            *p_mouse = current;
            if( p_filter->pf_video_mouse( p_filter, &filtered, &old, &current ) )
                return VLC_EGENERIC;
            current = filtered;
        }
    }

    *p_dst = current;
    return VLC_SUCCESS;
}

int filter_chain_MouseEvent( filter_chain_t *p_chain,
                             const vlc_mouse_t *p_mouse,
                             const video_format_t *p_fmt )
{
    for( chained_filter_t *f = p_chain->first; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;

        if( p_filter->pf_sub_mouse )
        {
            vlc_mouse_t old = *f->mouse;
            *f->mouse = *p_mouse;
            if( p_filter->pf_sub_mouse( p_filter, &old, p_mouse, p_fmt ) )
                return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

/* Helpers */
static void FilterDeletePictures( picture_t *picture )
{
    while( picture )
    {
        picture_t *next = picture->p_next;
        picture_Release( picture );
        picture = next;
    }
}
