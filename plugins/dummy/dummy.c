/*****************************************************************************
 * dummy.c : dummy plugin for vlc
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
#include "tests.h"
#include "plugins.h"

#include "interface.h"
#include "audio_output.h"
#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void aout_GetPlugin( p_aout_thread_t p_aout );
static void vout_GetPlugin( p_vout_thread_t p_vout );
static void intf_GetPlugin( p_intf_thread_t p_intf );

/* Audio output */
int     aout_DummyOpen         ( aout_thread_t *p_aout );
int     aout_DummySetFormat    ( aout_thread_t *p_aout );
long    aout_DummyGetBufInfo   ( aout_thread_t *p_aout, long l_buffer_info );
void    aout_DummyPlay         ( aout_thread_t *p_aout, byte_t *buffer,
                                 int i_size );
void    aout_DummyClose        ( aout_thread_t *p_aout );

/* Video output */
int     vout_DummyCreate       ( vout_thread_t *p_vout, char *psz_display,
                                 int i_root_window, void *p_data );
int     vout_DummyInit         ( p_vout_thread_t p_vout );
void    vout_DummyEnd          ( p_vout_thread_t p_vout );
void    vout_DummyDestroy      ( p_vout_thread_t p_vout );
int     vout_DummyManage       ( p_vout_thread_t p_vout );
void    vout_DummyDisplay      ( p_vout_thread_t p_vout );
void    vout_DummySetPalette   ( p_vout_thread_t p_vout,
                                 u16 *red, u16 *green, u16 *blue, u16 *transp );

/* Interface */
int     intf_DummyCreate       ( p_intf_thread_t p_intf );
void    intf_DummyDestroy      ( p_intf_thread_t p_intf );
void    intf_DummyManage       ( p_intf_thread_t p_intf );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "Dummy";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = aout_GetPlugin;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    p_info->yuv_GetPlugin  = NULL;

    /* The dummy plugin always works, but should have low priority */
    p_info->i_score = 0x1;

    /* If this plugin was requested, score it higher */
    if( TestMethod( VOUT_METHOD_VAR, "dummy" ) )
    {
        p_info->i_score += 0x200;
    }

    /* If this plugin was requested, score it higher */
    if( TestMethod( AOUT_METHOD_VAR, "dummy" ) )
    {
        p_info->i_score += 0x200;
    }


    return( p_info );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

static void aout_GetPlugin( p_aout_thread_t p_aout )
{
    p_aout->p_open        = aout_DummyOpen;
    p_aout->p_setformat   = aout_DummySetFormat;
    p_aout->p_getbufinfo  = aout_DummyGetBufInfo;
    p_aout->p_play        = aout_DummyPlay;
    p_aout->p_close       = aout_DummyClose;
}

static void vout_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_sys_create  = vout_DummyCreate;
    p_vout->p_sys_init    = vout_DummyInit;
    p_vout->p_sys_end     = vout_DummyEnd;
    p_vout->p_sys_destroy = vout_DummyDestroy;
    p_vout->p_sys_manage  = vout_DummyManage;
    p_vout->p_sys_display = vout_DummyDisplay;
}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_DummyCreate;
    p_intf->p_sys_destroy = intf_DummyDestroy;
    p_intf->p_sys_manage  = intf_DummyManage;
}

