/*****************************************************************************
 * osd.c - The OSD Menu core code.
 *****************************************************************************
 * Copyright (C) 2005 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc_keys.h>
#include <vlc_osd.h>

#undef OSD_MENU_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void osd_UpdateState( osd_menu_state_t *, int, int, int, int, picture_t * );
static inline osd_state_t *osd_VolumeStateChange( osd_state_t *, int );
static int osd_VolumeStep( vlc_object_t *, int, int );
static vlc_bool_t osd_isVisible( osd_menu_t *p_osd );

static vlc_bool_t osd_isVisible( osd_menu_t *p_osd )
{
    vlc_value_t val;

    var_Get( p_osd, "osd-menu-visible", &val );
    return val.b_bool;
}

/*****************************************************************************
 * OSD menu Funtions
 *****************************************************************************/
osd_menu_t *__osd_MenuCreate( vlc_object_t *p_this, const char *psz_file )
{
    osd_menu_t  *p_osd = NULL;
    vlc_value_t lockval;
    int         i_volume = 0;
    int         i_steps = 0;

    /* to be sure to avoid multiple creation */
    var_Create( p_this->p_libvlc, "osd_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        vlc_value_t val;

        msg_Dbg( p_this, "creating OSD menu object" );
        if( ( p_osd = vlc_object_create( p_this, VLC_OBJECT_OSDMENU ) ) == NULL )
        {
            msg_Err( p_this, "out of memory" );
            vlc_mutex_unlock( lockval.p_address );
            return NULL;
        }

        /* Parse configuration file */
        if( osd_ConfigLoader( p_this, psz_file, &p_osd ) )
            goto error;

        /* Setup default button (first button) */
        p_osd->p_state->p_visible = p_osd->p_button;
        p_osd->p_state->p_visible->p_current_state =
            osd_StateChange( p_osd->p_state->p_visible->p_states, OSD_BUTTON_SELECT );
        p_osd->i_width  = p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch;
        p_osd->i_height = p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines;

        /* Update the volume state images to match the current volume */
        i_volume = config_GetInt( p_this, "volume" );
        i_steps = osd_VolumeStep( p_this, i_volume, p_osd->p_state->p_volume->i_ranges );
        p_osd->p_state->p_volume->p_current_state = osd_VolumeStateChange( p_osd->p_state->p_volume->p_states, i_steps );

        /* Initialize OSD state */
        osd_UpdateState( p_osd->p_state, p_osd->i_x, p_osd->i_y,
                         p_osd->i_width, p_osd->i_height, NULL );

        vlc_object_yield( p_osd );
        vlc_object_attach( p_osd, p_this->p_vlc );

        /* Signal when an update of OSD menu is needed */
        var_Create( p_osd, "osd-menu-update", VLC_VAR_BOOL );
        var_Create( p_osd, "osd-menu-visible", VLC_VAR_BOOL );

        val.b_bool = VLC_FALSE;
        var_Set( p_osd, "osd-menu-update", val );
        var_Set( p_osd, "osd-menu-visible", val );
    }
    vlc_mutex_unlock( lockval.p_address );
    return p_osd;

error:
    msg_Err( p_this, "creating OSD menu object failed" );
    vlc_mutex_unlock( lockval.p_address );
    vlc_object_destroy( p_osd );
    return NULL;
}

void __osd_MenuDelete( vlc_object_t *p_this, osd_menu_t *p_osd )
{
    vlc_value_t lockval;

    if( !p_osd || !p_this ) return;

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    vlc_object_release( p_osd );
    if( p_osd->i_refcount > 0 )
    {
        vlc_mutex_unlock( lockval.p_address );
        return;
    }

    var_Destroy( p_osd, "osd-menu-visible" );
    var_Destroy( p_osd, "osd-menu-update" );

    osd_ConfigUnload( p_this, &p_osd );
    vlc_object_detach( p_osd );
    vlc_object_destroy( p_osd );
    p_osd = NULL;

    vlc_mutex_unlock( lockval.p_address );
}

osd_state_t *__osd_StateChange( osd_state_t *p_states, const int i_state )
{
    osd_state_t *p_current = p_states;
    osd_state_t *p_temp = NULL;
    int i = 0;

    for( i=0; p_current != NULL; i++ )
    {
        if( p_current->i_state == i_state )
            return p_current;
        p_temp = p_current->p_next;
        p_current = p_temp;
    }
    return p_states;
}

/* The volume can be modified in another interface while the OSD Menu 
 * has not been instantiated yet. This routines updates the "volume OSD menu item"
 * to reflect the current state of the GUI.
 */
