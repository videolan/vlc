/*****************************************************************************
 * vlc_filter.h: filter related structures and functions
 *****************************************************************************
 * Copyright (C) 1999-2014 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *          Rémi Denis-Courmont
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
#include <vlc_codec.h>

typedef struct vlc_video_context  vlc_video_context;

/**
 * \defgroup filter Filters
 * \ingroup output
 * Audio, video, text filters
 * @{
 * \file
 * Filter modules interface
 */

struct filter_video_callbacks
{
    picture_t *(*buffer_new)(filter_t *);
    vlc_decoder_device * (*hold_device)(vlc_object_t *, void *sys);
};

struct filter_subpicture_callbacks
{
    subpicture_t *(*buffer_new)(filter_t *);
};

typedef struct filter_owner_t
{
    union
    {
        const struct filter_video_callbacks *video;
        const struct filter_subpicture_callbacks *sub;
    };

    /* Input attachments
     * XXX use filter_GetInputAttachments */
    int (*pf_get_attachments)( filter_t *, input_attachment_t ***, int * );

    void *sys;
} filter_owner_t;

struct vlc_mouse_t;

/** Structure describing a filter
 * @warning BIG FAT WARNING : the code relies on the first 4 members of
 * filter_t and decoder_t to be the same, so if you have anything to add,
 * do it at the end of the structure.
 */
struct filter_t
{
    struct vlc_object_t obj;

    /* Module properties */
    module_t *          p_module;
    void               *p_sys;

    /* Input format */
    es_format_t         fmt_in;
    vlc_video_context   *vctx_in;  // video filter, set by owner

    /* Output format of filter */
    es_format_t         fmt_out;
    vlc_video_context   *vctx_out; // video filter, handled by the filter
    bool                b_allow_fmt_out_change;

    /* Name of the "video filter" shortcut that is requested, can be NULL */
    const char *        psz_name;
    /* Filter configuration */
    config_chain_t *    p_cfg;

    union
    {
        /** Filter a picture (video filter) */
        picture_t * (*pf_video_filter)( filter_t *, picture_t * );

        /** Filter an audio block (audio filter) */
        block_t * (*pf_audio_filter)( filter_t *, block_t * );

        /** Blend a subpicture onto a picture (blend) */
        void (*pf_video_blend)( filter_t *,  picture_t *, const picture_t *,
                                 int, int, int );

        /** Generate a subpicture (sub source) */
        subpicture_t *(*pf_sub_source)( filter_t *, vlc_tick_t );

        /** Filter a subpicture (sub filter) */
        subpicture_t *(*pf_sub_filter)( filter_t *, subpicture_t * );

        /** Render text (text render) */
        int (*pf_render)( filter_t *, subpicture_region_t *,
                          subpicture_region_t *, const vlc_fourcc_t * );
    };

    union
    {
        /* TODO: video filter drain */
        /** Drain (audio filter) */
        block_t *(*pf_audio_drain) ( filter_t * );
    };

    /** Flush
     *
     * Flush (i.e. discard) any internal buffer in a video or audio filter.
     */
    void (*pf_flush)( filter_t * );

    /** Change viewpoint
     *
     * Pass a new viewpoint to audio filters. Filters like the spatialaudio one
     * used for Ambisonics rendering will change its output according to this
     * viewpoint.
     */
    void (*pf_change_viewpoint)( filter_t *, const vlc_viewpoint_t * );

    union
    {
        /** Filter mouse state (video filter).
         *
         * If non-NULL, you must convert from output to input formats:
         * - If VLC_SUCCESS is returned, the mouse state is propagated.
         * - Otherwise, the mouse change is not propagated.
         * If NULL, the mouse state is considered unchanged and will be
         * propagated. */
        int (*pf_video_mouse)( filter_t *, struct vlc_mouse_t *,
                               const struct vlc_mouse_t *p_old,
                               const struct vlc_mouse_t *p_new );
    };

    /** Private structure for the owner of the filter */
    filter_owner_t      owner;
};

/**
 * This function will return a new picture usable by p_filter as an output
 * buffer. You have to release it using picture_Release or by returning
 * it to the caller as a pf_video_filter return value.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \return new picture on success or NULL on failure
 */
static inline picture_t *filter_NewPicture( filter_t *p_filter )
{
    picture_t *pic = NULL;
    if ( p_filter->owner.video != NULL && p_filter->owner.video->buffer_new != NULL)
        pic = p_filter->owner.video->buffer_new( p_filter );
    if ( pic == NULL )
    {
        // legacy filter owners not setting a default filter_allocator
        pic = picture_NewFromFormat( &p_filter->fmt_out.video );
    }
    if( pic == NULL )
        msg_Warn( p_filter, "can't get output picture" );
    return pic;
}

