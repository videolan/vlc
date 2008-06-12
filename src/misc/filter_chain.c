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

#include <vlc_filter.h>
#include <vlc_arrays.h>

struct filter_chain_t
{
    vlc_object_t *p_this; /* Parent object */

    vlc_array_t filters; /* List of filters */

    char *psz_capability; /* Capability of all the filters in the chain */
    es_format_t fmt_in; /* Input format (read only) */
    es_format_t fmt_out; /* Output format (writable depending on ... */
    bool b_allow_fmt_out_change; /* allow changing fmt_out if true */

    int (* pf_buffer_allocation_init)( filter_t *, void *p_data ); /* Callback called once filter allocation has succeeded to initialize the filter's buffer allocation callbacks. This function is responsible for setting p_owner if needed. */
    void (* pf_buffer_allocation_clear)( filter_t * ); /* Callback called on filter removal from chain to clean up buffer allocation callbacks data (ie p_owner) */
    void *p_buffer_allocation_data; /* Data for pf_buffer_allocation_init */
};

/**
 * Filter chain initialisation
 */
filter_chain_t *__filter_chain_New( vlc_object_t *p_this,
                                    const char *psz_capability,
                                    bool b_allow_fmt_out_change,
                      int (*pf_buffer_allocation_init)( filter_t *, void * ),
                      void (*pf_buffer_allocation_clear)( filter_t * ),
                      void *p_buffer_allocation_data )
{
    filter_chain_t *p_chain = (filter_chain_t *)
        malloc( sizeof( filter_chain_t ) );
    if( !p_chain ) return NULL;
    p_chain->p_this = p_this;
    vlc_array_init( &p_chain->filters );
    p_chain->psz_capability = strdup( psz_capability );
    if( !p_chain->psz_capability )
    {
        free( p_chain );
        return NULL;
    }
    es_format_Init( &p_chain->fmt_in, UNKNOWN_ES, 0 );
    es_format_Init( &p_chain->fmt_out, UNKNOWN_ES, 0 );
    p_chain->b_allow_fmt_out_change = b_allow_fmt_out_change;

    p_chain->pf_buffer_allocation_init = pf_buffer_allocation_init;
    p_chain->pf_buffer_allocation_clear = pf_buffer_allocation_clear;
    p_chain->p_buffer_allocation_data = p_buffer_allocation_data;

    return p_chain;
}

/**
 * Filter chain destruction
 */