static inline osd_state_t *osd_VolumeStateChange( osd_state_t *p_current, int i_steps )
{
    osd_state_t *p_temp = NULL;
    int i;

    if( i_steps < 0 ) i_steps = 0;

    for( i=0; (i < i_steps) && (p_current != NULL); i++ )
    {
        p_temp = p_current->p_next;
        if( !p_temp ) return p_current;
        p_current = p_temp;
    }
    return (!p_temp) ? p_current : p_temp;
}

/* Update the state of the OSD Menu */
static void osd_UpdateState( osd_menu_state_t *p_state, int i_x, int i_y,
        int i_width, int i_height, picture_t *p_pic )
{
    p_state->i_x = i_x;
    p_state->i_y = i_y;
    p_state->i_width = i_width;
    p_state->i_height = i_height;
    p_state->p_pic = p_pic;
}

void __osd_MenuShow( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuNext failed" );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "menu on" );
#endif
    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range ) 
            p_button->p_current_state = osd_StateChange( p_button->p_states, OSD_BUTTON_UNSELECT );
        p_osd->p_state->p_visible = p_osd->p_button;

        if( !p_osd->p_state->p_visible->b_range ) 
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible->p_states, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
    }
    osd_SetMenuVisible( p_osd, VLC_TRUE );

    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

void __osd_MenuHide( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    vlc_value_t lockval;

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuNext failed" );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "menu off" );
#endif
    osd_UpdateState( p_osd->p_state,
                p_osd->p_state->i_x, p_osd->p_state->i_y,
                0, 0, NULL );
    osd_SetMenuUpdate( p_osd, VLC_TRUE );

    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

void __osd_MenuActivate( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuNext failed" );
        return;
    }

    if( osd_isVisible( p_osd ) == VLC_FALSE )
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "select" );
#endif
    p_button = p_osd->p_state->p_visible;
    /*
     * Is there a menu item above or below? If so, then select it.
     */
    if( p_button && p_button->p_up)
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        vlc_mutex_unlock( lockval.p_address );
        __osd_MenuUp( p_this );   /* "menu select" means go to menu item above. */
        return;
    }
    if( p_button && p_button->p_down)
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        vlc_mutex_unlock( lockval.p_address );
        __osd_MenuDown( p_this ); /* "menu select" means go to menu item below. */
        return;
    }

    if( p_button && !p_button->b_range )
    {
        p_button->p_current_state = osd_StateChange( p_button->p_states, OSD_BUTTON_PRESSED );
        osd_UpdateState( p_osd->p_state,
                p_button->i_x, p_button->i_y,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_button->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
        osd_SetMenuVisible( p_osd, VLC_TRUE );
        osd_SetKeyPressed( VLC_OBJECT(p_osd->p_vlc), config_GetInt( p_osd, p_button->psz_action ) );
#if defined(OSD_MENU_DEBUG)
        msg_Dbg( p_osd, "select (%d, %s)", config_GetInt( p_osd, p_button->psz_action ), p_button->psz_action );
#endif
    }
    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

void __osd_MenuNext( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuNext failed" );
        return;
    }

    if( osd_isVisible( p_osd ) == VLC_FALSE )
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range ) 
            p_button->p_current_state = osd_StateChange( p_button->p_states, OSD_BUTTON_UNSELECT );
        if( p_button->p_next )
            p_osd->p_state->p_visible = p_button->p_next;
        else
            p_osd->p_state->p_visible = p_osd->p_button;

        if( !p_osd->p_state->p_visible->b_range ) 
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible->p_states, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state, 
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction right [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

void __osd_MenuPrev( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuPrev failed" );
        return;
    }

    if( osd_isVisible( p_osd ) == VLC_FALSE )
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range ) 
            p_button->p_current_state = osd_StateChange( p_button->p_states, OSD_BUTTON_UNSELECT );
        if( p_button->p_prev )
            p_osd->p_state->p_visible = p_button->p_prev;
        else
            p_osd->p_state->p_visible = p_osd->p_last_button;

        if( !p_osd->p_state->p_visible->b_range ) 
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible->p_states, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state, 
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction left [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

void __osd_MenuUp( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;
#if defined(OSD_MENU_DEBUG)
    vlc_value_t val;
#endif

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuDown failed" );
        return;
    }

    if( osd_isVisible( p_osd ) == VLC_FALSE )
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range ) 
        {
            p_button->p_current_state = osd_StateChange( p_button->p_states, OSD_BUTTON_SELECT );
            if( p_button->p_up )
                p_osd->p_state->p_visible = p_button->p_up;
        }

        if( p_button->b_range && p_osd->p_state->p_visible->b_range ) 
        {
            osd_state_t *p_temp = p_osd->p_state->p_visible->p_current_state;
            if( p_temp && p_temp->p_next )
                p_osd->p_state->p_visible->p_current_state = p_temp->p_next;
        }
        else if( !p_osd->p_state->p_visible->b_range )
        {
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible->p_states, OSD_BUTTON_SELECT );
        }

        osd_UpdateState( p_osd->p_state, 
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
        /* If this is a range style action with associated images of only one state, 
            * then perform "menu select" on every menu navigation
            */
        if( p_button->b_range ) 
        {
            osd_SetKeyPressed( VLC_OBJECT(p_osd->p_vlc), config_GetInt(p_osd, p_button->psz_action) );
#if defined(OSD_MENU_DEBUG)
            msg_Dbg( p_osd, "select (%d, %s)", val.i_int, p_button->psz_action );
#endif
        }
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction up [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

void __osd_MenuDown( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;
#if defined(OSD_MENU_DEBUG)
    vlc_value_t val;
#endif

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "osd_MenuDown failed" );
        return;
    }

    if( osd_isVisible( p_osd ) == VLC_FALSE )
    {
        vlc_object_release( (vlc_object_t*) p_osd );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range ) 
        {
            p_button->p_current_state = osd_StateChange( p_button->p_states, OSD_BUTTON_SELECT );
            if( p_button->p_down )
                p_osd->p_state->p_visible = p_button->p_down;
        }

        if( p_button->b_range && p_osd->p_state->p_visible->b_range ) 
        {
            osd_state_t *p_temp = p_osd->p_state->p_visible->p_current_state;
            if( p_temp && p_temp->p_prev )
                p_osd->p_state->p_visible->p_current_state = p_temp->p_prev;
        }
        else if( !p_osd->p_state->p_visible->b_range )
        {
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible->p_states, OSD_BUTTON_SELECT );
        }

        osd_UpdateState( p_osd->p_state, 
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
        /* If this is a range style action with associated images of only one state,
         * then perform "menu select" on every menu navigation
         */
        if( p_button->b_range ) 
        {
            osd_SetKeyPressed( VLC_OBJECT(p_osd->p_vlc), config_GetInt(p_osd, p_button->psz_action_down) );
#if defined(OSD_MENU_DEBUG)
            msg_Dbg( p_osd, "select (%d, %s)", val.i_int, p_button->psz_action_down );
#endif
        }
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction down [button %s]", p_osd->p_state->p_visible->psz_action ); 
#endif

    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}

