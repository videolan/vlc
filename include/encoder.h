/*****************************************************************************
 * encoder.h :
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: encoder.h,v 1.1 2003/01/22 10:41:57 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#ifndef _ENCODER_H
#define _ENCODER_H

typedef struct encoder_sys_t encoder_sys_t;

typedef struct video_encoder_s
{
    VLC_COMMON_MEMBERS

    module_t * p_module;

    vlc_fourcc_t  i_codec;          /* in */
    vlc_fourcc_t  i_chroma;         /* in/out */
    int           i_width;          /* in/out */
    int           i_height;         /* in/out */
    int           i_aspect;         /* in/out */

    size_t        i_buffer_size;    /* in/out */

    encoder_sys_t *p_sys;

    int  (*pf_init)     ( struct video_encoder_s *p_enc );
    int  (*pf_encode)   ( struct video_encoder_s *p_enc, picture_t *p_pic, void *p_data, size_t *pi_data );
    void (*pf_end)      ( struct video_encoder_s *p_enc );

} video_encoder_t;

/*
 * Video decoder:
 *
 *  = at loading a video decoder must
 *      * see if i_codec is supporte, if not => failling
 *      * modify i_width/i_height/i_chroma/i_aspect if required (for example,
 *          if a video codec required %8 size)
 *      * init/check the library
 *      * set pf_init, pf_encode and pf_end
 *      * set i_buffer_size to the max buffer size required to output a single frame
 *
 *  = pf_init must
 *      * start encoding processing 
 *      * doesn't change any parameters (i_chroma/i_width/i_height/i_aspect)
 *      * check for passed parameters (no one for the moment)
 *
 *  = pf_encode must
 *      * encode a single frame
 *      * doesn't change any parameters (...)
 *      * doesn't look for passed paramters
 *
 *  = pf_end must
 *      * end the encoding process
 *      * revert all that pf_init (and pf_encode) has done (memory...)
 *
 * = at unloading, a video decoder must revert all that was done while loading
 *
 *  XXX: pf_init/pf_end could be called multiple time without the plugin unloaded.
 *  XXX: all memory allocated by video encoder MUST be unallocated by video encoder
 *
 */

#endif
