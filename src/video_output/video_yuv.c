/*****************************************************************************
 * video_yuv.c: YUV transformation functions
 * These functions set up YUV tables for colorspace conversion
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <math.h>                                            /* exp(), pow() */
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "modules.h"

#include "video.h"
#include "video_output.h"
#include "video_yuv.h"

#include "intf_msg.h"

#include "main.h"

/*****************************************************************************
 * vout_InitYUV: allocate and initialize translation tables
 *****************************************************************************
 * This function will allocate memory to store translation tables, depending
 * on the screen depth.
 *****************************************************************************/
int vout_InitYUV( vout_thread_t *p_vout )
{
    /* Choose the best module */
    p_vout->yuv.p_module = module_Need( p_main->p_bank,
                                        MODULE_CAPABILITY_YUV, NULL );

    if( p_vout->yuv.p_module == NULL )
    {
        intf_ErrMsg( "vout error: no suitable yuv module" );
        return( -1 );
    }

#define yuv_functions p_vout->yuv.p_module->p_functions->yuv.functions.yuv
    p_vout->yuv.pf_init       = yuv_functions.pf_init;
    p_vout->yuv.pf_reset      = yuv_functions.pf_reset;
    p_vout->yuv.pf_end        = yuv_functions.pf_end;
#undef yuv_functions

    return( p_vout->yuv.pf_init( p_vout ) );
}

/*****************************************************************************
 * vout_ResetYUV: re-initialize translation tables
 *****************************************************************************
 * This function will initialize the tables allocated by vout_InitYUV and
 * set functions pointers.
 *****************************************************************************/
int vout_ResetYUV( vout_thread_t *p_vout )
{
    p_vout->yuv.pf_end( p_vout );
    return( p_vout->yuv.pf_init( p_vout ) );
}

/*****************************************************************************
 * vout_EndYUV: destroy translation tables
 *****************************************************************************
 * Free memory allocated by vout_InitYUV
 *****************************************************************************/
void vout_EndYUV( vout_thread_t *p_vout )
{
    p_vout->yuv.pf_end( p_vout );
    module_Unneed( p_main->p_bank, p_vout->yuv.p_module );
}

