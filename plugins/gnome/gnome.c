/*****************************************************************************
 * gnome.c : Gnome plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */

#include <X11/Xlib.h>

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "plugins.h"

#include "interface.h"
#include "audio_output.h"
#include "video.h"
#include "video_output.h"

#include "main.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void vout_GetPlugin( p_vout_thread_t p_vout );
static void intf_GetPlugin( p_intf_thread_t p_intf );

/* Video output */
int     vout_GnomeCreate       ( vout_thread_t *p_vout, char *psz_display,
                                 int i_root_window, void *p_data );
int     vout_GnomeInit         ( p_vout_thread_t p_vout );
void    vout_GnomeEnd          ( p_vout_thread_t p_vout );
void    vout_GnomeDestroy      ( p_vout_thread_t p_vout );
int     vout_GnomeManage       ( p_vout_thread_t p_vout );
void    vout_GnomeDisplay      ( p_vout_thread_t p_vout );
void    vout_GnomeSetPalette   ( p_vout_thread_t p_vout, u16 *red,
                                 u16 *green, u16 *blue, u16 *transp );

/* Interface */
int     intf_GnomeCreate       ( p_intf_thread_t p_intf );
void    intf_GnomeDestroy      ( p_intf_thread_t p_intf );
void    intf_GnomeManage       ( p_intf_thread_t p_intf );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    Display *p_display;
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "Gnome";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = NULL;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    p_info->yuv_GetPlugin  = NULL;

    /* Check that we can open the X display */
    if( (p_display = XOpenDisplay( XDisplayName(
                         main_GetPszVariable( VOUT_DISPLAY_VAR, NULL ) ) ))
        == NULL )
    {
        p_info->i_score = 0;
    }
    else
    {
        XCloseDisplay( p_display );
        p_info->i_score = 0x200;
    }

    if( TestProgram( "gvlc" ) )
    {
        p_info->i_score += 0x180;
    }

    /* If this plugin was requested, score it higher */
    if( TestMethod( VOUT_METHOD_VAR, "gnome" ) )
    {
        p_info->i_score += 0x200;
    }

    return( p_info );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

static void vout_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_sys_create  = vout_GnomeCreate;
    p_vout->p_sys_init    = vout_GnomeInit;
    p_vout->p_sys_end     = vout_GnomeEnd;
    p_vout->p_sys_destroy = vout_GnomeDestroy;
    p_vout->p_sys_manage  = vout_GnomeManage;
    p_vout->p_sys_display = vout_GnomeDisplay;

    /* optional functions */
    p_vout->p_set_palette = vout_GnomeSetPalette;
}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_GnomeCreate;
    p_intf->p_sys_destroy = intf_GnomeDestroy;
    p_intf->p_sys_manage  = intf_GnomeManage;
}

