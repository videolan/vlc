/*****************************************************************************
 * fb.c : Linux framebuffer plugin for vlc
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <unistd.h>                                               /* close() */

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
int     vout_FBCreate       ( vout_thread_t *p_vout, char *psz_display,
                              int i_root_window, void *p_data );
int     vout_FBInit         ( p_vout_thread_t p_vout );
void    vout_FBEnd          ( p_vout_thread_t p_vout );
void    vout_FBDestroy      ( p_vout_thread_t p_vout );
int     vout_FBManage       ( p_vout_thread_t p_vout );
void    vout_FBDisplay      ( p_vout_thread_t p_vout );
void    vout_FBSetPalette   ( p_vout_thread_t p_vout,
                              u16 *red, u16 *green, u16 *blue, u16 *transp );

/* Interface */
int     intf_FBCreate       ( p_intf_thread_t p_intf );
void    intf_FBDestroy      ( p_intf_thread_t p_intf );
void    intf_FBManage       ( p_intf_thread_t p_intf );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    int i_fd;
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "Linux framebuffer";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = NULL;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    p_info->yuv_GetPlugin  = NULL;

    /* Test if the device can be opened */
    if ( (i_fd = open( main_GetPszVariable( VOUT_FB_DEV_VAR,
                                            VOUT_FB_DEV_DEFAULT ),
                       O_RDWR )) < 0 )
    {
        p_info->i_score = 0;
    }
    else
    {
        close( i_fd );
        p_info->i_score = 0x100;
    }

    if( TestProgram( "fbvlc" ) )
    {
        p_info->i_score += 0x180;
    }

    /* If this plugin was requested, score it higher */
    if( TestMethod( VOUT_METHOD_VAR, "fb" ) )
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
    p_vout->p_sys_create  = vout_FBCreate;
    p_vout->p_sys_init    = vout_FBInit;
    p_vout->p_sys_end     = vout_FBEnd;
    p_vout->p_sys_destroy = vout_FBDestroy;
    p_vout->p_sys_manage  = vout_FBManage;
    p_vout->p_sys_display = vout_FBDisplay;
    
    /* optional functions */
    p_vout->p_set_palette = vout_FBSetPalette;
}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_FBCreate;
    p_intf->p_sys_destroy = intf_FBDestroy;
    p_intf->p_sys_manage  = intf_FBManage;
}

