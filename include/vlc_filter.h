/*****************************************************************************
 * vlc_filter.h: filter related structures and functions
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef VLC_FILTER_H
#define VLC_FILTER_H 1

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_subpicture.h>
#include <vlc_mouse.h>

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

    union
    {
        struct
        {
            picture_t * (*pf_filter) ( filter_t *, picture_t * );
            void        (*pf_flush)( filter_t * );
            picture_t * (*pf_buffer_new) ( filter_t * );
            void        (*pf_buffer_del) ( filter_t *, picture_t * );
            /* Filter mouse state.
             *
             * If non-NULL, you must convert from output to input formats:
             * - If VLC_SUCCESS is returned, the mouse state is propagated.
             * - Otherwise, the mouse change is not propagated.
             * If NULL, the mouse state is considered unchanged and will be
             * propagated.
             */
            int         (*pf_mouse)( filter_t *, vlc_mouse_t *,
                                     const vlc_mouse_t *p_old,
                                     const vlc_mouse_t *p_new );
        } video;
#define pf_video_filter     u.video.pf_filter
#define pf_video_flush      u.video.pf_flush
#define pf_video_mouse      u.video.pf_mouse
#define pf_video_buffer_new u.video.pf_buffer_new
#define pf_video_buffer_del u.video.pf_buffer_del

        struct
        {
            block_t *   (*pf_filter) ( filter_t *, block_t * );
        } audio;
#define pf_audio_filter     u.audio.pf_filter

        struct
        {
            void        (*pf_blend) ( filter_t *,  picture_t *,
                                      const picture_t *, int, int, int );
        } blend;
#define pf_video_blend     u.blend.pf_blend

        struct
        {
            subpicture_t * (*pf_source)    ( filter_t *, mtime_t );
            subpicture_t * (*pf_buffer_new)( filter_t * );
            void           (*pf_buffer_del)( filter_t *, subpicture_t * );
            int            (*pf_mouse)     ( filter_t *,
                                             const vlc_mouse_t *p_old,
                                             const vlc_mouse_t *p_new,
                                             const video_format_t * );
        } sub;
#define pf_sub_source      u.sub.pf_source
#define pf_sub_buffer_new  u.sub.pf_buffer_new
#define pf_sub_buffer_del  u.sub.pf_buffer_del
#define pf_sub_mouse       u.sub.pf_mouse

        struct
        {
            subpicture_t * (*pf_filter) ( filter_t *, subpicture_t * );
        } subf;
#define pf_sub_filter      u.subf.pf_filter

        struct
        {
            int         (*pf_text) ( filter_t *, subpicture_region_t *,
                                     subpicture_region_t *,
                                     const vlc_fourcc_t * );
            int         (*pf_html) ( filter_t *, subpicture_region_t *,
                                     subpicture_region_t *,
                                     const vlc_fourcc_t * );
        } render;
#define pf_render_text     u.render.pf_text
#define pf_render_html     u.render.pf_html

    } u;

    /* Input attachments
     * XXX use filter_GetInputAttachments */
    int (*pf_get_attachments)( filter_t *, input_attachment_t ***, int * );

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
    picture_t *p_picture = p_filter->pf_video_buffer_new( p_filter );
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
    p_filter->pf_video_buffer_del( p_filter, p_picture );
}

/**
 * This function will flush the state of a video filter.
 */
static inline void filter_FlushPictures( filter_t *p_filter )
{
    if( p_filter->pf_video_flush )
        p_filter->pf_video_flush( p_filter );
}

/**
 * This function will return a new subpicture usable by p_filter as an output
 * buffer. You have to release it using filter_DeleteSubpicture or by returning
 * it to the caller as a pf_sub_source return value.
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
 * This function gives all input attachments at once.
 *
 * You MUST release the returned values
 */
static inline int filter_GetInputAttachments( filter_t *p_filter,
                                              input_attachment_t ***ppp_attachment,
                                              int *pi_attachment )
{
    if( !p_filter->pf_get_attachments )
        return VLC_EGENERIC;
    return p_filter->pf_get_attachments( p_filter,
                                         ppp_attachment, pi_attachment );
}

/**
 * It creates a blend filter.
 *
 * Only the chroma properties of the dest format is used (chroma
 * type, rgb masks and shifts)
 */
VLC_API filter_t * filter_NewBlend( vlc_object_t *, const video_format_t *p_dst_chroma ) VLC_USED;

/**
 * It configures blend filter parameters that are allowed to changed
 * after the creation.
 */
VLC_API int filter_ConfigureBlend( filter_t *, int i_dst_width, int i_dst_height, const video_format_t *p_src );

