/*****************************************************************************
 * osd.c - The OSD Menu core code.
 *****************************************************************************
 * Copyright (C) 2005-2008 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_keys.h>
#include <vlc_osd.h>
#include <vlc_image.h>
#include <vlc_modules.h>

#include "libvlc.h"

#undef OSD_MENU_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void osd_UpdateState( osd_menu_state_t *, int, int, int, int, picture_t * );
static inline osd_state_t *osd_VolumeStateChange( osd_state_t *, int );
static int osd_VolumeStep( vlc_object_t *, int, int );
static bool osd_ParserLoad( osd_menu_t *, const char * );
static void osd_ParserUnload( osd_menu_t * );

static inline bool osd_isVisible( osd_menu_t *p_osd )
{
    return var_GetBool( p_osd, "osd-menu-visible" );
}

static vlc_mutex_t osd_mutex = VLC_STATIC_MUTEX;

/*****************************************************************************
 * Wrappers for loading and unloading osd parser modules.
 *****************************************************************************/
static bool osd_ParserLoad( osd_menu_t *p_menu, const char *psz_file )
{
    /* Stuff needed for Parser */
    p_menu->psz_file = strdup( psz_file );
    p_menu->p_image = image_HandlerCreate( p_menu );
    if( !p_menu->p_image || !p_menu->psz_file )
    {
        msg_Err( p_menu, "unable to load images, aborting .." );
        return false;
    }
    else
    {
        const char *psz_type;
        const char *psz_ext = strrchr( p_menu->psz_file, '.' );

        if( psz_ext && !strcmp( psz_ext, ".cfg") )
            psz_type = "import-osd";
        else
            psz_type = "import-osd-xml";

        p_menu->p_parser = module_need( p_menu, "osd parser",
                                        psz_type, true );
        if( !p_menu->p_parser )
        {
            return false;
        }
    }
    return true;
}

static void osd_ParserUnload( osd_menu_t *p_menu )
{
    if( p_menu->p_image )
        image_HandlerDelete( p_menu->p_image );

    if( p_menu->p_parser )
        module_unneed( p_menu, p_menu->p_parser );

    free( p_menu->psz_file );
}

/**
 * Change state on an osd_button_t.
 *
 * This function selects the specified state and returns a pointer
 * vlc_custom_create to it. The following states are currently supported:
 * \see OSD_BUTTON_UNSELECT
 * \see OSD_BUTTON_SELECT
 * \see OSD_BUTTON_PRESSED
 */
static osd_state_t *osd_StateChange( osd_button_t *p_button, const int i_state )
{
    osd_state_t *p_current = p_button->p_states;
    osd_state_t *p_temp = NULL;
    int i = 0;

    for( i= 0; p_current != NULL; i++ )
    {
        if( p_current->i_state == i_state )
        {
            p_button->i_x = p_current->i_x;
            p_button->i_y = p_current->i_y;
            p_button->i_width = p_current->i_width;
            p_button->i_height = p_current->i_height;
            return p_current;
        }
        p_temp = p_current->p_next;
        p_current = p_temp;
    }
    return p_button->p_states;
}

#undef osd_MenuCreate
/*****************************************************************************
 * OSD menu Funtions
 *****************************************************************************/
