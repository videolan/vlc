/*****************************************************************************
 * filter_common.h: Common filter functions
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
        vout_AllocatePicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma, \
                              p_vout->output.i_width,                         \
                              p_vout->output.i_height,                        \
                              p_vout->output.i_aspect );                      \
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

/*****************************************************************************
 * SetParentVal: forward variable value to parent whithout triggering the
 *               callback
 *****************************************************************************/
static int SetParentVal( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Change( (vlc_object_t *)p_data, psz_var, VLC_VAR_SETVALUE,
                 &newval, NULL );
    return VLC_SUCCESS;
}

#define ADD_CALLBACKS( newvout, handler ) \
    var_AddCallback( newvout, "fullscreen", SetParentVal, p_vout );           \
    var_AddCallback( newvout, "mouse-x", SendEvents, p_vout );                \
    var_AddCallback( newvout, "mouse-y", SendEvents, p_vout );                \
    var_AddCallback( newvout, "mouse-moved", SendEvents, p_vout );            \
    var_AddCallback( newvout, "mouse-clicked", SendEvents, p_vout );

#define DEL_CALLBACKS( newvout, handler ) \
    var_DelCallback( newvout, "fullscreen", SetParentVal, p_vout );           \
    var_DelCallback( newvout, "mouse-x", SendEvents, p_vout );                \
    var_DelCallback( newvout, "mouse-y", SendEvents, p_vout );                \
    var_DelCallback( newvout, "mouse-moved", SendEvents, p_vout );            \
    var_DelCallback( newvout, "mouse-clicked", SendEvents, p_vout );

#define ADD_PARENT_CALLBACKS( handler ) \
    var_AddCallback( p_vout, "fullscreen", handler, NULL );                   \
    var_AddCallback( p_vout, "aspect-ratio", handler, NULL );                 \
    var_AddCallback( p_vout, "crop", handler, NULL );

#define DEL_PARENT_CALLBACKS( handler ) \
    var_DelCallback( p_vout, "fullscreen", handler, NULL );                   \
    var_DelCallback( p_vout, "aspect-ratio", handler, NULL );                 \
    var_DelCallback( p_vout, "crop", handler, NULL );

static int  SendEventsToChild( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
