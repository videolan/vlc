/*****************************************************************************
 * filter_chain.c : Handle chains of filter_t objects.
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * $Id$
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
#include <vlc_spu.h>
#include <libvlc.h>
#include <assert.h>

typedef struct
{
    int (*pf_init)( filter_t *, void *p_data ); /* Callback called once filter allocation has succeeded to initialize the filter's buffer allocation callbacks. This function is responsible for setting p_owner if needed. */
    void (* pf_clean)( filter_t * ); /* Callback called on filter removal from chain to clean up buffer allocation callbacks data (ie p_owner) */
    void *p_data; /* Data for pf_buffer_allocation_init */

} filter_chain_allocator_t;

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
static inline chained_filter_t *chained (filter_t *filter)
{
    return (chained_filter_t *)filter;
}

static int  AllocatorInit( const filter_chain_allocator_t *,
                           chained_filter_t * );
static void AllocatorClean( const filter_chain_allocator_t *,
                            chained_filter_t * );

static bool IsInternalVideoAllocator( chained_filter_t * );

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
    vlc_object_t *p_this; /**< Owner object */
    filter_chain_allocator_t allocator; /**< Owner allocation callbacks */

    chained_filter_t *first, *last; /**< List of filters */

    es_format_t fmt_in; /**< Chain input format (constant) */
    es_format_t fmt_out; /**< Chain current output format */
    unsigned length; /**< Number of filters */
    bool b_allow_fmt_out_change; /**< Can the output format be changed? */
    char psz_capability[1]; /**< Module capability for all chained filters */
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

static void FilterDeletePictures( filter_t *, picture_t * );

#undef filter_chain_New
/**
 * Filter chain initialisation
 */
filter_chain_t *filter_chain_New( vlc_object_t *p_this,
                                  const char *psz_capability,
                                  bool b_allow_fmt_out_change,
                                  int  (*pf_buffer_allocation_init)( filter_t *, void * ),
                                  void (*pf_buffer_allocation_clean)( filter_t * ),
                                  void *p_buffer_allocation_data )
{
    assert( p_this );
    assert( psz_capability );

    size_t size = sizeof(filter_chain_t) + strlen(psz_capability);
    filter_chain_t *p_chain = malloc( size );
    if( !p_chain )
        return NULL;

    p_chain->p_this = p_this;
    p_chain->last = p_chain->first = NULL;
    p_chain->length = 0;
    strcpy( p_chain->psz_capability, psz_capability );

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

    free( p_chain );
}
/**
 * Filter chain reinitialisation
 */
void filter_chain_Reset( filter_chain_t *p_chain, const es_format_t *p_fmt_in,
                         const es_format_t *p_fmt_out )
{
    filter_t *p_filter;

    while( (p_filter = &p_chain->first->filter) != NULL )
        filter_chain_DeleteFilterInternal( p_chain, p_filter );

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
    return p_chain->length;
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
            FilterDeletePictures( p_filter, f->pending );
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

        FilterDeletePictures( p_filter, f->pending );
        f->pending = NULL;

        filter_FlushPictures( p_filter );
    }
}


block_t *filter_chain_AudioFilter( filter_chain_t *p_chain, block_t *p_block )
{
    for( chained_filter_t *f = p_chain->first; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;

        p_block = p_filter->pf_audio_filter( p_filter, p_block );
        if( !p_block )
            break;
    }
    return p_block;
}

