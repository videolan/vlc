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
static void aout_GetPlugin( p_aout_thread_t p_aout );

/* Audio output */
int     aout_DspOpen         ( aout_thread_t *p_aout );
int     aout_DspReset        ( aout_thread_t *p_aout );
int     aout_DspSetFormat    ( aout_thread_t *p_aout );
int     aout_DspSetChannels  ( aout_thread_t *p_aout );
int     aout_DspSetRate      ( aout_thread_t *p_aout );
long    aout_DspGetBufInfo   ( aout_thread_t *p_aout, long l_buffer_info );
void    aout_DspPlaySamples  ( aout_thread_t *p_aout, byte_t *buffer,
                               int i_size );
void    aout_DspClose        ( aout_thread_t *p_aout );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    int i_fd;
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "OSS /dev/dsp";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = aout_GetPlugin;
    p_info->vout_GetPlugin = NULL;
    p_info->intf_GetPlugin = NULL;
    p_info->yuv_GetPlugin  = NULL;

    /* Test if the device can be opened */
    if ( (i_fd = open( main_GetPszVariable( AOUT_DSP_VAR, AOUT_DSP_DEFAULT ),
                       O_WRONLY )) < 0 )
    {
        p_info->i_score = 0;
    }
    else
    {
        close( i_fd );
        p_info->i_score = 0x100;
    }

    /* If this plugin was requested, score it higher */
    if( TestMethod( AOUT_METHOD_VAR, "dsp" ) )
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
    p_aout->p_sys_open        = aout_DspOpen;
    p_aout->p_sys_reset       = aout_DspReset;
    p_aout->p_sys_setformat   = aout_DspSetFormat;
    p_aout->p_sys_setchannels = aout_DspSetChannels;
    p_aout->p_sys_setrate     = aout_DspSetRate;
    p_aout->p_sys_getbufinfo  = aout_DspGetBufInfo;
    p_aout->p_sys_playsamples = aout_DspPlaySamples;
    p_aout->p_sys_close       = aout_DspClose;
}

