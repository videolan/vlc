/*****************************************************************************
 * vlc_video_splitter.h: "video splitter" related structures and functions
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_VIDEO_SPLITTER_H
#define VLC_VIDEO_SPLITTER_H 1

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_mouse.h>
#include <vlc_vout_display.h>

/**
 * \file
 * This file defines the structure and types used by video splitter filters.
 */

typedef struct video_splitter_t video_splitter_t;

struct vlc_window_mouse_event;

/** Structure describing a video splitter output properties
 */
typedef struct
{
    /* Video format of the output */
    video_format_t fmt;

    /* Video output module
     * Use NULL for default
     */
    char *psz_module;

} video_splitter_output_t;

/** Structure describing a video splitter
 */
struct video_splitter_t
{
    struct vlc_object_t obj;

    /* Module properties */
    module_t        *p_module;

    /* configuration */
    config_chain_t  *p_cfg;

    /* Input format
     * It is filled by the creator and cannot be modified.
     */
    video_format_t  fmt;

    /* Output formats
     *
     * It can only be set in the open() function and must remain
     * constant.
     * The module is responsible for the allocation and deallocation.
     */
    int                     i_output;
    video_splitter_output_t *p_output;

    int             (*pf_filter)( video_splitter_t *, picture_t *pp_dst[],
                                  picture_t *p_src );
    int (*mouse)(video_splitter_t *, int idx,
                 struct vlc_window_mouse_event *);

    void *p_sys;
};

/**
 * It will create an array of pictures suitable as output.
 *
 * You must either returned them through pf_filter or by calling
 * video_splitter_DeletePicture.
 *
 * If VLC_SUCCESS is not returned, pp_picture values are undefined.
 */
static inline int video_splitter_NewPicture(video_splitter_t *splitter,
                                            picture_t *pics[])
{
    for (int i = 0; i < splitter->i_output; i++) {
        pics[i] = picture_NewFromFormat(&splitter->p_output[i].fmt);
        if (pics[i] == NULL) {
            for (int j = 0; j < i; j++)
                picture_Release(pics[j]);

            msg_Warn(splitter, "can't get output pictures");
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

/**
 * It will release an array of pictures created by video_splitter_NewPicture.
 * Provided for convenience.
 */
static inline void video_splitter_DeletePicture( video_splitter_t *p_splitter,
                                                 picture_t *pp_picture[] )
{
    for (int i = 0; i < p_splitter->i_output; i++)
        picture_Release(pp_picture[i]);
}

/* */
video_splitter_t * video_splitter_New( vlc_object_t *, const char *psz_name, const video_format_t * );
void video_splitter_Delete( video_splitter_t * );

static inline int video_splitter_Filter( video_splitter_t *p_splitter,
                                         picture_t *pp_dst[], picture_t *p_src )
{
    return p_splitter->pf_filter( p_splitter, pp_dst, p_src );
}

static inline int video_splitter_Mouse(video_splitter_t *splitter, int index,
                                       struct vlc_window_mouse_event *ev)
{
    return (splitter->mouse != NULL)
        ? splitter->mouse(splitter, index, ev) : VLC_SUCCESS;
}

#endif /* VLC_VIDEO_SPLITTER_H */

