/*****************************************************************************
 * video_yuv32.c: YUV transformation functions for 32 bpp
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

#include "SDL/SDL.h"


typedef struct vout_sys_s
{
        SDL_Surface *   p_display;                             /* display device */
        Uint8   *   p_buffer[2];
                                                                 /* Buffers informations */
        boolean_t   b_must_acquire;           /* must be acquired before writing */
}   vout_sys_t;



void Convert8( YUV_ARGS_8BPP )
{
}

void Convert16( YUV_ARGS_16BPP )
{
}

void Convert24( YUV_ARGS_24BPP )
{
}

void Convert32( YUV_ARGS_32BPP )
{
}

void ConvertRGB8( YUV_ARGS_8BPP )
{
}

void ConvertRGB16( YUV_ARGS_16BPP )
{
}

void ConvertRGB24( YUV_ARGS_24BPP )
{
}

void ConvertRGB32( YUV_ARGS_32BPP )
{
    /* for now, the only function filled because I use 32bpp display :P */
    SDL_Overlay * screen;
    SDL_Rect    disp;
    screen=SDL_CreateYUVOverlay( i_width, i_height,SDL_IYUV_OVERLAY,  p_vout->p_sys->p_display );
    SDL_LockYUVOverlay(screen);
    
    memcpy(screen->pixels, p_vout->yuv.p_buffer, screen->h * screen->pitch );
    
    SDL_UnlockYUVOverlay(screen);
    disp.x=0;
    disp.y=0;
    disp.w= i_width;
    disp.h= i_height;
    
    SDL_DisplayYUVOverlay(screen,&disp);
    
    //memcpy(p_pic, p_vout->p_sys->p_display->pixels, screen->h * screen->pitch );
        
    
    SDL_FreeYUVOverlay(screen); 

}