static int osd_VolumeStep( vlc_object_t *p_this, int i_volume, int i_steps )
{
    int i_volume_step = 0;

    i_volume_step = config_GetInt( p_this->p_vlc, "volume-step" );
    return (i_volume/i_volume_step);
}

/**
 * Display current audio volume bitmap
 *
 * The OSD Menu audio volume bar is updated to reflect the new audio volume. Call this function
 * when the audio volume is updated outside the OSD menu command "menu up", "menu down" or "menu select".
 */
void __osd_Volume( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = NULL;
    osd_button_t *p_button = NULL;
    vlc_value_t lockval;
    int i_volume = 0;
    int i_steps = 0;

    if( ( p_osd = vlc_object_find( p_this, VLC_OBJECT_OSDMENU, FIND_ANYWHERE ) ) == NULL )
    {
        msg_Err( p_this, "OSD menu volume update failed" );
        return;
    }

    var_Get( p_this->p_libvlc, "osd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    p_button = p_osd->p_state->p_volume;
    if( p_osd->p_state->p_volume ) 
        p_osd->p_state->p_visible = p_osd->p_state->p_volume;
    if( p_button && p_button->b_range )
    {
        /* Update the volume state images to match the current volume */
        i_volume = config_GetInt( p_this, "volume" );
        i_steps = osd_VolumeStep( p_this, i_volume, p_button->i_ranges );
        p_button->p_current_state = osd_VolumeStateChange( p_button->p_states, i_steps );

        osd_UpdateState( p_osd->p_state,
                p_button->i_x, p_button->i_y,
                p_button->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                p_button->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                p_button->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, VLC_TRUE );
        osd_SetMenuVisible( p_osd, VLC_TRUE );
    }
    vlc_object_release( (vlc_object_t*) p_osd );
    vlc_mutex_unlock( lockval.p_address );
}
