/*****************************************************************************
 * filter_chain.c : Handle chains of filter_t objects.
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Author: Antoine Cellerier <dionoea at videolan dot org>
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

#include <vlc_filter.h>
#include <vlc_arrays.h>
#include <vlc_osd.h>
#include <libvlc.h>

typedef struct
{
    int (*pf_init)( filter_t *, void *p_data ); /* Callback called once filter allocation has succeeded to initialize the filter's buffer allocation callbacks. This function is responsible for setting p_owner if needed. */
    void (* pf_clean)( filter_t * ); /* Callback called on filter removal from chain to clean up buffer allocation callbacks data (ie p_owner) */
    void *p_data; /* Data for pf_buffer_allocation_init */

} filter_chain_allocator_t;

static int  AllocatorInit( const filter_chain_allocator_t *, filter_t * );
static void AllocatorClean( const filter_chain_allocator_t *, filter_t * );

static bool IsInternalVideoAllocator( filter_t * );

static int  InternalVideoInit( filter_t *, void * );
static void InternalVideoClean( filter_t * );

static const filter_chain_allocator_t internal_video_allocator = {
    .pf_init = InternalVideoInit,
    .pf_clean = InternalVideoClean,
    .p_data = NULL,
};

/* */
struct filter_chain_t
{
    /* Parent object */
    vlc_object_t *p_this;

    /* List of filters */
    vlc_array_t filters;
    vlc_array_t mouses;

    /* Capability of all the filters in the chain */
    char *psz_capability;

    /* Input format (read only) */
    es_format_t fmt_in;

    /* allow changing fmt_out if true */
    bool b_allow_fmt_out_change;

    /* Output format (writable depending on ... */
    es_format_t fmt_out;

    /* User provided allocator */
    filter_chain_allocator_t allocator;
};

/**
 * Local prototypes
 */
static filter_t *filter_chain_AppendFilterInternal( filter_chain_t *,
                                                    const char *, config_chain_t *,
                                                    const es_format_t *, const es_format_t * );

static int filter_chain_AppendFromStringInternal( filter_chain_t *, const char * );

static int filter_chain_DeleteFilterInternal( filter_chain_t *, filter_t * );

static int UpdateBufferFunctions( filter_chain_t * );

/**
 * Filter chain initialisation
 */
filter_chain_t *__filter_chain_New( vlc_object_t *p_this,
                                    const char *psz_capability,
                                    bool b_allow_fmt_out_change,
                                    int  (*pf_buffer_allocation_init)( filter_t *, void * ),
                                    void (*pf_buffer_allocation_clean)( filter_t * ),
                                    void *p_buffer_allocation_data )
{
    filter_chain_t *p_chain = malloc( sizeof( *p_chain ) );
    if( !p_chain )
        return NULL;

    p_chain->p_this = p_this;
    vlc_array_init( &p_chain->filters );
    vlc_array_init( &p_chain->mouses );
    p_chain->psz_capability = strdup( psz_capability );
    if( !p_chain->psz_capability )
    {
        vlc_array_clear( &p_chain->mouses );
        vlc_array_clear( &p_chain->filters );
        free( p_chain );
        return NULL;
    }
    es_format_Init( &p_chain->fmt_in, UNKNOWN_ES, 0 );
    es_format_Init( &p_chain->fmt_out, UNKNOWN_ES, 0 );
    p_chain->b_allow_fmt_out_change = b_allow_fmt_out_change;

    p_chain->allocator.pf_init = pf_buffer_allocation_init;
    p_chain->allocator.pf_clean = pf_buffer_allocation_clean;
    p_chain->allocator.p_data = p_buffer_allocation_data;

    return p_chain;
}

/**
 * Filter chain destruction
 */
void filter_chain_Delete( filter_chain_t *p_chain )
{
    filter_chain_Reset( p_chain, NULL, NULL );

    es_format_Clean( &p_chain->fmt_in );
    es_format_Clean( &p_chain->fmt_out );

    free( p_chain->psz_capability );
    vlc_array_clear( &p_chain->mouses );
    vlc_array_clear( &p_chain->filters );
    free( p_chain );
}
/**
 * Filter chain reinitialisation
 */
