/*****************************************************************************
 * vlc_codec.h: codec related structures
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#ifndef _VLC_FILTER_H
#define _VLC_FILTER_H 1

/**
 * \file
 * This file defines the structure and types used by video and audio filters
 */

typedef struct filter_owner_sys_t filter_owner_sys_t;

/**
 * \defgroup filter Filter
 *
 * The structure describing a filter
 *
 * @{
 */
struct filter_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    filter_sys_t *     p_sys;

    void                ( * pf_video_blend )  ( filter_t *, picture_t *,
                                                picture_t *, picture_t *,
                                                int, int );
    picture_t *         ( * pf_video_filter ) ( filter_t *, picture_t * );

    subpicture_t *      ( *pf_render_string ) ( filter_t *, block_t * );

    /* Input format */
    es_format_t         fmt_in;

    /* Output format of filter */
    es_format_t         fmt_out;

    /*
     * Buffers allocation
     */

    /* Audio output callbacks */
    aout_buffer_t * ( * pf_aout_buffer_new) ( filter_t *, int );
    void            ( * pf_aout_buffer_del) ( filter_t *, aout_buffer_t * );

    /* Video output callbacks */
    picture_t     * ( * pf_vout_buffer_new) ( filter_t * );
    void            ( * pf_vout_buffer_del) ( filter_t *, picture_t * );
    void            ( * pf_picture_link)    ( filter_t *, picture_t * );
    void            ( * pf_picture_unlink)  ( filter_t *, picture_t * );

    /* SPU output callbacks */
    subpicture_t *  ( * pf_spu_buffer_new) ( filter_t * );
    void            ( * pf_spu_buffer_del) ( filter_t *, subpicture_t * );

    /* Private structure for the owner of the decoder */
    filter_owner_sys_t *p_owner;
};

/**
 * @}
 */

#endif /* _VLC_FILTER_H */
