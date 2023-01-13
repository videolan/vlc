/*****************************************************************************
 * vlc_picture.h: picture definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2009 VLC authors and VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Olivier Aubert <oaubert 47 videolan d07 org>
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

#ifndef VLC_PICTURE_H
#define VLC_PICTURE_H 1

#include <assert.h>
#include <vlc_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vlc_ancillary;
typedef uint32_t vlc_ancillary_id;

/**
 * \defgroup picture Generic picture API
 * \ingroup output
 * @{
 * \file
 * This file defines picture structures and functions in vlc
 */

#include <vlc_es.h>

/** Description of a planar graphic field */
typedef struct plane_t
{
    uint8_t *p_pixels;                        /**< Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_lines;           /**< Number of lines, including margins */
    int i_pitch;           /**< Number of bytes in a line, including margins */

    /** Size of a macropixel, defaults to 1 */
    int i_pixel_pitch;

    /* Variables used for pictures with margins */
    int i_visible_lines;            /**< How many visible lines are there? */
    int i_visible_pitch;            /**< How many bytes for visible pixels are there? */

} plane_t;

/**
 * Maximum number of plane for a picture
 */
#define PICTURE_PLANE_MAX (VOUT_MAX_PLANES)

typedef struct picture_context_t
{
    void (*destroy)(struct picture_context_t *);
    struct picture_context_t *(*copy)(struct picture_context_t *);
    struct vlc_video_context *vctx;
} picture_context_t;

typedef struct picture_buffer_t
{
    int fd;
    void *base;
    size_t size;
    off_t offset;
} picture_buffer_t;

typedef struct vlc_decoder_device vlc_decoder_device;
typedef struct vlc_video_context vlc_video_context;

struct vlc_video_context_operations
{
    void (*destroy)(void *priv);
};

/** Decoder device type */
enum vlc_video_context_type
{
    VLC_VIDEO_CONTEXT_VAAPI = 1, //!< private: vaapi_vctx* or empty
    VLC_VIDEO_CONTEXT_VDPAU,     //!< private: chroma type (YUV) or empty (RGB)
    VLC_VIDEO_CONTEXT_DXVA2,     //!< private: d3d9_video_context_t*
    VLC_VIDEO_CONTEXT_D3D11VA,   //!< private: d3d11_video_context_t*
    VLC_VIDEO_CONTEXT_AWINDOW,   //!< private: android_video_context_t*
    VLC_VIDEO_CONTEXT_NVDEC,     //!< empty
    VLC_VIDEO_CONTEXT_CVPX,      //!< private: cvpx_video_context*
    VLC_VIDEO_CONTEXT_MMAL,      //!< empty
    VLC_VIDEO_CONTEXT_GSTDECODE, //!< empty
};

VLC_API vlc_video_context * vlc_video_context_Create(vlc_decoder_device *,
                                        enum vlc_video_context_type private_type,
                                        size_t private_size,
                                        const struct vlc_video_context_operations *);
VLC_API void vlc_video_context_Release(vlc_video_context *);

VLC_API enum vlc_video_context_type vlc_video_context_GetType(const vlc_video_context *);
VLC_API void *vlc_video_context_GetPrivate(vlc_video_context *, enum vlc_video_context_type);
VLC_API vlc_video_context *vlc_video_context_Hold(vlc_video_context *);

/**
 * Get the decoder device used by the device context.
 *
 * This will increment the refcount of the decoder device.
 */
VLC_API vlc_decoder_device *vlc_video_context_HoldDevice(vlc_video_context *);


/**
 * Video picture
 */
struct picture_t
{
    /**
     * The properties of the picture
     */
    video_frame_format_t format;

    plane_t         p[PICTURE_PLANE_MAX];     /**< description of the planes */
    int             i_planes;                /**< number of allocated planes */

    /** \name Picture management properties
     * These properties can be modified using the video output thread API,
     * but should never be written directly */
    /**@{*/
    vlc_tick_t      date;                                  /**< display date */
    bool            b_force;
    bool            b_still;
    /**@}*/