void filter_chain_SubSource( filter_chain_t *p_chain,
                             mtime_t display_date )
{
    for( chained_filter_t *f = p_chain->first; f != NULL; f = f->next )
    {
        filter_t *p_filter = &f->filter;
        subpicture_t *p_subpic = p_filter->pf_sub_source( p_filter, display_date );
        /* XXX I find that spu_t cast ugly */
        if( p_subpic )
            spu_PutSubpicture( (spu_t*)p_chain->p_this, p_subpic );
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
static filter_t *filter_chain_AppendFilterInternal( filter_chain_t *p_chain,
                                                    const char *psz_name,
                                                    config_chain_t *p_cfg,
                                                    const es_format_t *p_fmt_in,
                                                    const es_format_t *p_fmt_out )
{
    chained_filter_t *p_chained =
        vlc_custom_create( p_chain->p_this, sizeof(*p_chained), "filter" );
    filter_t *p_filter = &p_chained->filter;
    if( !p_filter )
        return NULL;

    if( !p_fmt_in )
    {
        if( p_chain->last != NULL )
            p_fmt_in = &p_chain->last->filter.fmt_out;
        else
            p_fmt_in = &p_chain->fmt_in;
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

    if( AllocatorInit( &p_chain->allocator, p_chained ) )
        goto error;

    if( p_chain->last == NULL )
    {
        assert( p_chain->first == NULL );
        p_chain->first = p_chained;
    }
    else
        p_chain->last->next = p_chained;
    p_chained->prev = p_chain->last;
    p_chain->last = p_chained;
    p_chained->next = NULL;
    p_chain->length++;

    vlc_mouse_t *p_mouse = malloc( sizeof(*p_mouse) );
    if( p_mouse )
        vlc_mouse_Init( p_mouse );
    p_chained->mouse = p_mouse;
    p_chained->pending = NULL;

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
    chained_filter_t *p_chained = chained( p_filter );

    /* Remove it from the chain */
    if( p_chained->prev != NULL )
        p_chained->prev->next = p_chained->next;
    else
    {
        assert( p_chained == p_chain->first );
        p_chain->first = p_chained->next;
    }

    if( p_chained->next != NULL )
        p_chained->next->prev = p_chained->prev;
    else
    {
        assert( p_chained == p_chain->last );
        p_chain->last = p_chained->prev;
    }
    p_chain->length--;

    msg_Dbg( p_chain->p_this, "Filter %p removed from chain", p_filter );

    FilterDeletePictures( &p_chained->filter, p_chained->pending );

    /* Destroy the filter object */
    if( IsInternalVideoAllocator( p_chained ) )
        AllocatorClean( &internal_video_allocator, p_chained );
    else
        AllocatorClean( &p_chain->allocator, p_chained );

    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );
    free( p_chained->mouse );
    vlc_object_release( p_filter );


    /* FIXME: check fmt_in/fmt_out consitency */
    return VLC_SUCCESS;
}

static void FilterDeletePictures( filter_t *filter, picture_t *picture )
{
    while( picture )
    {
        picture_t *next = picture->p_next;
        filter_DeletePicture( filter, picture );
        picture = next;
    }
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
    /* FIXME: we should only update the last and penultimate filters */
    chained_filter_t *f;

    for( f = p_chain->first; f != p_chain->last; f = f->next )
    {
        if( !IsInternalVideoAllocator( f ) )
        {
            AllocatorClean( &p_chain->allocator, f );

            AllocatorInit( &internal_video_allocator, f );
        }
    }

    if( f != NULL )
    {
        if( IsInternalVideoAllocator( f ) )
        {
            AllocatorClean( &internal_video_allocator, f );

            if( AllocatorInit( &p_chain->allocator, f ) )
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

    picture_t *p_picture = picture_NewFromFormat( p_fmt );
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

    p_filter->pf_video_buffer_new = VideoBufferNew;
    p_filter->pf_video_buffer_del = VideoBufferDelete;

    return VLC_SUCCESS;
}
static void InternalVideoClean( filter_t *p_filter )
{
    p_filter->pf_video_buffer_new = NULL;
    p_filter->pf_video_buffer_del = NULL;
}

static bool IsInternalVideoAllocator( chained_filter_t *p_filter )
{
    return p_filter->filter.pf_video_buffer_new == VideoBufferNew;
}

/* */
static int AllocatorInit( const filter_chain_allocator_t *p_alloc,
                          chained_filter_t *p_filter )
{
    if( p_alloc->pf_init )
        return p_alloc->pf_init( &p_filter->filter, p_alloc->p_data );
    return VLC_SUCCESS;
}

static void AllocatorClean( const filter_chain_allocator_t *p_alloc,
                            chained_filter_t *p_filter )
{
    if( p_alloc->pf_clean )
        p_alloc->pf_clean( &p_filter->filter );
}

