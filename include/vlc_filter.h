/*****************************************************************************
 * vlc_filter.h: filter related structures and functions
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef VLC_FILTER_H
#define VLC_FILTER_H 1

#include <vlc_es.h>

/**
 * \file
 * This file defines the structure and types used by video and audio filters
 */

typedef struct filter_owner_sys_t filter_owner_sys_t;

/** Structure describing a filter
 * @warning BIG FAT WARNING : the code relies on the first 4 members of
 * filter_t and decoder_t to be the same, so if you have anything to add,
 * do it at the end of the structure.
 */
struct filter_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    filter_sys_t *      p_sys;

    /* Input format */
    es_format_t         fmt_in;

    /* Output format of filter */
    es_format_t         fmt_out;
    bool                b_allow_fmt_out_change;

    /* Filter configuration */
    config_chain_t *    p_cfg;

    picture_t *         ( * pf_video_filter ) ( filter_t *, picture_t * );
    block_t *           ( * pf_audio_filter ) ( filter_t *, block_t * );
    void                ( * pf_video_blend )  ( filter_t *,
                                                picture_t *, picture_t *,
                                                int, int, int );

    subpicture_t *      ( *pf_sub_filter ) ( filter_t *, mtime_t );
    int                 ( *pf_render_text ) ( filter_t *, subpicture_region_t *,
                                              subpicture_region_t * );
    int                 ( *pf_render_html ) ( filter_t *, subpicture_region_t *,
                                              subpicture_region_t * );

    /*
     * Buffers allocation
     */

    /* Audio output callbacks */
    block_t *       ( * pf_audio_buffer_new) ( filter_t *, int );

    /* Video output callbacks */
    picture_t     * ( * pf_vout_buffer_new) ( filter_t * );
    void            ( * pf_vout_buffer_del) ( filter_t *, picture_t * );
    /* void            ( * pf_picture_link)    ( picture_t * );
    void            ( * pf_picture_unlink)  ( picture_t * ); */

    /* SPU output callbacks */
    subpicture_t *  ( * pf_sub_buffer_new) ( filter_t * );
    void            ( * pf_sub_buffer_del) ( filter_t *, subpicture_t * );

    /* Private structure for the owner of the decoder */
    filter_owner_sys_t *p_owner;
};

/**
 * This function will return a new picture usable by p_filter as an output
 * buffer. You have to release it using filter_DeletePicture or by returning
 * it to the caller as a pf_video_filter return value.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \return new picture on success or NULL on failure
 */
static inline picture_t *filter_NewPicture( filter_t *p_filter )
{
    picture_t *p_picture = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_picture )
        msg_Warn( p_filter, "can't get output picture" );
    return p_picture;
}

/**
 * This function will release a picture create by filter_NewPicture.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \param p_picture picture to be deleted
 */
static inline void filter_DeletePicture( filter_t *p_filter, picture_t *p_picture )
{
    p_filter->pf_vout_buffer_del( p_filter, p_picture );
}

/**
 * This function will return a new subpicture usable by p_filter as an output
 * buffer. You have to release it using filter_DeleteSubpicture or by returning
 * it to the caller as a pf_sub_filter return value.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \return new subpicture
 */
static inline subpicture_t *filter_NewSubpicture( filter_t *p_filter )
{
    subpicture_t *p_subpicture = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_subpicture )
        msg_Warn( p_filter, "can't get output subpicture" );
    return p_subpicture;
}

/**
 * This function will release a subpicture create by filter_NewSubicture.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \param p_subpicture to be released
 */
static inline void filter_DeleteSubpicture( filter_t *p_filter, subpicture_t *p_subpicture )
{
    p_filter->pf_sub_buffer_del( p_filter, p_subpicture );
}

/**
 * This function will return a new audio buffer usable by p_filter as an
 * output buffer. You have to release it using block_Release or by returning
 * it to the caller as a pf_audio_filter return value.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \param i_size size of audio buffer requested
 * \return block to be used as audio output buffer
 */
static inline block_t *filter_NewAudioBuffer( filter_t *p_filter, int i_size )
{
    block_t *p_block = p_filter->pf_audio_buffer_new( p_filter, i_size );
    if( !p_block )
        msg_Warn( p_filter, "can't get output block" );
    return p_block;
}

/**
 * Create a picture_t *(*)( filter_t *, picture_t * ) compatible wrapper
 * using a void (*)( filter_t *, picture_t *, picture_t * ) function
 *
 * Currently used by the chroma video filters
 */
#define VIDEO_FILTER_WRAPPER( name )                                    \
    static picture_t *name ## _Filter ( filter_t *p_filter,             \
                                        picture_t *p_pic )              \
    {                                                                   \
        picture_t *p_outpic = filter_NewPicture( p_filter );            \
        if( p_outpic )                                                  \
        {                                                               \
            name( p_filter, p_pic, p_outpic );                          \
            picture_CopyProperties( p_outpic, p_pic );                  \
        }                                                               \
        picture_Release( p_pic );                                       \
        return p_outpic;                                                \
    }

