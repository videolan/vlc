/*****************************************************************************
 * filter_common.h: Common filter functions
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003 VideoLAN
 * $Id: filter_common.h,v 1.2 2003/01/17 16:18:03 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#define ALLOCATE_DIRECTBUFFERS( i_max ) \
    /* Try to initialize i_max direct buffers */                              \
    while( I_OUTPUTPICTURES < ( i_max ) )                                     \
    {                                                                         \
        p_pic = NULL;                                                         \
                                                                              \
        /* Find an empty picture slot */                                      \
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )          \
        {                                                                     \
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )       \
            {                                                                 \
                p_pic = p_vout->p_picture + i_index;                          \
                break;                                                        \
            }                                                                 \
        }                                                                     \
                                                                              \
        if( p_pic == NULL )                                                   \
        {                                                                     \
            break;                                                            \
        }                                                                     \
                                                                              \
        /* Allocate the picture */                                            \
        vout_AllocatePicture( p_vout, p_pic,                                  \
                              p_vout->output.i_width,                         \
                              p_vout->output.i_height,                        \
                              p_vout->output.i_chroma );                      \
                                                                              \
        if( !p_pic->i_planes )                                                \
        {                                                                     \
            break;                                                            \
        }                                                                     \
                                                                              \
        p_pic->i_status = DESTROYED_PICTURE;                                  \
        p_pic->i_type   = DIRECT_PICTURE;                                     \
                                                                              \
        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;                         \
                                                                              \
        I_OUTPUTPICTURES++;                                                   \
    }                                                                         \

#define ADD_CALLBACKS( newvout, handler ) \
    var_AddCallback( newvout, "mouse-x", SendEvents, p_vout );                \
    var_AddCallback( newvout, "mouse-y", SendEvents, p_vout );                \
    var_AddCallback( newvout, "mouse-moved", SendEvents, p_vout );            \
    var_AddCallback( newvout, "mouse-clicked", SendEvents, p_vout );          \
    var_AddCallback( newvout, "key-pressed", SendEvents, p_vout )

#define DEL_CALLBACKS( newvout, handler ) \
    var_DelCallback( newvout, "mouse-x", SendEvents, p_vout );                \
    var_DelCallback( newvout, "mouse-y", SendEvents, p_vout );                \
    var_DelCallback( newvout, "mouse-moved", SendEvents, p_vout );            \
    var_DelCallback( newvout, "mouse-clicked", SendEvents, p_vout );          \
    var_DelCallback( newvout, "key-pressed", SendEvents, p_vout )