void filter_chain_Reset( filter_chain_t *p_chain, const es_format_t *p_fmt_in,
                         const es_format_t *p_fmt_out )
{
    while( vlc_array_count( &p_chain->filters ) > 0 )
    {
        filter_t *p_filter = vlc_array_item_at_index( &p_chain->filters, 0 );

        filter_chain_DeleteFilterInternal( p_chain, p_filter );
    }
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

filter_t *filter_chain_AppendFilter( filter_chain_t *p_chain,
                                     const char *psz_name,
                                     config_chain_t *p_cfg,
                                     const es_format_t *p_fmt_in,
                                     const es_format_t *p_fmt_out )
{
    filter_t *p_filter = filter_chain_AppendFilterInternal( p_chain, psz_name,
                                                            p_cfg, p_fmt_in,
                                                            p_fmt_out );
    if( UpdateBufferFunctions( p_chain ) < 0 )
        msg_Err( p_filter, "Woah! This doesn't look good." );
    return p_filter;
}

int filter_chain_AppendFromString( filter_chain_t *p_chain,
                                   const char *psz_string )
{
    const int i_ret = filter_chain_AppendFromStringInternal( p_chain, psz_string );
    if( i_ret < 0 )
        return i_ret;

    /* FIXME That one seems bad if a error is returned */
    return UpdateBufferFunctions( p_chain );
}

int filter_chain_DeleteFilter( filter_chain_t *p_chain, filter_t *p_filter )
{
    const int i_ret = filter_chain_DeleteFilterInternal( p_chain, p_filter );
    if( i_ret < 0 )
        return i_ret;

    /* FIXME That one seems bad if a error is returned */
    return UpdateBufferFunctions( p_chain );
}

int filter_chain_GetLength( filter_chain_t *p_chain )
{
    return vlc_array_count( &p_chain->filters );
}

const es_format_t *filter_chain_GetFmtOut( filter_chain_t *p_chain )
{

    if( p_chain->b_allow_fmt_out_change )
        return &p_chain->fmt_out;

    const int i_count = vlc_array_count( &p_chain->filters );
    if( i_count > 0 )
    {
        filter_t *p_last = vlc_array_item_at_index( &p_chain->filters, i_count - 1 );
        return &p_last->fmt_out;
    }

    /* Unless filter_chain_Reset has been called we are doomed */
    return &p_chain->fmt_out;
}

picture_t *filter_chain_VideoFilter( filter_chain_t *p_chain, picture_t *p_pic )
{
    for( int i = 0; i < vlc_array_count( &p_chain->filters ); i++ )
    {
        filter_t *p_filter = vlc_array_item_at_index( &p_chain->filters, i );

        p_pic = p_filter->pf_video_filter( p_filter, p_pic );
        if( !p_pic )
            break;
    }
    return p_pic;
}

block_t *filter_chain_AudioFilter( filter_chain_t *p_chain, block_t *p_block )
{
    for( int i = 0; i < vlc_array_count( &p_chain->filters ); i++ )
    {
        filter_t *p_filter = vlc_array_item_at_index( &p_chain->filters, i );

        p_block = p_filter->pf_audio_filter( p_filter, p_block );
        if( !p_block )
            break;
    }
    return p_block;
}

void filter_chain_SubFilter( filter_chain_t *p_chain,
                             mtime_t display_date )
{
    for( int i = 0; i < vlc_array_count( &p_chain->filters ); i++ )
    {
        filter_t *p_filter = vlc_array_item_at_index( &p_chain->filters, i );

        subpicture_t *p_subpic = p_filter->pf_sub_filter( p_filter, display_date );
        /* XXX I find that spu_t cast ugly */
        if( p_subpic )
            spu_DisplaySubpicture( (spu_t*)p_chain->p_this, p_subpic );
    }
}

int filter_chain_MouseFilter( filter_chain_t *p_chain, vlc_mouse_t *p_dst, const vlc_mouse_t *p_src )
{
    vlc_mouse_t current = *p_src;

    for( int i = vlc_array_count( &p_chain->filters ) - 1; i >= 0; i-- )
    {
        filter_t *p_filter = vlc_array_item_at_index( &p_chain->filters, i );
        vlc_mouse_t *p_mouse = vlc_array_item_at_index( &p_chain->mouses, i );

        if( p_filter->pf_mouse && p_mouse )
        {
            vlc_mouse_t old = *p_mouse;
            vlc_mouse_t filtered;

            *p_mouse = current;
            if( p_filter->pf_mouse( p_filter, &filtered, &old, &current ) )
                return VLC_EGENERIC;
            current = filtered;
        }
    }

    *p_dst = current;
    return VLC_SUCCESS;
}


/* Helpers */
static filter_t *filter_chain_AppendFilterInternal( filter_chain_t *p_chain,
                                                    const char *psz_name,
                                                    config_chain_t *p_cfg,
                                                    const es_format_t *p_fmt_in,
                                                    const es_format_t *p_fmt_out )
{
    filter_t *p_filter = vlc_custom_create( p_chain->p_this, sizeof(*p_filter),
                                            VLC_OBJECT_GENERIC, "filter" );
    if( !p_filter )
        return NULL;
    vlc_object_set_name( p_filter, psz_name );
    vlc_object_attach( p_filter, p_chain->p_this );

    if( !p_fmt_in )
    {
        p_fmt_in = &p_chain->fmt_in;

        const int i_count = vlc_array_count( &p_chain->filters );
        if( i_count > 0 )
        {
            filter_t *p_last = vlc_array_item_at_index( &p_chain->filters, i_count - 1 );
            p_fmt_in = &p_last->fmt_out;
        }
    }

    if( !p_fmt_out )
    {
        p_fmt_out = &p_chain->fmt_out;
    }

    es_format_Copy( &p_filter->fmt_in, p_fmt_in );
    es_format_Copy( &p_filter->fmt_out, p_fmt_out );
    p_filter->p_cfg = p_cfg;
    p_filter->b_allow_fmt_out_change = p_chain->b_allow_fmt_out_change;

    p_filter->p_module = module_need( p_filter, p_chain->psz_capability,
                                      psz_name, psz_name != NULL );

    if( !p_filter->p_module )
        goto error;

    if( p_filter->b_allow_fmt_out_change )
    {
        es_format_Clean( &p_chain->fmt_out );
        es_format_Copy( &p_chain->fmt_out, &p_filter->fmt_out );
    }

    if( AllocatorInit( &p_chain->allocator, p_filter ) )
        goto error;

    vlc_array_append( &p_chain->filters, p_filter );

    vlc_mouse_t *p_mouse = malloc( sizeof(*p_mouse) );
    if( p_mouse )
        vlc_mouse_Init( p_mouse );
    vlc_array_append( &p_chain->mouses, p_mouse );

    msg_Dbg( p_chain->p_this, "Filter '%s' (%p) appended to chain",
             psz_name ? psz_name : module_get_name(p_filter->p_module, false),
             p_filter );

    return p_filter;

error:
    if( psz_name )
        msg_Err( p_chain->p_this, "Failed to create %s '%s'",
                 p_chain->psz_capability, psz_name );
    else
        msg_Err( p_chain->p_this, "Failed to create %s",
                 p_chain->psz_capability );
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );
    es_format_Clean( &p_filter->fmt_in );
    es_format_Clean( &p_filter->fmt_out );
    vlc_object_detach( p_filter );
    vlc_object_release( p_filter );
    return NULL;
}


