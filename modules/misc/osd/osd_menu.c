/*****************************************************************************
 * parser.c : OSD import module
 *****************************************************************************
 * Copyright (C) 2007-2008 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_config.h>

#include <vlc_keys.h>
#include <vlc_image.h>
#include <vlc_osd.h>
#include <vlc_charset.h>

#include "osd_menu.h"

#undef OSD_MENU_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * Create a new Menu structure
 *****************************************************************************/
osd_menu_t *osd_MenuNew( osd_menu_t *p_menu, const char *psz_path,
                         int i_x, int i_y )
{
    if( !p_menu ) return NULL;

    p_menu->p_state = (osd_menu_state_t *) malloc( sizeof( osd_menu_state_t ) );
    if( !p_menu->p_state )
    {
        msg_Err( p_menu, "Memory allocation for OSD Menu state failed" );
        return NULL;
    }

    memset(p_menu->p_state, 0, sizeof(osd_menu_state_t));
    if( psz_path != NULL )
        p_menu->psz_path = strdup( psz_path );
    else
        p_menu->psz_path = NULL;

    p_menu->i_x = i_x;
    p_menu->i_y = i_y;
    p_menu->i_style = OSD_MENU_STYLE_SIMPLE;

    return p_menu;
}

/*****************************************************************************
 * Free the menu
 *****************************************************************************/
void osd_MenuFree( osd_menu_t *p_menu )
{
    msg_Dbg( p_menu, "freeing menu" );
    osd_ButtonFree( p_menu, p_menu->p_button );

    free( p_menu->psz_path );
    free( p_menu->p_state );

    p_menu->p_button = NULL;
    p_menu->p_last_button = NULL;
    p_menu->psz_path = NULL;
    p_menu->p_state = NULL;
}

/*****************************************************************************
 * Create a new button
 *****************************************************************************/
osd_button_t *osd_ButtonNew( const char *psz_action, int i_x, int i_y )
{
    osd_button_t *p_button = NULL;
    p_button = (osd_button_t*) malloc( sizeof(osd_button_t) );
    if( !p_button )
        return NULL;

    memset( p_button, 0, sizeof(osd_button_t) );
    p_button->psz_action = strdup(psz_action);
    p_button->psz_action_down = NULL;
    p_button->p_feedback = NULL;
    p_button->i_x = i_x;
    p_button->i_y = i_y;

    return p_button;
}

/*****************************************************************************
 * Free a button
 *****************************************************************************/
void osd_ButtonFree( osd_menu_t *p_menu, osd_button_t *p_button )
{
    osd_button_t *p_current = p_button;
    osd_button_t *p_next = NULL;
    osd_button_t *p_prev = NULL;

    if( !p_current ) return;

    /* First walk to the end. */
    while( p_current->p_next )
    {
        p_next = p_current->p_next;
        p_current = p_next;
    }
    /* Then free end first and walk to the start. */
    while( p_current->p_prev )
    {
        msg_Dbg( p_menu, "+ freeing button %s [%p]",
                 p_current->psz_action, p_current );
        p_prev = p_current->p_prev;
        p_current = p_prev;
        if( p_current->p_next )
        {
            free( p_current->p_next->psz_name );
            free( p_current->p_next->psz_action );
            free( p_current->p_next->psz_action_down );
            if( p_current->p_feedback )
            {
                free( p_current->p_feedback->p_data_orig );
                free( p_current->p_feedback );
                p_current->p_feedback = NULL;
            }

            /* Free all states first */
            if( p_current->p_next->p_states )
                osd_StatesFree( p_menu, p_current->p_next->p_states );

            free( p_current->p_next );
            p_current->p_next = NULL;
        }

        if( p_current->p_up )
        {
            free( p_current->p_up->psz_name );
            free( p_current->p_up->psz_action );
            free( p_current->p_up->psz_action_down );
            if( p_current->p_feedback )
            {
                free( p_current->p_feedback->p_data_orig );
                free( p_current->p_feedback );
            }

            p_current->p_feedback = NULL;

            /* Free all states first */
            if( p_current->p_up->p_states )
                osd_StatesFree( p_menu, p_current->p_up->p_states );
            free( p_current->p_up );
            p_current->p_up = NULL;
        }
    }
    /* Free the last one. */
    if( p_button )
    {
        msg_Dbg( p_menu, "+ freeing button %s [%p]",
                 p_button->psz_action, p_button );
        free( p_button->psz_name );
        free( p_button->psz_action );
        free( p_button->psz_action_down );
        if( p_current->p_feedback )
        {
            free( p_current->p_feedback->p_data_orig );
            free( p_current->p_feedback );
            p_current->p_feedback = NULL;
        }

        if( p_button->p_states )
            osd_StatesFree( p_menu, p_button->p_states );

        free( p_button );
        p_button = NULL;
    }
}