osd_menu_t *osd_MenuCreate( vlc_object_t *p_this, const char *psz_file )
{
    osd_menu_t  *p_osd = NULL;
    vlc_value_t val;
    int         i_volume = 0;
    int         i_steps = 0;

    /* to be sure to avoid multiple creation */
    vlc_mutex_lock( &osd_mutex );

    var_Create( p_this->p_libvlc, "osd-object", VLC_VAR_ADDRESS );
    var_Get( p_this->p_libvlc, "osd-object", &val );
    if( val.p_address == NULL )
    {
        p_osd = vlc_custom_create( p_this->p_libvlc, sizeof( *p_osd ),
                                   "osd menu" );
        if( !p_osd )
            return NULL;

        p_osd->p_parser = NULL;

        /* Parse configuration file */
        if ( !osd_ParserLoad( p_osd, psz_file ) )
            goto error;
        if( !p_osd->p_state )
            goto error;

        /* Setup default button (first button) */
        p_osd->p_state->p_visible = p_osd->p_button;
        p_osd->p_state->p_visible->p_current_state =
            osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );
        p_osd->i_width  = p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch;
        p_osd->i_height = p_osd->p_state->p_visible->p_current_state->p_pic->p[Y_PLANE].i_visible_lines;

        if( p_osd->p_state->p_volume )
        {
            /* Update the volume state images to match the current volume */
            i_volume = config_GetInt( p_this, "volume" );
            i_steps = osd_VolumeStep( p_this, i_volume, p_osd->p_state->p_volume->i_ranges );
            p_osd->p_state->p_volume->p_current_state = osd_VolumeStateChange(
                                    p_osd->p_state->p_volume->p_states, i_steps );
        }
        /* Initialize OSD state */
        osd_UpdateState( p_osd->p_state, p_osd->i_x, p_osd->i_y,
                         p_osd->i_width, p_osd->i_height, NULL );

        /* Signal when an update of OSD menu is needed */
        var_Create( p_osd, "osd-menu-update", VLC_VAR_BOOL );
        var_Create( p_osd, "osd-menu-visible", VLC_VAR_BOOL );

        var_SetBool( p_osd, "osd-menu-update", false );
        var_SetBool( p_osd, "osd-menu-visible", false );

        var_SetAddress( p_this->p_libvlc, "osd-object", p_osd );
    }
    else
        p_osd = val.p_address;
    vlc_object_hold( p_osd );
    vlc_mutex_unlock( &osd_mutex );
    return p_osd;

error:
    vlc_mutex_unlock( &osd_mutex );
    osd_MenuDelete( p_this, p_osd );
    return NULL;
}

#undef osd_MenuDelete
void osd_MenuDelete( vlc_object_t *p_this, osd_menu_t *p_osd )
{
    if( !p_osd || !p_this ) return;

    vlc_mutex_lock( &osd_mutex );

    if( vlc_internals( VLC_OBJECT(p_osd) )->i_refcount == 1 )
    {
        var_Destroy( p_osd, "osd-menu-visible" );
        var_Destroy( p_osd, "osd-menu-update" );
        osd_ParserUnload( p_osd );
        var_SetAddress( p_this->p_libvlc, "osd-object", NULL );
    }

    vlc_object_release( p_osd );
    vlc_mutex_unlock( &osd_mutex );
}

static osd_menu_t *osd_Find( vlc_object_t *p_this, bool visible,
                             const char *func )
{
    osd_menu_t *menu;

    vlc_mutex_lock( &osd_mutex );
    menu = var_GetAddress( p_this->p_libvlc, "osd-object" );
    if( menu == NULL || ( visible && !osd_isVisible(menu) ) )
    {
        vlc_mutex_unlock( &osd_mutex );
        msg_Err( p_this, "%s failed", func );
    }
    return menu;
}
#define osd_Find(o) (osd_Find)(o, false, __func__)
#define osd_FindVisible(o) (osd_Find)(o, true, __func__)

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

#undef osd_MenuShow
void osd_MenuShow( vlc_object_t *p_this )
{
    osd_button_t *p_button = NULL;

    osd_menu_t *p_osd = osd_Find( p_this );
    if( p_osd == NULL )
        return;

#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "menu on" );
#endif
    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range )
            p_button->p_current_state = osd_StateChange( p_button, OSD_BUTTON_UNSELECT );
        p_osd->p_state->p_visible = p_osd->p_button;

        if( !p_osd->p_state->p_visible->b_range )
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
    }
    osd_SetMenuVisible( p_osd, true );

    vlc_mutex_unlock( &osd_mutex );
}

#undef osd_MenuHide
void osd_MenuHide( vlc_object_t *p_this )
{
    osd_menu_t *p_osd = osd_Find( p_this );
    if( p_osd == NULL )
        return;

#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "menu off" );
#endif
    osd_UpdateState( p_osd->p_state,
                p_osd->p_state->i_x, p_osd->p_state->i_y,
                0, 0, NULL );
    osd_SetMenuUpdate( p_osd, true );

    vlc_mutex_unlock( &osd_mutex );
}