/**
 * Filter chain management API
 * The filter chain management API is used to dynamically construct filters
 * and add them in a chain.
 */

typedef struct filter_chain_t filter_chain_t;

/**
 * Create new filter chain
 *
 * \param p_object pointer to a vlc object
 * \param psz_capability vlc capability of filters in filter chain
 * \param b_allow_format_fmt_change allow changing of fmt
 * \param pf_buffer_allocation_init callback function to initialize buffer allocations
 * \param pf_buffer_allocation_clear callback function to clear buffer allocation initialization
 * \param p_buffer_allocation_data pointer to private allocation data
 * \return pointer to a filter chain
 */
VLC_EXPORT( filter_chain_t *, __filter_chain_New, ( vlc_object_t *, const char *, bool, int (*)( filter_t *, void * ), void (*)( filter_t * ), void *  ) );
#define filter_chain_New( a, b, c, d, e, f ) __filter_chain_New( VLC_OBJECT( a ), b, c, d, e, f )

/**
 * Delete filter chain will delete all filters in the chain and free all
 * allocated data. The pointer to the filter chain is then no longer valid.
 *
 * \param p_chain pointer to filter chain
 */
VLC_EXPORT( void, filter_chain_Delete, ( filter_chain_t * ) );

/**
 * Reset filter chain will delete all filters in the chain and
 * reset p_fmt_in and p_fmt_out to the new values.
 *
 * \param p_chain pointer to filter chain
 * \param p_fmt_in new fmt_in params
 * \param p_fmt_out new fmt_out params
 */
VLC_EXPORT( void, filter_chain_Reset, ( filter_chain_t *, const es_format_t *, const es_format_t * ) );

/**
 * Append filter to the end of the chain.
 *
 * \param p_chain pointer to filter chain
 * \param psz_name name of filter
 * \param p_cfg
 * \param p_fmt_in input es_format_t
 * \param p_fmt_out output es_format_t
 * \return pointer to filter chain
 */
VLC_EXPORT( filter_t *, filter_chain_AppendFilter, ( filter_chain_t *, const char *, config_chain_t *, const es_format_t *, const es_format_t * ) );

/**
 * Append new filter to filter chain from string.
 *
 * \param p_chain pointer to filter chain
 * \param psz_string string of filters
 * \return 0 for success
 */
VLC_EXPORT( int, filter_chain_AppendFromString, ( filter_chain_t *, const char * ) );

/**
 * Delete filter from filter chain. This function also releases the filter
 * object and unloads the filter modules. The pointer to p_filter is no
 * longer valid after this function successfully returns.
 *
 * \param p_chain pointer to filter chain
 * \param p_filter pointer to filter object
 * \return VLC_SUCCESS on succes, else VLC_EGENERIC
 */
VLC_EXPORT( int, filter_chain_DeleteFilter, ( filter_chain_t *, filter_t * ) );

/**
 * Get filter by name of position in the filter chain.
 *
 * \param p_chain pointer to filter chain
 * \param i_position position of filter in filter chain
 * \param psz_name name of filter to get
 * \return filter object based on position or name provided
 */
VLC_EXPORT( filter_t *, filter_chain_GetFilter, ( filter_chain_t *, int, const char * ) );

/**
 * Get the number of filters in the filter chain.
 *
 * \param p_chain pointer to filter chain
 * \return number of filters in this filter chain
 */
VLC_EXPORT( int, filter_chain_GetLength, ( filter_chain_t * ) );

/**
 * Get last p_fmt_out in the chain.
 *
 * \param p_chain pointer to filter chain
 * \return last p_fmt (es_format_t) of this filter chain
 */
VLC_EXPORT( const es_format_t *, filter_chain_GetFmtOut, ( filter_chain_t * ) );

/**
 * Apply the filter chain to a video picture.
 *
 * \param p_chain pointer to filter chain
 * \param p_picture picture to apply filters on
 * \return modified picture after applying all video filters
 */
VLC_EXPORT( picture_t *, filter_chain_VideoFilter, ( filter_chain_t *, picture_t * ) );

/**
 * Apply the filter chain to a audio block.
 *
 * \param p_chain pointer to filter chain
 * \param p_block audio frame to apply filters on
 * \return modified audio frame after applying all audio filters
 */
VLC_EXPORT( block_t *, filter_chain_AudioFilter, ( filter_chain_t *, block_t * ) );

/**
 * Apply filter chain to subpictures.
 *
 * \param p_chain pointer to filter chain
 * \param display_date of subpictures
 */
VLC_EXPORT( void, filter_chain_SubFilter, ( filter_chain_t *, mtime_t ) );

#endif /* _VLC_FILTER_H */