void filter_chain_Delete( filter_chain_t *p_chain )
{
    while( p_chain->filters.i_count )
        filter_chain_DeleteFilter( p_chain,
                                   (filter_t*)p_chain->filters.pp_elems[0] );
    vlc_array_clear( &p_chain->filters );
    free( p_chain->psz_capability );
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
    while( p_chain->filters.i_count )
        filter_chain_DeleteFilter( p_chain,
                                   (filter_t*)p_chain->filters.pp_elems[0] );
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


/**
 * Modifying the filter chain
 */
filter_t *filter_chain_AppendFilter( filter_chain_t *p_chain,
                                     const char *psz_name,
                                     config_chain_t *p_cfg,
                                     const es_format_t *p_fmt_in,
                                     const es_format_t *p_fmt_out )
{
    filter_t *p_filter =
        vlc_object_create( p_chain->p_this, VLC_OBJECT_FILTER );
    if( !p_filter ) return NULL;
    vlc_object_attach( p_filter, p_chain->p_this );

    if( !p_fmt_in )
    {
        if( p_chain->filters.i_count )
            p_fmt_in = &((filter_t*)p_chain->filters.pp_elems[p_chain->filters.i_count-1])->fmt_out;
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

    p_filter->p_module = module_Need( p_filter, p_chain->psz_capability,
                                      psz_name, psz_name ? true : false );

    if( !p_filter->p_module )
        goto error;

    if( p_filter->b_allow_fmt_out_change )
    {
        es_format_Clean( &p_chain->fmt_out );
        es_format_Copy( &p_chain->fmt_out, &p_filter->fmt_out );
    }

    if( p_chain->pf_buffer_allocation_init( p_filter,
            p_chain->p_buffer_allocation_data ) != VLC_SUCCESS )
        goto error;

    vlc_array_append( &p_chain->filters, p_filter );

    msg_Dbg( p_chain->p_this, "Filter '%s' (%p) appended to chain",
             psz_name, p_filter );

    return p_filter;

    error:
        msg_Err( p_chain->p_this, "Failed to create video filter '%s'",
                 psz_name );
        if( p_filter->p_module ) module_Unneed( p_filter,
                                                p_filter->p_module );
        es_format_Clean( &p_filter->fmt_in );
        es_format_Clean( &p_filter->fmt_out );
        vlc_object_detach( p_filter );
        vlc_object_release( p_filter );
        return NULL;
}

int filter_chain_AppendFromString( filter_chain_t *p_chain,
                                   const char *psz_string )
{
    config_chain_t *p_cfg = NULL;
    char *psz_name = NULL;

    if( !psz_string || !*psz_string ) return 0;

    psz_string = config_ChainCreate( &psz_name, &p_cfg, psz_string );

    filter_t *p_filter = filter_chain_AppendFilter( p_chain, psz_name, p_cfg,
                                                    NULL, NULL );
    if( !p_filter )
    {
        msg_Err( p_chain->p_this, "Failed while trying to append '%s' "
                 "to filter chain", psz_name );
        free( psz_name );
        free( p_cfg );
        return -1;
    }
    free( psz_name );

    int ret = filter_chain_AppendFromString( p_chain, psz_string );
    if( ret < 0 )
    {
        filter_chain_DeleteFilter( p_chain, p_filter );
        return ret;
    }
    return 1 + ret;
}

int filter_chain_DeleteFilter( filter_chain_t *p_chain, filter_t *p_filter )
{
    int i;
    /* Find the filter in the chain */
    for( i = 0; i < p_chain->filters.i_count; i++ )
        if( (filter_t*)p_chain->filters.pp_elems[i] == p_filter )
            break;

    /* Oops, filter wasn't found */
    if( i == p_chain->filters.i_count )
    {
        msg_Err( p_chain->p_this, "Couldn't find filter '%s' (%p) when trying "
                 "to remove it from chain", p_filter->psz_object_name,
                 p_filter );
        return VLC_EGENERIC;
    }

    /* Remove it from the chain */
    vlc_array_remove( &p_chain->filters, i );
    msg_Dbg( p_chain->p_this, "Filter '%s' (%p) removed from chain",
             p_filter->psz_object_name, p_filter );

    /* Destroy the filter object */
    if( p_chain->pf_buffer_allocation_clear )
        p_chain->pf_buffer_allocation_clear( p_filter );
    vlc_object_detach( p_filter );
    if( p_filter->p_module )
        module_Unneed( p_filter, p_filter->p_module );
    vlc_object_release( p_filter );

    /* FIXME: check fmt_in/fmt_out consitency */

    return VLC_SUCCESS;
}

/**
 * Reading from the filter chain
 */
filter_t *filter_chain_GetFilter( filter_chain_t *p_chain, int i_position,
                                  const char *psz_name )
{
    int i;
    filter_t **pp_filters = (filter_t **)p_chain->filters.pp_elems;

    if( i_position < 0 )
        return NULL;

    if( !psz_name )
    {
        if( i_position >= p_chain->filters.i_count )
            return NULL;
        return pp_filters[i_position];
    }

    for( i = 0; i < p_chain->filters.i_count; i++ )
    {
        if( !strcmp( psz_name, pp_filters[i]->psz_object_name ) )
            i_position--;
        if( i_position < 0 )
            return pp_filters[i];
   }
   return NULL;
}

int filter_chain_GetLength( filter_chain_t *p_chain )
{
    return p_chain->filters.i_count;
}

const es_format_t *filter_chain_GetFmtOut( filter_chain_t *p_chain )
{
    return &p_chain->fmt_out;
}

/**
 * Apply the filter chain
 */

/* FIXME This include is needed by the Ugly hack */
#include <vlc_vout.h>

picture_t *filter_chain_VideoFilter( filter_chain_t *p_chain, picture_t *p_pic )
{
    int i;
    filter_t **pp_filter = (filter_t **)p_chain->filters.pp_elems;
    for( i = 0; i < p_chain->filters.i_count; i++ )
    {
        filter_t *p_filter = pp_filter[i];
        picture_t *p_newpic = p_filter->pf_video_filter( p_filter, p_pic );
        if( !p_newpic )
            return NULL;
        /* FIXME Ugly hack to make it work in picture core.
         * FIXME Remove this code when the picture release API has been
         * FIXME cleaned up (a git revert of the commit should work) */
        if( p_chain->p_this->i_object_type == VLC_OBJECT_VOUT )
        {
            if( p_pic->i_refcount )
                p_pic->i_status = DISPLAYED_PICTURE;
            else
                p_pic->i_status = DESTROYED_PICTURE;
            p_newpic->i_status = READY_PICTURE;
        }
        p_pic = p_newpic;
    }
    return p_pic;
}

block_t *filter_chain_AudioFilter( filter_chain_t *p_chain, block_t *p_block )
{
    int i;
    filter_t **pp_filter = (filter_t **)p_chain->filters.pp_elems;
    for( i = 0; i < p_chain->filters.i_count; i++ )
    {
        filter_t *p_filter = pp_filter[i];
        p_block = p_filter->pf_audio_filter( p_filter, p_block );
        if( !p_block )
            return NULL;
    }
    return p_block;
}

#include <vlc_osd.h>

void filter_chain_SubFilter( filter_chain_t *p_chain,
                             mtime_t display_date )
{
    int i;
    filter_t **pp_filter = (filter_t **)p_chain->filters.pp_elems;
    for( i = 0; i < p_chain->filters.i_count; i++ )
    {
        filter_t *p_filter = pp_filter[i];
        subpicture_t *p_subpic = p_filter->pf_sub_filter( p_filter, display_date );
        if( p_subpic )
            spu_DisplaySubpicture( (spu_t*)p_chain->p_this, p_subpic );
    }
}
