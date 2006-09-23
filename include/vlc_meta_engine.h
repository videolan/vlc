/*****************************************************************************
 * vlc_meta_engine.h: meta engine module.
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea A videolan D org>
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

#ifndef _VLC_META_ENGINE_H
#define _VLC_META_ENGINE_H

#include "vlc_meta.h"

#define VLC_META_ENGINE_TITLE           0x00000001
#define VLC_META_ENGINE_AUTHOR          0x00000002
#define VLC_META_ENGINE_ARTIST          0x00000004
#define VLC_META_ENGINE_GENRE           0x00000008
#define VLC_META_ENGINE_COPYRIGHT       0x00000010
#define VLC_META_ENGINE_COLLECTION      0x00000020
#define VLC_META_ENGINE_SEQ_NUM         0x00000040
#define VLC_META_ENGINE_DESCRIPTION     0x00000080
#define VLC_META_ENGINE_RATING          0x00000100
#define VLC_META_ENGINE_DATE            0x00000200
#define VLC_META_ENGINE_URL             0x00000400
#define VLC_META_ENGINE_LANGUAGE        0x00000800

#define VLC_META_ENGINE_ART_URL         0x00001000

#define VLC_META_ENGINE_MB_ARTIST_ID    0x00002000
#define VLC_META_ENGINE_MB_RELEASE_ID   0x00004000
#define VLC_META_ENGINE_MB_TRACK_ID     0x00008000
#define VLC_META_ENGINE_MB_TRM_ID       0x00010000

typedef struct meta_engine_sys_t meta_engine_sys_t;

struct meta_engine_t
{
    VLC_COMMON_MEMBERS

    module_t *p_module;

    uint32_t i_mandatory; /**< Stuff which we really need to get */
    uint32_t i_optional; /**< Stuff which we'd like to have */

    input_item_t *p_item;
};

#endif
