/*****************************************************************************
 * dsp.c : OSS /dev/dsp plugin for vlc
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

#include "plugins_export.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
void aout_GetPlugin( p_aout_thread_t p_aout );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "OSS /dev/dsp";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = aout_GetPlugin;
    p_info->vout_GetPlugin = NULL;
    p_info->intf_GetPlugin = NULL;
    p_info->yuv_GetPlugin  = NULL;

    return( p_info );
}

/*****************************************************************************
 * Test: tests if the plugin can be launched
 *****************************************************************************/
int Test( void )
{
    /* TODO: check if suitable */
    return( 1 );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

void aout_GetPlugin( p_aout_thread_t p_aout )
{
    p_aout->p_sys_open        = aout_SysOpen;
    p_aout->p_sys_reset       = aout_SysReset;
    p_aout->p_sys_setformat   = aout_SysSetFormat;
    p_aout->p_sys_setchannels = aout_SysSetChannels;
    p_aout->p_sys_setrate     = aout_SysSetRate;
    p_aout->p_sys_getbufinfo  = aout_SysGetBufInfo;
    p_aout->p_sys_playsamples = aout_SysPlaySamples;
    p_aout->p_sys_close       = aout_SysClose;
}