/**
 * Flush a filter
 *
 * This function will flush the state of a filter (audio or video).
 */
static inline void filter_Flush( filter_t *p_filter )
{
    if( p_filter->pf_flush != NULL )
        p_filter->pf_flush( p_filter );
}

static inline void filter_ChangeViewpoint( filter_t *p_filter,
                                           const vlc_viewpoint_t *vp)
{
    if( p_filter->pf_change_viewpoint != NULL )
        p_filter->pf_change_viewpoint( p_filter, vp );
}

static inline vlc_decoder_device * filter_HoldDecoderDevice( filter_t *p_filter )
{
    if ( !p_filter->owner.video || !p_filter->owner.video->hold_device )
        return NULL;

    return p_filter->owner.video->hold_device( VLC_OBJECT(p_filter), p_filter->owner.sys );
}

static inline vlc_decoder_device * filter_HoldDecoderDeviceType( filter_t *p_filter,
                                                                 enum vlc_decoder_device_type type )
{
    if ( !p_filter->owner.video || !p_filter->owner.video->hold_device )
        return NULL;

    vlc_decoder_device *dec_dev = p_filter->owner.video->hold_device( VLC_OBJECT(p_filter),
                                                                      p_filter->owner.sys );
    if ( dec_dev != NULL )
    {
        if ( dec_dev->type == type )
            return dec_dev;
        vlc_decoder_device_Release(dec_dev);
    }
    return NULL;
}

/**
 * This function will drain, then flush an audio filter.
 */
static inline block_t *filter_DrainAudio( filter_t *p_filter )
{
    if( p_filter->pf_audio_drain )
        return p_filter->pf_audio_drain( p_filter );
    else
        return NULL;
}

/**
 * This function will return a new subpicture usable by p_filter as an output
 * buffer. You have to release it using subpicture_Delete or by returning it to
 * the caller as a pf_sub_source return value.
 * Provided for convenience.
 *
 * \param p_filter filter_t object
 * \return new subpicture
 */
static inline subpicture_t *filter_NewSubpicture( filter_t *p_filter )
{
    subpicture_t *subpic = p_filter->owner.sub->buffer_new( p_filter );
    if( subpic == NULL )
        msg_Warn( p_filter, "can't get output subpicture" );
    return subpic;
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
    if( !p_filter->owner.pf_get_attachments )
        return VLC_EGENERIC;
    return p_filter->owner.pf_get_attachments( p_filter,
                                               ppp_attachment, pi_attachment );
}

/**
 * This function duplicates every variables from the filter, and adds a proxy
 * callback to trigger filter events from obj.
 *
 * \param restart_cb a vlc_callback_t to call if the event means restarting the
 * filter (i.e. an event on a non-command variable)
 */
VLC_API void filter_AddProxyCallbacks( vlc_object_t *obj, filter_t *filter,
                                       vlc_callback_t restart_cb );
# define filter_AddProxyCallbacks(a, b, c) \
    filter_AddProxyCallbacks(VLC_OBJECT(a), b, c)

/**
 * This function removes the callbacks previously added to every duplicated
 * variables, and removes them afterward.
 *
 * \param restart_cb the same vlc_callback_t passed to filter_AddProxyCallbacks
 */
VLC_API void filter_DelProxyCallbacks( vlc_object_t *obj, filter_t *filter,
                                       vlc_callback_t restart_cb);
# define filter_DelProxyCallbacks(a, b, c) \
    filter_DelProxyCallbacks(VLC_OBJECT(a), b, c)

typedef filter_t vlc_blender_t;

/**
 * It creates a blend filter.
 *
 * Only the chroma properties of the dest format is used (chroma
 * type, rgb masks and shifts)
 */
VLC_API vlc_blender_t * filter_NewBlend( vlc_object_t *, const video_format_t *p_dst_chroma ) VLC_USED;

/**
 * It configures blend filter parameters that are allowed to changed
 * after the creation.
 */
VLC_API int filter_ConfigureBlend( vlc_blender_t *, int i_dst_width, int i_dst_height, const video_format_t *p_src );

/**
 * It blends a picture into another one.
 *
 * The input picture is not modified and not released.
 */
VLC_API int filter_Blend( vlc_blender_t *, picture_t *p_dst, int i_dst_x, int i_dst_y, const picture_t *p_src, int i_alpha );

/**
 * It destroys a blend filter created by filter_NewBlend.
 */
VLC_API void filter_DeleteBlend( vlc_blender_t * );

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
 * \param obj pointer to a vlc object
 * \param psz_capability vlc capability of filters in filter chain
 * \return pointer to a filter chain
 */
