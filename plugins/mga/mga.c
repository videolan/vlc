/*****************************************************************************
 * mga.c : Matrox Graphic Array plugin for vlc
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

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "interface.h"
#include "audio_output.h"
#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void vout_GetPlugin( p_vout_thread_t p_vout );
static void intf_GetPlugin( p_intf_thread_t p_intf );

/* Video output */
int     vout_MGACreate       ( vout_thread_t *p_vout, char *psz_display,
                               int i_root_window, void *p_data );
int     vout_MGAInit         ( p_vout_thread_t p_vout );
void    vout_MGAEnd          ( p_vout_thread_t p_vout );
void    vout_MGADestroy      ( p_vout_thread_t p_vout );
int     vout_MGAManage       ( p_vout_thread_t p_vout );
void    vout_MGADisplay      ( p_vout_thread_t p_vout );
void    vout_SetPalette      ( p_vout_thread_t p_vout,
                               u16 *red, u16 *green, u16 *blue, u16 *transp );

/* Interface */
int     intf_MGACreate       ( p_intf_thread_t p_intf );
void    intf_MGADestroy      ( p_intf_thread_t p_intf );
void    intf_MGAManage       ( p_intf_thread_t p_intf );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "Matrox Acceleration";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = NULL;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    p_info->yuv_GetPlugin  = NULL;

    /* The MGA module does not work yet */
    p_info->i_score = 0x0;

    return( p_info );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

static void vout_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_sys_create  = vout_MGACreate;
    p_vout->p_sys_init    = vout_MGAInit;
    p_vout->p_sys_end     = vout_MGAEnd;
    p_vout->p_sys_destroy = vout_MGADestroy;
    p_vout->p_sys_manage  = vout_MGAManage;
    p_vout->p_sys_display = vout_MGADisplay;
}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_MGACreate;
    p_intf->p_sys_destroy = intf_MGADestroy;
    p_intf->p_sys_manage  = intf_MGAManage;
}

