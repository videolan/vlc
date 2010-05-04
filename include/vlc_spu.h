/*****************************************************************************
 * vlc_spu.h: spu_t definition and functions.
 *****************************************************************************
 * Copyright (C) 1999-2010 the VideoLAN team
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

VLC_EXPORT( spu_t *, spu_Create, ( vlc_object_t * ) );
#define spu_Create(a) spu_Create(VLC_OBJECT(a))
VLC_EXPORT( int, spu_Init, ( spu_t * ) );
VLC_EXPORT( void, spu_Destroy, ( spu_t * ) );
void spu_Attach( spu_t *, vlc_object_t *, bool );

/**
 * This function sends a subpicture to the spu_t core.
 * 
 * You cannot use the provided subpicture anymore. The spu_t core
 * will destroy it at its convenience.
 */
VLC_EXPORT( void, spu_DisplaySubpicture, ( spu_t *, subpicture_t * ) );

/**
 * This function asks the spu_t core a list of subpictures to display.
 *
 * The returned list can only be used by spu_RenderSubpictures.
 */
VLC_EXPORT( subpicture_t *, spu_SortSubpictures, ( spu_t *, mtime_t render_subtitle_date, bool b_subtitle_only ) );

/**
 * This function renders a list of subpicture_t on the provided picture.
 *
 * \param p_fmt_dst is the format of the destination picture.
 * \param p_fmt_src is the format of the original(source) video.
 */
VLC_EXPORT( void, spu_RenderSubpictures, ( spu_t *,  picture_t *, const video_format_t *p_fmt_dst, subpicture_t *p_list, const video_format_t *p_fmt_src, mtime_t render_subtitle_date ) );

/**
 * It registers a new SPU channel.
 */
VLC_EXPORT( int, spu_RegisterChannel, ( spu_t * ) );

/**
 * It clears all subpictures associated to a SPU channel.
 */
VLC_EXPORT( void, spu_ClearChannel, ( spu_t *, int ) );

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* VLC_SPU_H */

