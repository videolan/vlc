/*****************************************************************************
 * vdec_block_c.c: Macroblock copy functions in C
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: vdec_block_c.c,v 1.5 2001/07/17 09:48:07 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/* MODULE_NAME defined in Makefile together with -DBUILTIN */
#ifdef BUILTIN
#   include "modules_inner.h"
#else
#   define _M( foo ) foo
#endif

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <string.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "vdec_ext-plugins.h"

#include "vdec_block.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Static variables
 *****************************************************************************
 * We can keep them static since they will always contain the same values.
 *****************************************************************************/
static u8  pi_crop_buf[VDEC_CROPRANGE];
static u8 *pi_crop;

/*****************************************************************************
 * vdec_InitDecode: initialize video decoder thread
 *****************************************************************************/
void _M( vdec_InitDecode ) ( vdec_thread_t *p_vdec )
{
    int i_dummy;

    /* Init crop table */
    pi_crop = pi_crop_buf + (VDEC_CROPRANGE >> 1);

    for( i_dummy = -(VDEC_CROPRANGE >> 1); i_dummy < 0; i_dummy++ )
    {
        pi_crop[i_dummy] = 0;
    }

    for( ; i_dummy < 255; i_dummy ++ )
    {
        pi_crop[i_dummy] = i_dummy;
    }

    for( ; i_dummy < (VDEC_CROPRANGE >> 1) -1; i_dummy++ )
    {
        pi_crop[i_dummy] = 255;
    }
}

/*****************************************************************************
 * AddBlock : add a block
 *****************************************************************************/
static __inline__ void AddBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                 yuv_data_t * p_data, int i_incr )
{
    int i_x, i_y;

    for( i_y = 0; i_y < 8; i_y++ )
    {
        for( i_x = 0; i_x < 8; i_x++ )
        {
            *p_data = pi_crop[*p_data + *p_block++];
            p_data++;
        }
        p_data += i_incr;
    }
}

/*****************************************************************************
 * CopyBlock : copy a block
 *****************************************************************************/
static __inline__ void CopyBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                  yuv_data_t * p_data, int i_incr )
{
    int i_x, i_y;

    for( i_y = 0; i_y < 8; i_y++ )
    {
        for( i_x = 0; i_x < 8; i_x++ )
        {
            *p_data++ = pi_crop[*p_block++];
        }
        p_data += i_incr;
    }
}

void _M( vdec_DecodeMacroblockC ) ( vdec_thread_t *p_vdec, macroblock_t * p_mb )
{
    if( !(p_mb->i_mb_type & MB_INTRA) )
    {
        /*
         * Motion Compensation (ISO/IEC 13818-2 section 7.6)
         */
        if( p_mb->pf_motion == 0 )
        {
            intf_WarnMsg( 2, "pf_motion set to NULL" );
        }
        else
        {
            p_mb->pf_motion( p_mb );
        }

        DECODEBLOCKSC( AddBlock )
    }
    else
    {
        DECODEBLOCKSC( CopyBlock )
    }
}

void _M( vdec_DecodeMacroblockBW ) ( vdec_thread_t *p_vdec,
                                     macroblock_t * p_mb )
{
    if( !(p_mb->i_mb_type & MB_INTRA) )
    {
        /*
         * Motion Compensation (ISO/IEC 13818-2 section 7.6)
         */
        if( p_mb->pf_motion == 0 )
        {
            intf_WarnMsg( 2, "pf_motion set to NULL" );
        }
        else
        {
            p_mb->pf_motion( p_mb );
        }

        DECODEBLOCKSBW( AddBlock )
    }
    else
    {
        DECODEBLOCKSBW( CopyBlock )
    }
}