    /** \name Picture dynamic properties
     * Those properties can be changed by the decoder
     * @{
     */
    bool            b_progressive;          /**< is it a progressive frame? */
    bool            b_top_field_first;             /**< which field is first */
    bool            b_multiview_left_eye; /**< left eye or right eye in multiview */
    unsigned int    i_nb_fields;                  /**< number of displayed fields */
    picture_context_t *context;      /**< video format-specific data pointer */
    /**@}*/

    /** Private data - the video output plugin might want to put stuff here to
     * keep track of the picture */
    void           *p_sys;

    /** Next picture in a FIFO a pictures */
    struct picture_t *p_next;

    vlc_atomic_rc_t refs;
};

static inline vlc_video_context* picture_GetVideoContext(picture_t *pic)
{
    return pic->context ? pic->context->vctx : NULL;
}

/**
 * Check whether a picture has other pictures linked
 */
static inline bool picture_HasChainedPics(const picture_t *pic)
{
    return pic->p_next != NULL;
}

/**
 * picture chaining helpers
 */

typedef struct vlc_pic_chain {
    picture_t *front;
    picture_t *tail;
} vlc_picture_chain_t;

/**
 * Initializes or reset a picture chain
 *
 * \warning do not call this if the chain still holds pictures, it will leak them.
 */
static inline void vlc_picture_chain_Init(vlc_picture_chain_t *chain)
{
    chain->front = NULL;
    // chain->tail = NULL not needed
}

/**
 * Check whether a picture chain holds pictures or not.
 *
 * \return true if it is empty.
 */
static inline bool vlc_picture_chain_IsEmpty(const vlc_picture_chain_t *chain)
{
    return chain->front == NULL;
}

/**
 * Check whether a picture chain has more than one picture.
 */
static inline bool vlc_picture_chain_HasNext(const vlc_picture_chain_t *chain)
{
    return !vlc_picture_chain_IsEmpty(chain) && chain->front != chain->tail;
}

/**
 * Pop the front of a picture chain.
 *
 * The next picture in the chain becomes the front of the picture chain.
 *
 * \return the front of the picture chain (the picture itself)
 */
static inline picture_t * vlc_picture_chain_PopFront(vlc_picture_chain_t *chain)
{
    picture_t *front = chain->front;
    if (front)
    {
        chain->front = front->p_next;
        // unlink the front picture from the rest of the chain
        front->p_next = NULL;
    }
    return front;
}

/**
 * Peek the front of a picture chain.
 *
 * The picture chain is unchanged.
 *
 * \return the front of the picture chain (the picture itself)
 */
static inline picture_t * vlc_picture_chain_PeekFront(vlc_picture_chain_t *chain)
{
    return chain->front;
}

/**
 * Append a picture to a picture chain.
 *
 * \param chain the picture chain pointer
 * \param pic the picture to append to the chain
 */
static inline void vlc_picture_chain_Append(vlc_picture_chain_t *chain,
                                            picture_t *pic)
{
    if (chain->front == NULL)
        chain->front = pic;
    else
        chain->tail->p_next = pic;
    // make sure the picture doesn't have chained pics
    vlc_assert( !picture_HasChainedPics( pic ) );
    pic->p_next = NULL; // we're appending a picture, not a chain
    chain->tail = pic;
}

/**
 * Append a picture chain to a picture chain.
 */
static inline void vlc_picture_chain_AppendChain(picture_t *chain, picture_t *tail)
{
    chain->p_next = tail;
}

/**
 * Copy the picture chain in another picture chain and clear the original
 * picture chain.
 *
 * \param in picture chain to copy and clear
 * \param out picture chain to copy into
 */
static inline void vlc_picture_chain_GetAndClear(vlc_picture_chain_t *in,
                                                 vlc_picture_chain_t *out)
{
    *out = *in;
    vlc_picture_chain_Init(in);
}