static int filter_chain_AppendFromStringInternal( filter_chain_t *p_chain,
                                                  const char *psz_string )
{
    config_chain_t *p_cfg = NULL;
    char *psz_name = NULL;
    char* psz_new_string;

    if( !psz_string || !*psz_string )
        return 0;

    psz_new_string = config_ChainCreate( &psz_name, &p_cfg, psz_string );

    filter_t *p_filter = filter_chain_AppendFilterInternal( p_chain, psz_name,
                                                            p_cfg, NULL, NULL );
    if( !p_filter )
    {
        msg_Err( p_chain->p_this, "Failed while trying to append '%s' "
                 "to filter chain", psz_name );
        free( psz_name );
        free( p_cfg );
        free( psz_new_string );
        return -1;
    }
    free( psz_name );

    const int i_ret = filter_chain_AppendFromStringInternal( p_chain, psz_new_string );
    free( psz_new_string );
    if( i_ret < 0 )
    {
        filter_chain_DeleteFilterInternal( p_chain, p_filter );
        return i_ret;
    }
    return 1 + i_ret;
}

static int filter_chain_DeleteFilterInternal( filter_chain_t *p_chain,
                                              filter_t *p_filter )
{
    const int i_filter_idx = vlc_array_index_of_item( &p_chain->filters, p_filter );
    if( i_filter_idx < 0 )
    {
        /* Oops, filter wasn't found
         * FIXME shoulnd't it be an assert instead ? */
        msg_Err( p_chain->p_this,
                 "Couldn't find filter %p when trying to remove it from chain",
                 p_filter );
        return VLC_EGENERIC;
    }

    /* Remove it from the chain */
    vlc_array_remove( &p_chain->filters, i_filter_idx );

    msg_Dbg( p_chain->p_this, "Filter %p removed from chain", p_filter );

    /* Destroy the filter object */
    if( IsInternalVideoAllocator( p_filter ) )
        AllocatorClean( &internal_video_allocator, p_filter );
    else
        AllocatorClean( &p_chain->allocator, p_filter );

    vlc_object_detach( p_filter );
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );
    vlc_object_release( p_filter );

    vlc_mouse_t *p_mouse = vlc_array_item_at_index( &p_chain->mouses, i_filter_idx );
    free( p_mouse );
    vlc_array_remove( &p_chain->mouses, i_filter_idx );

    /* FIXME: check fmt_in/fmt_out consitency */
    return VLC_SUCCESS;
}

