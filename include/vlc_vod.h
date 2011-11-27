/*****************************************************************************
 * vlc_vod.h: interface for VoD server modules
 *****************************************************************************
 * Copyright (C) 2000, 2001 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

#ifndef VLC_VOD_H
#define VLC_VOD_H 1

/**
 * \file
 * This file defines an interface for VOD server modules in vlc
 */

/**
 * \defgroup vod Video On Demand (VOD)
 * \ingroup server
 * Video On Demand (VOD) functionality is provided from VLM.
 * @{
 */

struct vod_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t  *p_module;
    vod_sys_t *p_sys;

    vod_media_t * (*pf_media_new)   ( vod_t *, const char *, input_item_t * );
    void          (*pf_media_del)   ( vod_t *, vod_media_t * );

    /* Owner properties */
    int (*pf_media_control) ( void *, vod_media_t *, const char *, int, va_list );
    void *p_data;
};

static inline int vod_MediaControl( vod_t *p_vod, vod_media_t *p_media,
                                    const char *psz_id, int i_query, ... )
{
    va_list args;
    int i_result;

    if( !p_vod->pf_media_control ) return VLC_EGENERIC;

    va_start( args, i_query );
    i_result = p_vod->pf_media_control( p_vod->p_data, p_media, psz_id,
                                        i_query, args );
    va_end( args );
    return i_result;
}

enum vod_query_e
{
    VOD_MEDIA_PLAY,         /* arg1= char *, arg2= int64_t *, res=    */
    VOD_MEDIA_PAUSE,        /* arg1= int64_t *      res=    */
    VOD_MEDIA_STOP,         /* arg1=                res=can fail    */
    VOD_MEDIA_SEEK,         /* arg1= double         res=    */
    VOD_MEDIA_REWIND,       /* arg1= double         res=    */
    VOD_MEDIA_FORWARD,      /* arg1= double         res=    */
};

/**
 * @}
 */

#endif
