/*****************************************************************************
 * video_yuv.c: YUV transformation functions
 * Provides functions to perform the YUV conversion. The functions provided here
 * are a complete and portable C implementation, and may be replaced in certain
 * case by optimized functions.
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
#include "plugins.h"
#include "video.h"
#include "video_output.h"
#include "video_yuv.h"

#include "intf_msg.h"

/*****************************************************************************
 * vout_InitYUV: allocate and initialize translations tables
 *****************************************************************************
 * This function will allocate memory to store translation tables, depending
 * of the screen depth.
 *****************************************************************************/
int yuv_CInit( vout_thread_t *p_vout )
{

    /* Initialize tables */
    SetSDLYUV( p_vout );
    return( 0 );
}

/*****************************************************************************
 * yuv_CEnd: destroy translations tables
 *****************************************************************************
 * Free memory allocated by yuv_CCreate.
 *****************************************************************************/
void yuv_CEnd( vout_thread_t *p_vout )
{
    free( p_vout->yuv.p_base );
    free( p_vout->yuv.p_buffer );
    free( p_vout->yuv.p_offset );
}

/*****************************************************************************
 * yuv_CReset: re-initialize translations tables
 *****************************************************************************
 * This function will initialize the tables allocated by vout_CreateTables and
 * set functions pointers.
 *****************************************************************************/
int yuv_CReset( vout_thread_t *p_vout )
{
    yuv_CEnd( p_vout );
    return( yuv_CInit( p_vout ) );
}

/* following functions are local */

/*****************************************************************************
 * SetYUV: compute tables and set function pointers
+ *****************************************************************************/
void SetSDLYUV( vout_thread_t *p_vout )
{
    /*
     * Set functions pointers
     */
    if( p_vout->b_grayscale )
    {
        /* Grayscale */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) Convert8;
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) Convert8;
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) Convert8;
            break;
        case 2:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) Convert16;
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) Convert16;
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) Convert16;
            break;
        case 3:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) Convert24;
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) Convert24;
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) Convert24;
            break;
        case 4:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) Convert32;
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) Convert32;
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) Convert32;
            break;
        }
    }
    else
    {
        /* Color */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertRGB8;
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) ConvertRGB8;
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) ConvertRGB8;
            break;
        case 2:
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertRGB16;
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertRGB16;
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertRGB16;
            break;
        case 3:
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertRGB24;
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertRGB24;
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertRGB24;
            break;
        case 4:
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertRGB32;
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertRGB32;
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertRGB32;
            break;
        }
    }
}
