/*****************************************************************************
 * vlc_spu.h: spu_t definition and functions.
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_SPU_H
#define VLC_SPU_H 1

#include <vlc_subpicture.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************
 * Base SPU structures
 **********************************************************************/
/**
 * \defgroup spu Subpicture Unit
 * This module describes the programming interface for the subpicture unit.
 * It includes functions allowing to create/destroy an spu, and render
 * subpictures.
 * @{
 */

typedef struct spu_private_t spu_private_t;

/* Default subpicture channel ID */
#define SPU_DEFAULT_CHANNEL (1)

/**
 * Subpicture unit descriptor
 */
struct spu_t
{
    VLC_COMMON_MEMBERS

    spu_private_t *p;
};

VLC_API spu_t * spu_Create( vlc_object_t * );
#define spu_Create(a) spu_Create(VLC_OBJECT(a))
VLC_API void spu_Destroy( spu_t * );

/**
 * This function sends a subpicture to the spu_t core.
 * 
 * You cannot use the provided subpicture anymore. The spu_t core
 * will destroy it at its convenience.
 */
VLC_API void spu_PutSubpicture( spu_t *, subpicture_t * );

/**
 * This function will return an unique subpicture containing the OSD and
 * subtitles visibles at the requested date.
 *
 * \param p_chroma_list is a list of supported chroma for the output (can be NULL)
 * \param p_fmt_dst is the format of the picture on which the return subpicture will be rendered.
 * \param p_fmt_src is the format of the original(source) video.
 *
 * The returned value if non NULL must be released by subpicture_Delete().
 */
VLC_API subpicture_t * spu_Render( spu_t *, const vlc_fourcc_t *p_chroma_list, const video_format_t *p_fmt_dst, const video_format_t *p_fmt_src, mtime_t render_subtitle_date, mtime_t render_osd_date, bool ignore_osd );

/**
 * It registers a new SPU channel.
 */
VLC_API int spu_RegisterChannel( spu_t * );

/**
 * It clears all subpictures associated to a SPU channel.
 */
VLC_API void spu_ClearChannel( spu_t *, int );

/**
 * It changes the sub sources list
 */
VLC_API void spu_ChangeSources( spu_t *, const char * );

/**
 * It changes the sub filters list
 */
VLC_API void spu_ChangeFilters( spu_t *, const char * );

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* VLC_SPU_H */