/**
 * It blends a picture into another one.
 *
 * The input picture is not modified and not released.
 */
VLC_API int filter_Blend( filter_t *, picture_t *p_dst, int i_dst_x, int i_dst_y, const picture_t *p_src, int i_alpha );

/**
 * It destroys a blend filter created by filter_NewBlend.
 */
VLC_API void filter_DeleteBlend( filter_t * );

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
VLC_API filter_chain_t * filter_chain_New( vlc_object_t *, const char *, bool, int (*)( filter_t *, void * ), void (*)( filter_t * ), void *  ) VLC_USED;
#define filter_chain_New( a, b, c, d, e, f ) filter_chain_New( VLC_OBJECT( a ), b, c, d, e, f )

/**
 * Delete filter chain will delete all filters in the chain and free all
 * allocated data. The pointer to the filter chain is then no longer valid.
 *
 * \param p_chain pointer to filter chain
 */
VLC_API void filter_chain_Delete( filter_chain_t * );

/**
 * Reset filter chain will delete all filters in the chain and
 * reset p_fmt_in and p_fmt_out to the new values.
 *
 * \param p_chain pointer to filter chain
 * \param p_fmt_in new fmt_in params
 * \param p_fmt_out new fmt_out params
 */
VLC_API void filter_chain_Reset( filter_chain_t *, const es_format_t *, const es_format_t * );

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
VLC_API filter_t * filter_chain_AppendFilter( filter_chain_t *, const char *, config_chain_t *, const es_format_t *, const es_format_t * );

/**
 * Append new filter to filter chain from string.
 *
 * \param p_chain pointer to filter chain
 * \param psz_string string of filters
 * \return 0 for success
 */
VLC_API int filter_chain_AppendFromString( filter_chain_t *, const char * );

/**
 * Delete filter from filter chain. This function also releases the filter
 * object and unloads the filter modules. The pointer to p_filter is no
 * longer valid after this function successfully returns.
 *
 * \param p_chain pointer to filter chain
 * \param p_filter pointer to filter object
 * \return VLC_SUCCESS on succes, else VLC_EGENERIC
 */
VLC_API int filter_chain_DeleteFilter( filter_chain_t *, filter_t * );

/**
 * Get the number of filters in the filter chain.
 *
 * \param p_chain pointer to filter chain
 * \return number of filters in this filter chain
 */
VLC_API int filter_chain_GetLength( filter_chain_t * );

/**
 * Get last p_fmt_out in the chain.
 *
 * \param p_chain pointer to filter chain
 * \return last p_fmt (es_format_t) of this filter chain
 */
VLC_API const es_format_t * filter_chain_GetFmtOut( filter_chain_t * );

/**
 * Apply the filter chain to a video picture.
 *
 * \param p_chain pointer to filter chain
 * \param p_picture picture to apply filters on
 * \return modified picture after applying all video filters
 */
VLC_API picture_t * filter_chain_VideoFilter( filter_chain_t *, picture_t * );

/**
 * Flush a video filter chain.
 */
VLC_API void filter_chain_VideoFlush( filter_chain_t * );

/**
 * Apply the filter chain to a audio block.
 *
 * \param p_chain pointer to filter chain
 * \param p_block audio frame to apply filters on
 * \return modified audio frame after applying all audio filters
 */
VLC_API block_t * filter_chain_AudioFilter( filter_chain_t *, block_t * );

/**
 * Apply filter chain to subpictures.
 *
 * \param p_chain pointer to filter chain
 * \param display_date of subpictures
 */
VLC_API void filter_chain_SubSource( filter_chain_t *, mtime_t );

/**
 * Apply filter chain to subpictures.
 *
 * \param p_chain pointer to filter chain
 * \param p_subpicture subpicture to apply filters on
 * \return modified subpicture after applying all subpicture filters
 */
VLC_API subpicture_t * filter_chain_SubFilter( filter_chain_t *, subpicture_t * );

/**
 * Apply the filter chain to a mouse state.
 *
 * It will be applied from the output to the input. It makes sense only
 * for a video filter chain.
 *
 * The vlc_mouse_t* pointers may be the same.
 */
VLC_API int filter_chain_MouseFilter( filter_chain_t *, vlc_mouse_t *, const vlc_mouse_t * );

/**
 * Inform the filter chain of mouse state.
 *
 * It makes sense only for a sub source chain.
 */
VLC_API int filter_chain_MouseEvent( filter_chain_t *, const vlc_mouse_t *, const video_format_t * );

#endif /* _VLC_FILTER_H */