filter_chain_t * filter_chain_NewSPU( vlc_object_t *obj, const char *psz_capability )
VLC_USED;
#define filter_chain_NewSPU( a, b ) filter_chain_NewSPU( VLC_OBJECT( a ), b )

/**
 * Creates a new video filter chain.
 *
 * \param obj pointer to parent VLC object
 * \param change whether to allow changing the output format
 * \param owner owner video buffer callbacks
 * \return new filter chain, or NULL on error
 */
VLC_API filter_chain_t * filter_chain_NewVideo( vlc_object_t *obj, bool change,
                                                const filter_owner_t *owner )
VLC_USED;
#define filter_chain_NewVideo( a, b, c ) \
        filter_chain_NewVideo( VLC_OBJECT( a ), b, c )

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
 * \paramt vctx_in new input video context
 * \param p_fmt_out new fmt_out params
 */
VLC_API void filter_chain_Reset( filter_chain_t *p_chain,
                                 const es_format_t *p_fmt_in,
                                 vlc_video_context *vctx_in,
                                 const es_format_t *p_fmt_out );

/**
 * Remove all existing filters
 *
 * \param p_chain pointer to filter chain
 */
VLC_API void filter_chain_Clear(filter_chain_t *);

/**
 * Append a filter to the chain.
 *
 * \param chain filter chain to append a filter to
 * \param name filter name
 * \param fmt_out filter output format
 * \return a pointer to the filter or NULL on error
 */
VLC_API filter_t *filter_chain_AppendFilter(filter_chain_t *chain,
    const char *name, config_chain_t *cfg,
    const es_format_t *fmt_out);

/**
 * Append a conversion to the chain.
 *
 * \param chain filter chain to append a filter to
 * \param fmt_out filter output format
 * \retval 0 on success
 * \retval -1 on failure
 */
VLC_API int filter_chain_AppendConverter(filter_chain_t *chain,
    const es_format_t *fmt_out);

/**
 * Append new filter to filter chain from string.
 *
 * \param chain filter chain to append a filter to
 * \param str filters chain nul-terminated string
 */
VLC_API int filter_chain_AppendFromString(filter_chain_t *chain,
                                          const char *str);

/**
 * Delete filter from filter chain. This function also releases the filter
 * object and unloads the filter modules. The pointer to p_filter is no
 * longer valid after this function successfully returns.
 *
 * \param chain filter chain to remove the filter from
 * \param filter filter to remove from the chain and delete
 */
VLC_API void filter_chain_DeleteFilter(filter_chain_t *chain,
                                       filter_t *filter);

/**
 * Checks if the filter chain is empty.
 *
 * \param chain pointer to filter chain
 * \return true if and only if there are no filters in this filter chain
 */
VLC_API bool filter_chain_IsEmpty(const filter_chain_t *chain);

/**
 * Get last output format of the last element in the filter chain.
 *
 * \param chain filter chain
 */
VLC_API const es_format_t *filter_chain_GetFmtOut(const filter_chain_t *chain);

/**
 * Get last output video context of the last element in the filter chain.
 * \note doesn't create change the reference count
 *
 * \param chain filter chain
 */
VLC_API vlc_video_context *filter_chain_GetVideoCtxOut(const filter_chain_t *chain);

/**
 * Apply the filter chain to a video picture.
 *
 * \param chain pointer to filter chain
 * \param pic picture to apply filters to
 * \return modified picture after applying all video filters
 */
VLC_API picture_t *filter_chain_VideoFilter(filter_chain_t *chain,
                                            picture_t *pic);

/**
 * Flush a video filter chain.
 */
VLC_API void filter_chain_VideoFlush( filter_chain_t * );

/**
 * Generate subpictures from a chain of subpicture source "filters".
 *
 * \param chain filter chain
 * \param display_date of subpictures
 */
void filter_chain_SubSource(filter_chain_t *chain, spu_t *,
                            vlc_tick_t display_date);

/**
 * Apply filter chain to subpictures.
 *
 * \param chain filter chain
 * \param subpic subpicture to apply filters on
 * \return modified subpicture after applying all subpicture filters
 */
VLC_API subpicture_t *filter_chain_SubFilter(filter_chain_t *chain,
                                             subpicture_t *subpic);

/**
 * Apply the filter chain to a mouse state.
 *
 * It will be applied from the output to the input. It makes sense only
 * for a video filter chain.
 *
 * The vlc_mouse_t* pointers may be the same.
 */
VLC_API int filter_chain_MouseFilter( filter_chain_t *, struct vlc_mouse_t *,
                                      const struct vlc_mouse_t * );

VLC_API int filter_chain_ForEach( filter_chain_t *chain,
                          int (*cb)( filter_t *, void * ), void *opaque );

/** @} */
#endif /* _VLC_FILTER_H */