/**
 * Reset a picture chain.
 *
 * \return the picture chain that was contained in the picture
 */
static inline vlc_picture_chain_t picture_GetAndResetChain(picture_t *pic)
{
    vlc_picture_chain_t chain = { pic->p_next, pic->p_next };
    while ( chain.tail && chain.tail->p_next ) // find the proper tail
        chain.tail = chain.tail->p_next;
    pic->p_next = NULL;
    return chain;
}


/**
 * This function will create a new picture.
 * The picture created will implement a default release management compatible
 * with picture_Hold and picture_Release. This default management will release
 * p_sys, gc.p_sys fields if non NULL.
 */
VLC_API picture_t * picture_New( vlc_fourcc_t i_chroma, int i_width, int i_height, int i_sar_num, int i_sar_den ) VLC_USED;

/**
 * This function will create a new picture using the given format.
 *
 * When possible, it is preferred to use this function over picture_New
 * as more information about the format is kept.
 */
VLC_API picture_t * picture_NewFromFormat( const video_format_t *p_fmt ) VLC_USED;

/**
 * Resource for a picture.
 */
typedef struct
{
    void *p_sys;
    void (*pf_destroy)(picture_t *);

    /* Plane resources
     * XXX all fields MUST be set to the right value.
     */
    struct
    {
        uint8_t *p_pixels;  /**< Start of the plane's data */
        int i_lines;        /**< Number of lines, including margins */
        int i_pitch;        /**< Number of bytes in a line, including margins */
    } p[PICTURE_PLANE_MAX];

} picture_resource_t;

/**
 * This function will create a new picture using the provided resource.
 */
VLC_API picture_t * picture_NewFromResource( const video_format_t *, const picture_resource_t * ) VLC_USED;

/**
 * Destroys a picture without references.
 *
 * This function destroys a picture with zero references left.
 * Never call this function directly. Use picture_Release() instead.
 */
VLC_API void picture_Destroy(picture_t *picture);

/**
 * Increments the picture reference count.
 *
 * \return picture
 */
static inline picture_t *picture_Hold(picture_t *picture)
{
    vlc_atomic_rc_inc(&picture->refs);
    return picture;
}

/**
 * Decrements the picture reference count.
 *
 * If the reference count reaches zero, the picture is destroyed. If it was
 * allocated from a pool, the underlying picture buffer will be returned to the
 * pool. Otherwise, the picture buffer will be freed.
 */
static inline void picture_Release(picture_t *picture)
{
    if (vlc_atomic_rc_dec(&picture->refs))
        picture_Destroy(picture);
}

/**
 * This function will copy all picture dynamic properties.
 */
VLC_API void picture_CopyProperties( picture_t *p_dst, const picture_t *p_src );

/**
 * This function will reset a picture information (properties and quantizers).
 * It is sometimes useful for reusing pictures (like from a pool).
 */
VLC_API void picture_Reset( picture_t * );

/**
 * This function will copy the picture pixels.
 * You can safely copy between pictures that do not have the same size,
 * only the compatible(smaller) part will be copied.
 */
VLC_API void picture_CopyPixels( picture_t *p_dst, const picture_t *p_src );
VLC_API void plane_CopyPixels( plane_t *p_dst, const plane_t *p_src );

/**
 * This function will copy both picture dynamic properties and pixels.
 * You have to notice that sometime a simple picture_Hold may do what
 * you want without the copy overhead.
 * Provided for convenience.
 *
 * \param p_dst pointer to the destination picture.
 * \param p_src pointer to the source picture.
 */
VLC_API void picture_Copy( picture_t *p_dst, const picture_t *p_src );

/**
 * Perform a shallow picture copy
 *
 * This function makes a shallow copy of an existing picture. The same planes
 * and resources will be used, and the cloned picture reference count will be
 * incremented.
 *
 * \return A clone picture on success, NULL on error.
 */
VLC_API picture_t *picture_Clone(picture_t *pic);

