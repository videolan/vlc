/*****************************************************************************
 * mosaic.h:
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

typedef struct bridged_es_t
{
    es_format_t fmt;
    picture_t *p_picture;
    picture_t **pp_last;
    vlc_bool_t b_empty;
    char *psz_id;
} bridged_es_t;

typedef struct bridge_t
{
    bridged_es_t **pp_es;
    int i_es_num;
} bridge_t;

#define GetBridge(a) __GetBridge( VLC_OBJECT(a) )
static bridge_t *__GetBridge( vlc_object_t *p_object )
{
    libvlc_t *p_libvlc = p_object->p_libvlc;
    bridge_t *p_bridge;
    vlc_value_t val;

    if( var_Get( p_libvlc, "mosaic-struct", &val ) != VLC_SUCCESS )
    {
        p_bridge = NULL;
    }
    else
    {
        p_bridge = val.p_address;
    }    

    return p_bridge;
}