/*****************************************************************************
 * Create a new state image
 *****************************************************************************/
osd_state_t *osd_StateNew( osd_menu_t *p_menu, const char *psz_file,
                           const char *psz_state )
{
    osd_state_t *p_state = NULL;
    video_format_t fmt_in, fmt_out;

    p_state = (osd_state_t*) malloc( sizeof(osd_state_t) );
    if( !p_state )
        return NULL;

    memset( p_state, 0, sizeof(osd_state_t) );
    memset( &fmt_in, 0, sizeof(video_format_t) );
    memset( &fmt_out, 0, sizeof(video_format_t) );

    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    if( p_menu->p_image )
    {
        p_state->p_pic = image_ReadUrl( p_menu->p_image, psz_file,
                                        &fmt_in, &fmt_out );
        if( p_state->p_pic )
        {
            p_state->i_width  = p_state->p_pic->p[Y_PLANE].i_visible_pitch;
            p_state->i_height = p_state->p_pic->p[Y_PLANE].i_visible_lines;
        }
    }

    if( psz_state )
    {
        p_state->psz_state = strdup( psz_state );
        if( strncmp( ppsz_button_states[0], psz_state,
                     strlen(ppsz_button_states[0]) ) == 0 )
            p_state->i_state = OSD_BUTTON_UNSELECT;
        else if( strncmp( ppsz_button_states[1], psz_state,
                          strlen(ppsz_button_states[1]) ) == 0 )
            p_state->i_state = OSD_BUTTON_SELECT;
        else if( strncmp( ppsz_button_states[2], psz_state,
                          strlen(ppsz_button_states[2]) ) == 0 )
            p_state->i_state = OSD_BUTTON_PRESSED;
    }
    return p_state;
}

/*****************************************************************************
 * Free state images
 *****************************************************************************/
void osd_StatesFree( osd_menu_t *p_menu, osd_state_t *p_states )
{
    osd_state_t *p_state = p_states;
    osd_state_t *p_next = NULL;
    osd_state_t *p_prev = NULL;

    if( !p_state ) return;

    while( p_state->p_next )
    {
        p_next = p_state->p_next;
        p_state = p_next;
    }
    /* Then free end first and walk to the start. */
    while( p_state->p_prev )
    {
        msg_Dbg( p_menu, " |- freeing state %s [%p]",
                 p_state->psz_state, p_state );
        p_prev = p_state->p_prev;
        p_state = p_prev;
        if( p_state->p_next )
        {
            if( p_state->p_next->p_pic )
            {
                free( p_state->p_next->p_pic->p_data_orig );
                free( p_state->p_next->p_pic );
            }
            free( p_state->p_next->psz_state );
            free( p_state->p_next );
            p_state->p_next = NULL;
        }
    }
    /* Free the last one. */
    if( p_states )
    {
        msg_Dbg( p_menu, " |- freeing state %s [%p]",
                 p_state->psz_state, p_states );
        if( p_states->p_pic )
        {
            free( p_states->p_pic->p_data_orig );
            free( p_states->p_pic );
        }
        free( p_state->psz_state );
        free( p_states );
        p_states = NULL;
    }
}