/**
 * Attach an ancillary to the picture
 *
 * @warning the ancillary will be released only if the picture is created from
 * picture_New(), and picture_Clone().
 *
 * @note Several ancillaries can be attached to a picture, but if two
 * ancillaries are identified by the same ID, only the last one take
 * precedence.
 *
 * @param pic the picture to attach an ancillary
 * @param ancillary ancillary that will be held by the frame, can't be NULL
 * @return VLC_SUCCESS in case of success, VLC_ENOMEM in case of alloc error
 */
VLC_API int
picture_AttachAncillary(picture_t *pic, struct vlc_ancillary *ancillary);

/**
 * Allocate a new ancillary and attach it to a picture. Helper equivalent to
 * malloc + vlc_ancillary_Create + picture_AttachAncillary. The returned memory
 * is not initialized.
 *
 * @param pic picture to attach created ancillary to
 * @param id id of the ancillary to create
 * @param size allocation size in bytes
 * @return The allocated pointer on success, NULL on out-of-memory
 */
VLC_API void *
picture_AttachNewAncillary(picture_t *pic, vlc_ancillary_id id, size_t size);

/**
 * Return the ancillary identified by an ID
 *
 * @param id id of ancillary to request
 * @return the ancillary or NULL if the ancillary for that particular id is
 * not present
 */
VLC_API struct vlc_ancillary *
picture_GetAncillary(const picture_t *pic, vlc_ancillary_id id);

/**
 * This function will export a picture to an encoded bitstream.
 *
 * pp_image will contain the encoded bitstream in psz_format format.
 *
 * p_fmt can be NULL otherwise it will be set with the format used for the
 * picture before encoding.
 *
 * i_override_width/height allow to override the width and/or the height of the
 * picture to be encoded:
 *  - if strictly lower than 0, the original dimension will be used.
 *  - if equal to 0, it will be deduced from the other dimension which must be
 *  different to 0.
 *  - if strictly higher than 0, it will either override the dimension if b_crop
 *  is false, or crop the picture to the provided size if b_crop is true.
 * If at most one of them is > 0 then the picture aspect ratio will be kept.
 */
VLC_API int picture_Export( vlc_object_t *p_obj, block_t **pp_image, video_format_t *p_fmt,
                            picture_t *p_picture, vlc_fourcc_t i_format, int i_override_width,
                            int i_override_height, bool b_crop );

/**
 * This function will setup all fields of a picture_t without allocating any
 * memory.
 * XXX The memory must already be initialized.
 * It does not need to be released.
 *
 * It will return VLC_EGENERIC if the core does not understand the requested
 * format.
 *
 * It can be useful to get the properties of planes.
 */
VLC_API int picture_Setup( picture_t *, const video_format_t * );


/*****************************************************************************
 * Shortcuts to access image components
 *****************************************************************************/

/* Plane indices */
enum
{
    Y_PLANE = 0,
    U_PLANE = 1,
    V_PLANE = 2,
    A_PLANE = 3,
};

/* Shortcuts */
#define Y_PIXELS     p[Y_PLANE].p_pixels
#define Y_PITCH      p[Y_PLANE].i_pitch
#define U_PIXELS     p[U_PLANE].p_pixels
#define U_PITCH      p[U_PLANE].i_pitch
#define V_PIXELS     p[V_PLANE].p_pixels
#define V_PITCH      p[V_PLANE].i_pitch
#define A_PIXELS     p[A_PLANE].p_pixels
#define A_PITCH      p[A_PLANE].i_pitch

/**
 * Swap UV planes of a Tri Planars picture.
 *
 * It just swap the planes information without doing any copy.
 */
static inline void picture_SwapUV(picture_t *picture)
{
    vlc_assert(picture->i_planes == 3);

    plane_t tmp_plane   = picture->p[U_PLANE];
    picture->p[U_PLANE] = picture->p[V_PLANE];
    picture->p[V_PLANE] = tmp_plane;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VLC_PICTURE_H */