/**
 * Internal chain buffer handling
 */

static int UpdateVideoBufferFunctions( filter_chain_t *p_chain )
{
    /**
     * Last filter uses the filter chain's parent buffer allocation
     * functions. All the other filters use internal functions.
     * This makes it possible to have format changes between each
     * filter without having to worry about the parent's picture
     * heap format.
     */
    const int i_count = vlc_array_count( &p_chain->filters );
    for( int i = 0; i < i_count - 1; i++ )
    {
        filter_t *p_filter = vlc_array_item_at_index( &p_chain->filters, i );

        if( !IsInternalVideoAllocator( p_filter ) )
        {
            AllocatorClean( &p_chain->allocator, p_filter );

            AllocatorInit( &internal_video_allocator, p_filter );
        }
    }
    if( i_count >= 1 )
    {
        filter_t * p_filter = vlc_array_item_at_index( &p_chain->filters, i_count - 1 );
        if( IsInternalVideoAllocator( p_filter ) )
        {
            AllocatorClean( &internal_video_allocator, p_filter );

            if( AllocatorInit( &p_chain->allocator, p_filter ) )
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}
/**
 * This function should be called after every filter chain change
 */
static int UpdateBufferFunctions( filter_chain_t *p_chain )
{
    if( !strcmp( p_chain->psz_capability, "video filter2" ) )
        return UpdateVideoBufferFunctions( p_chain );

    return VLC_SUCCESS;
}

/* Internal video allocator functions */
static picture_t *VideoBufferNew( filter_t *p_filter )
{
    const video_format_t *p_fmt = &p_filter->fmt_out.video;

    picture_t *p_picture = picture_New( p_fmt->i_chroma,
                                        p_fmt->i_width, p_fmt->i_height,
                                        p_fmt->i_aspect );
    if( !p_picture )
        msg_Err( p_filter, "Failed to allocate picture" );
    return p_picture;
}
static void VideoBufferDelete( filter_t *p_filter, picture_t *p_picture )
{
    VLC_UNUSED( p_filter );
    picture_Release( p_picture );
}
static int InternalVideoInit( filter_t *p_filter, void *p_data )
{
    VLC_UNUSED(p_data);

    p_filter->pf_vout_buffer_new = VideoBufferNew;
    p_filter->pf_vout_buffer_del = VideoBufferDelete;

    return VLC_SUCCESS;
}
static void InternalVideoClean( filter_t *p_filter )
{
    p_filter->pf_vout_buffer_new = NULL;
    p_filter->pf_vout_buffer_del = NULL;
}
static bool IsInternalVideoAllocator( filter_t *p_filter )
{
    return p_filter->pf_vout_buffer_new == VideoBufferNew;
}

/* */
static int AllocatorInit( const filter_chain_allocator_t *p_alloc, filter_t *p_filter )
{
    if( p_alloc->pf_init )
        return p_alloc->pf_init( p_filter, p_alloc->p_data );
    return VLC_SUCCESS;
}
static void AllocatorClean( const filter_chain_allocator_t *p_alloc, filter_t *p_filter )
{
    if( p_alloc->pf_clean )
        p_alloc->pf_clean( p_filter );
}

