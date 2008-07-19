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

#ifndef _VLC_FILTER_H
#define _VLC_FILTER_H 1

#include <vlc_es.h>

/**
 * \file
 * This file defines the structure and types used by video and audio filters
 */

typedef struct filter_owner_sys_t filter_owner_sys_t;

/** Structure describing a filter
 * @warning BIG FAT WARNING : the code relies in the first 4 members of
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
    void                ( * pf_video_blend )  ( filter_t *, picture_t *,
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
        if( !p_outpic )                                                 \
        {                                                               \
            picture_Release( p_pic );                                   \
            return NULL;                                                \
        }                                                               \
                                                                        \
        name( p_filter, p_pic, p_outpic );                              \
                                                                        \
        picture_CopyProperties( p_outpic, p_pic );                      \
        picture_Release( p_pic );                                       \
                                                                        \
        return p_outpic;                                                \
    }

/**
 * Filter chain management API
 */

typedef struct filter_chain_t filter_chain_t;

VLC_EXPORT( filter_chain_t *, __filter_chain_New, ( vlc_object_t *, const char *, bool, int (*)( filter_t *, void * ), void (*)( filter_t * ), void *  ) );
#define filter_chain_New( a, b, c, d, e, f ) __filter_chain_New( VLC_OBJECT( a ), b, c, d, e, f )
VLC_EXPORT( void, filter_chain_Delete, ( filter_chain_t * ) );
VLC_EXPORT( void, filter_chain_Reset, ( filter_chain_t *, const es_format_t *, const es_format_t * ) );

VLC_EXPORT( filter_t *, filter_chain_AppendFilter, ( filter_chain_t *, const char *, config_chain_t *, const es_format_t *, const es_format_t * ) );
VLC_EXPORT( int, filter_chain_AppendFromString, ( filter_chain_t *, const char * ) );
VLC_EXPORT( int, filter_chain_DeleteFilter, ( filter_chain_t *, filter_t * ) );

VLC_EXPORT( filter_t *, filter_chain_GetFilter, ( filter_chain_t *, int, const char * ) );
VLC_EXPORT( int, filter_chain_GetLength, ( filter_chain_t * ) );
VLC_EXPORT( const es_format_t *, filter_chain_GetFmtOut, ( filter_chain_t * ) );

/**
 * Apply the filter chain
 */
VLC_EXPORT( picture_t *, filter_chain_VideoFilter, ( filter_chain_t *, picture_t * ) );
VLC_EXPORT( block_t *, filter_chain_AudioFilter, ( filter_chain_t *, block_t * ) );
VLC_EXPORT( void, filter_chain_SubFilter, ( filter_chain_t *, mtime_t ) );

#endif /* _VLC_FILTER_H */
