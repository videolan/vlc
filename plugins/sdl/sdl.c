/*****************************************************************************
 * sdl.c : SDL plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
 *      . Initial plugin code by Samuel Hocevar <sam@via.ecp.fr>
 *      . Modified to use the SDL by Pierre Baillet <octplane@via.ecp.fr>
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
#include "tests.h"
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

#if 0
static void yuv_GetPlugin( p_vout_thread_t p_vout );
#endif

/* Video output */
int     vout_SDLCreate       ( vout_thread_t *p_vout, char *psz_display,
                               int i_root_window, void *p_data );
int     vout_SDLInit         ( p_vout_thread_t p_vout );
void    vout_SDLEnd          ( p_vout_thread_t p_vout );
void    vout_SDLDestroy      ( p_vout_thread_t p_vout );
int     vout_SDLManage       ( p_vout_thread_t p_vout );
void    vout_SDLDisplay      ( p_vout_thread_t p_vout );
void    vout_SDLSetPalette   ( p_vout_thread_t p_vout,
                               u16 *red, u16 *green, u16 *blue, u16 *transp );

#if 0
/* YUV transformations */
int     yuv_CInit          ( p_vout_thread_t p_vout );
int     yuv_CReset         ( p_vout_thread_t p_vout );
void    yuv_CEnd           ( p_vout_thread_t p_vout );
#endif

/* Interface */
int     intf_SDLCreate       ( p_intf_thread_t p_intf );
void    intf_SDLDestroy      ( p_intf_thread_t p_intf );
void    intf_SDLManage       ( p_intf_thread_t p_intf );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "SDL (video)";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = NULL;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    
    
    /* TODO: before doing this, we have to know if the videoCard is capable of 
     * hardware YUV -> display acceleration....
     */

#if 0
    p_info->yuv_GetPlugin  = (void *) yuv_GetPlugin;
#else
    p_info->yuv_GetPlugin = NULL;
#endif
	    
    
    /* if the SDL libraries are there, assume we can enter the
     * initialization part at least, even if we fail afterwards */
    
    p_info->i_score = 0x100;
    
    
    
    /* If this plugin was requested, score it higher */
    if( TestMethod( VOUT_METHOD_VAR, "sdl" ) )
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
    p_vout->p_sys_create  = vout_SDLCreate;
    p_vout->p_sys_init    = vout_SDLInit;
    p_vout->p_sys_end     = vout_SDLEnd;
    p_vout->p_sys_destroy = vout_SDLDestroy;
    p_vout->p_sys_manage  = vout_SDLManage;
    p_vout->p_sys_display = vout_SDLDisplay;
}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_SDLCreate;
    p_intf->p_sys_destroy = intf_SDLDestroy;
    p_intf->p_sys_manage  = intf_SDLManage;
}

#if 0
static void yuv_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_yuv_init   = yuv_CInit;
    p_vout->p_yuv_reset  = yuv_CReset;
    p_vout->p_yuv_end    = yuv_CEnd;
}
#endif

