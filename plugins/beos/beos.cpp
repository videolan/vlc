/*****************************************************************************
 * beos.cpp : BeOS plugin for vlc
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

extern "C"
{
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
static void aout_GetPlugin( p_aout_thread_t p_aout );
static void vout_GetPlugin( p_vout_thread_t p_vout );
static void intf_GetPlugin( p_intf_thread_t p_intf );

/* Audio output */
int     aout_BeOpen         ( aout_thread_t *p_aout );
int     aout_BeReset        ( aout_thread_t *p_aout );
int     aout_BeSetFormat    ( aout_thread_t *p_aout );
int     aout_BeSetChannels  ( aout_thread_t *p_aout );
int     aout_BeSetRate      ( aout_thread_t *p_aout );
long    aout_BeGetBufInfo   ( aout_thread_t *p_aout, long l_buffer_info );
void    aout_BePlay         ( aout_thread_t *p_aout, byte_t *buffer,
                              int i_size );
void    aout_BeClose        ( aout_thread_t *p_aout );

/* Video output */
int     vout_BeCreate       ( vout_thread_t *p_vout, char *psz_display,
                              int i_root_window, void *p_data );
int     vout_BeInit         ( p_vout_thread_t p_vout );
void    vout_BeEnd          ( p_vout_thread_t p_vout );
void    vout_BeDestroy      ( p_vout_thread_t p_vout );
int     vout_BeManage       ( p_vout_thread_t p_vout );
void    vout_BeDisplay      ( p_vout_thread_t p_vout );
void    vout_BeSetPalette   ( p_vout_thread_t p_vout,
                              u16 *red, u16 *green, u16 *blue, u16 *transp );

/* Interface */
int     intf_BeCreate       ( p_intf_thread_t p_intf );
void    intf_BeDestroy      ( p_intf_thread_t p_intf );
void    intf_BeManage       ( p_intf_thread_t p_intf );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "BeOS";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = aout_GetPlugin;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    p_info->yuv_GetPlugin = NULL;

    /* the beos plugin always works under BeOS :) */
    p_info->i_score = 0x800;

    return( p_info );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

static void aout_GetPlugin( p_aout_thread_t p_aout )
{
    p_aout->p_open        = aout_BeOpen;
    p_aout->p_setformat   = aout_BeSetFormat;
    p_aout->p_getbufinfo  = aout_BeGetBufInfo;
    p_aout->p_play        = aout_BePlay;
    p_aout->p_close       = aout_BeClose;
}

static void vout_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_sys_create  = vout_BeCreate;
    p_vout->p_sys_init    = vout_BeInit;
    p_vout->p_sys_end     = vout_BeEnd;
    p_vout->p_sys_destroy = vout_BeDestroy;
    p_vout->p_sys_manage  = vout_BeManage;
    p_vout->p_sys_display = vout_BeDisplay;
}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_BeCreate;
    p_intf->p_sys_destroy = intf_BeDestroy;
    p_intf->p_sys_manage  = intf_BeManage;
}

} /* extern "C" */
