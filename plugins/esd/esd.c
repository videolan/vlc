/*****************************************************************************
 * esd.c : Esound plugin for vlc
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

/* Audio output */
int     aout_EsdOpen         ( aout_thread_t *p_aout );
int     aout_EsdReset        ( aout_thread_t *p_aout );
int     aout_EsdSetFormat    ( aout_thread_t *p_aout );
int     aout_EsdSetChannels  ( aout_thread_t *p_aout );
int     aout_EsdSetRate      ( aout_thread_t *p_aout );
long    aout_EsdGetBufInfo   ( aout_thread_t *p_aout, long l_buffer_info );
void    aout_EsdPlaySamples  ( aout_thread_t *p_aout, byte_t *buffer,
                               int i_size );
void    aout_EsdClose        ( aout_thread_t *p_aout );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "Esound";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = aout_GetPlugin;
    p_info->vout_GetPlugin = NULL;
    p_info->intf_GetPlugin = NULL;
    p_info->yuv_GetPlugin  = NULL;

    /* esound should always work, but score it lower than DSP */
    p_info->i_score = 0x100;

    /* If this plugin was requested, score it higher */
    if( TestMethod( AOUT_METHOD_VAR, "esd" ) )
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
    p_aout->p_sys_open        = aout_EsdOpen;
    p_aout->p_sys_reset       = aout_EsdReset;
    p_aout->p_sys_setformat   = aout_EsdSetFormat;
    p_aout->p_sys_setchannels = aout_EsdSetChannels;
    p_aout->p_sys_setrate     = aout_EsdSetRate;
    p_aout->p_sys_getbufinfo  = aout_EsdGetBufInfo;
    p_aout->p_sys_playsamples = aout_EsdPlaySamples;
    p_aout->p_sys_close       = aout_EsdClose;
}