#undef osd_MenuActivate
void osd_MenuActivate( vlc_object_t *p_this )
{
    osd_button_t *p_button = NULL;

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return;

#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "select" );
#endif
    p_button = p_osd->p_state->p_visible;
    /*
     * Is there a menu item above or below? If so, then select it.
     */
    if( p_button && p_button->p_up )
    {
        vlc_mutex_unlock( &osd_mutex );
        osd_MenuUp( p_this );   /* "menu select" means go to menu item above. */
        return;
    }
    if( p_button && p_button->p_down )
    {
        vlc_mutex_unlock( &osd_mutex );
        osd_MenuDown( p_this ); /* "menu select" means go to menu item below. */
        return;
    }

    if( p_button && !p_button->b_range )
    {
        p_button->p_current_state = osd_StateChange( p_button, OSD_BUTTON_PRESSED );
        osd_UpdateState( p_osd->p_state,
                p_button->i_x, p_button->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_button->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
        osd_SetMenuVisible( p_osd, true );
        osd_SetKeyPressed( VLC_OBJECT(p_osd->p_libvlc),
                           var_InheritInteger( p_osd, p_button->psz_action ) );
#if defined(OSD_MENU_DEBUG)
        msg_Dbg( p_osd, "select (%d, %s)",
                 (int)var_InheritInteger( p_osd, p_button->psz_action ),
                 p_button->psz_action );
#endif
    }
    vlc_mutex_unlock( &osd_mutex );
}

#undef osd_MenuNext
void osd_MenuNext( vlc_object_t *p_this )
{
    osd_button_t *p_button = NULL;

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return;

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range )
            p_button->p_current_state = osd_StateChange( p_button, OSD_BUTTON_UNSELECT );
        if( p_button->p_next )
            p_osd->p_state->p_visible = p_button->p_next;
        else
            p_osd->p_state->p_visible = p_osd->p_button;

        if( !p_osd->p_state->p_visible->b_range )
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction right [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_mutex_unlock( &osd_mutex );
}

#undef osd_MenuPrev
void osd_MenuPrev( vlc_object_t *p_this )
{
    osd_button_t *p_button = NULL;

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return;

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range )
            p_button->p_current_state = osd_StateChange( p_button, OSD_BUTTON_UNSELECT );
        if( p_button->p_prev )
            p_osd->p_state->p_visible = p_button->p_prev;
        else
            p_osd->p_state->p_visible = p_osd->p_last_button;

        if( !p_osd->p_state->p_visible->b_range )
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction left [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_mutex_unlock( &osd_mutex );
}

#undef osd_MenuUp
void osd_MenuUp( vlc_object_t *p_this )
{
    osd_button_t *p_button = NULL;
#if defined(OSD_MENU_DEBUG)
    vlc_value_t val;
#endif

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return;

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range )
        {
            p_button->p_current_state = osd_StateChange( p_button, OSD_BUTTON_SELECT );
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
                osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );
        }

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
        /* If this is a range style action with associated images of only one state,
            * then perform "menu select" on every menu navigation
            */
        if( p_button->b_range )
        {
            osd_SetKeyPressed( VLC_OBJECT(p_osd->p_libvlc),
                               var_InheritInteger(p_osd, p_button->psz_action) );
#if defined(OSD_MENU_DEBUG)
            msg_Dbg( p_osd, "select (%"PRId64", %s)", val.i_int, p_button->psz_action );
#endif
        }
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction up [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_mutex_unlock( &osd_mutex );
}

#undef osd_MenuDown
void osd_MenuDown( vlc_object_t *p_this )
{
    osd_button_t *p_button = NULL;
#if defined(OSD_MENU_DEBUG)
    vlc_value_t val;
#endif

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return;

    p_button = p_osd->p_state->p_visible;
    if( p_button )
    {
        if( !p_button->b_range )
        {
            p_button->p_current_state = osd_StateChange( p_button, OSD_BUTTON_SELECT );
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
                osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );
        }

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
        /* If this is a range style action with associated images of only one state,
         * then perform "menu select" on every menu navigation
         */
        if( p_button->b_range )
        {
            osd_SetKeyPressed( VLC_OBJECT(p_osd->p_libvlc),
                               var_InheritInteger(p_osd, p_button->psz_action_down) );
#if defined(OSD_MENU_DEBUG)
            msg_Dbg( p_osd, "select (%"PRId64", %s)", val.i_int, p_button->psz_action_down );
#endif
        }
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "direction down [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_mutex_unlock( &osd_mutex );
}

