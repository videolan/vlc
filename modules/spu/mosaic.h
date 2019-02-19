/*****************************************************************************
 * mosaic.h:
 *****************************************************************************
 * Copyright (C) 2004-2008 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

typedef struct bridged_es_t
{
    es_format_t fmt;
    picture_t *p_picture;
    picture_t **pp_last;
    bool b_empty;
    char *psz_id;

    int i_alpha;
    int i_x;
    int i_y;
} bridged_es_t;

typedef struct bridge_t
{
    bridged_es_t **pp_es;
    int i_es_num;
} bridge_t;

static bridge_t *GetBridge( vlc_object_t *p_object )
{
    return var_GetAddress(VLC_OBJECT(vlc_object_instance(p_object)),
                          "mosaic-struct");
}
#define GetBridge(a) GetBridge( VLC_OBJECT(a) )
