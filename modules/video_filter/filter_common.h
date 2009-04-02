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

/**
 * Initialize i_max direct buffers for a vout_thread_t.
 */
static inline void vout_filter_AllocateDirectBuffers( vout_thread_t *p_vout, int i_max )
{
    I_OUTPUTPICTURES = 0;

    /* Try to initialize i_max direct buffers */
    while( I_OUTPUTPICTURES < i_max )
    {
        picture_t *p_pic = NULL;

        /* Find an empty picture slot */
        for( int i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        if( p_pic == NULL )
            break;

        /* Allocate the picture */
        vout_AllocatePicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                              p_vout->output.i_width,
                              p_vout->output.i_height,
                              p_vout->output.i_aspect );

        if( !p_pic->i_planes )
            break;

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }
}

static inline void vout_filter_ReleaseDirectBuffers( vout_thread_t *p_vout )
{
    for( int i_index = I_OUTPUTPICTURES-1; i_index >= 0; i_index-- )
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
}

/**
 * Internal helper to forward an event from p_this to p_data
 */
static inline int ForwardEvent( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    vlc_object_t *p_dst = (vlc_object_t*)p_data;

    return var_Set( p_dst, psz_var, newval );
}
/**
 * Internal helper to forward fullscreen event from p_this to p_data.
 */
static inline int ForwardFullscreen( vlc_object_t *p_this, char const *psz_var,
                                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    vlc_object_t *p_dst = (vlc_object_t*)p_data;

    if( !var_GetBool( p_dst, "fullscreen" ) != !newval.b_bool )
        return var_SetBool( p_dst, psz_var, newval.b_bool );
    return VLC_SUCCESS;
}
/**
 * Install/remove all callbacks needed for proper event handling inside
 * a vout-filter.
 */
static inline void vout_filter_SetupChild( vout_thread_t *p_parent, vout_thread_t *p_child,
                                           vlc_callback_t pf_mouse_event,
                                           vlc_callback_t pf_fullscreen_up,
                                           vlc_callback_t pf_fullscreen_down,
                                           bool b_init )
{
    int (*pf_execute)( vlc_object_t *, const char *, vlc_callback_t, void * );

    if( b_init )
        pf_execute = __var_AddCallback;
    else
        pf_execute = __var_DelCallback;

    /* */
    if( !pf_mouse_event )
        pf_mouse_event = ForwardEvent;
    pf_execute( VLC_OBJECT(p_child), "mouse-x",           pf_mouse_event, p_parent );
    pf_execute( VLC_OBJECT(p_child), "mouse-y",           pf_mouse_event, p_parent );
    pf_execute( VLC_OBJECT(p_child), "mouse-moved",       pf_mouse_event, p_parent );
    pf_execute( VLC_OBJECT(p_child), "mouse-clicked",     pf_mouse_event, p_parent );
    pf_execute( VLC_OBJECT(p_child), "mouse-button-down", pf_mouse_event, p_parent );

    /* */
    pf_execute( VLC_OBJECT(p_parent), "autoscale",    ForwardEvent, p_child );
    pf_execute( VLC_OBJECT(p_parent), "scale",        ForwardEvent, p_child );
    pf_execute( VLC_OBJECT(p_parent), "aspect-ratio", ForwardEvent, p_child );
    pf_execute( VLC_OBJECT(p_parent), "crop",         ForwardEvent, p_child );

    /* */
    if( !pf_fullscreen_up )
        pf_fullscreen_up = ForwardFullscreen;
    if( !pf_fullscreen_down )
        pf_fullscreen_down = ForwardFullscreen;
    pf_execute( VLC_OBJECT(p_child),  "fullscreen", pf_fullscreen_up,   p_parent );
    pf_execute( VLC_OBJECT(p_parent), "fullscreen", pf_fullscreen_down, p_child );
}

#define vout_filter_AddChild( a, b, c ) vout_filter_SetupChild( a, b, c, NULL, NULL, true )
#define vout_filter_DelChild( a, b, c ) vout_filter_SetupChild( a, b, c, NULL, NULL, false )