static int osd_VolumeStep( vlc_object_t *p_this, int i_volume, int i_steps )
{
    int i_volume_step = 0;
    (void)i_steps;

    i_volume_step = config_GetInt( p_this->p_libvlc, "volume-step" );
    return (i_volume/i_volume_step);
}

#undef osd_ButtonFind
osd_button_t *osd_ButtonFind( vlc_object_t *p_this, int i_x, int i_y,
    int i_window_height, int i_window_width,
    int i_scale_width, int i_scale_height )
{
    osd_button_t *p_button;

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return NULL;

    p_button = p_osd->p_button;
    for( ; p_button != NULL; p_button = p_button->p_next )
    {
        int i_source_video_width  = ( i_window_width  * 1000 ) / i_scale_width;
        int i_source_video_height = ( i_window_height * 1000 ) / i_scale_height;
        int i_y_offset = p_button->i_y;
        int i_x_offset = p_button->i_x;
        int i_width = p_button->i_width;
        int i_height = p_button->i_height;

        if( p_osd->i_position > 0 )
        {
            int i_inv_scale_y = i_source_video_height;
            int i_inv_scale_x = i_source_video_width;
            int pi_x = 0;

            if( p_osd->i_position & SUBPICTURE_ALIGN_BOTTOM )
            {
                i_y_offset = i_window_height - p_button->i_height -
                    (p_osd->i_y + p_button->i_y) * i_inv_scale_y / 1000;
            }
            else if ( !(p_osd->i_position & SUBPICTURE_ALIGN_TOP) )
            {
                i_y_offset = i_window_height / 2 - p_button->i_height / 2;
            }

            if( p_osd->i_position & SUBPICTURE_ALIGN_RIGHT )
            {
                i_x_offset = i_window_width - p_button->i_width -
                    (pi_x + p_button->i_x)
                    * i_inv_scale_x / 1000;
            }
            else if ( !(p_osd->i_position & SUBPICTURE_ALIGN_LEFT) )
            {
                i_x_offset = i_window_width / 2 - p_button->i_width / 2;
            }

            i_width = i_window_width - p_button->i_width - i_inv_scale_x / 1000;
            i_height = i_window_height - p_button->i_height - i_inv_scale_y / 1000;
        }

        // TODO: write for Up / Down case too.
        // TODO: handle absolute positioning case
        if( ( i_x >= i_x_offset ) && ( i_x <= i_x_offset + i_width ) &&
            ( i_y >= i_y_offset ) && ( i_y <= i_y_offset + i_height ) )
        {
            vlc_mutex_unlock( &osd_mutex );
            return p_button;
        }
    }

    vlc_mutex_unlock( &osd_mutex );
    return NULL;
}

#undef osd_ButtonSelect
/**
 * Select the button provided as the new active button
 */
void osd_ButtonSelect( vlc_object_t *p_this, osd_button_t *p_button )
{
    osd_button_t *p_old;

    osd_menu_t *p_osd = osd_FindVisible( p_this );
    if( p_osd == NULL )
        return;

    p_old = p_osd->p_state->p_visible;
    if( p_old )
    {
        if( !p_old->b_range )
            p_old->p_current_state = osd_StateChange( p_old, OSD_BUTTON_UNSELECT );
        p_osd->p_state->p_visible = p_button;

        if( !p_osd->p_state->p_visible->b_range )
            p_osd->p_state->p_visible->p_current_state =
                osd_StateChange( p_osd->p_state->p_visible, OSD_BUTTON_SELECT );

        osd_UpdateState( p_osd->p_state,
                p_osd->p_state->p_visible->i_x, p_osd->p_state->p_visible->i_y,
                p_osd->p_state->p_visible->p_current_state->i_width,
                p_osd->p_state->p_visible->p_current_state->i_height,
                p_osd->p_state->p_visible->p_current_state->p_pic );
        osd_SetMenuUpdate( p_osd, true );
    }
#if defined(OSD_MENU_DEBUG)
    msg_Dbg( p_osd, "button selected is [button %s]", p_osd->p_state->p_visible->psz_action );
#endif

    vlc_mutex_unlock( &osd_mutex );
}
