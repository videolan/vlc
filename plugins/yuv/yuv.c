/*****************************************************************************
 * yuv.c : C YUV functions for vlc
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
static void yuv_GetPlugin( p_vout_thread_t p_vout );

/* YUV transformations */
int     yuv_CInit          ( p_vout_thread_t p_vout );
int     yuv_CReset         ( p_vout_thread_t p_vout );
void    yuv_CEnd           ( p_vout_thread_t p_vout );

/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "C YUV to RGB transformations";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = NULL;
    p_info->vout_GetPlugin = NULL;
    p_info->intf_GetPlugin = NULL;
    p_info->yuv_GetPlugin  = yuv_GetPlugin;

    /* The C YUV functions should always work */
    p_info->i_score = 0x100;

    /* If this plugin was requested, score it higher */
    if( TestMethod( YUV_METHOD_VAR, "nommx" ) )
    {
        p_info->i_score += 0x200;
    }

    return( p_info );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

static void yuv_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_yuv_init   = yuv_CInit;
    p_vout->p_yuv_reset  = yuv_CReset;
    p_vout->p_yuv_end    = yuv_CEnd;
}

