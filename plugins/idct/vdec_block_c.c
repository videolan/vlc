/*****************************************************************************
 * vdec_block_c.c: Macroblock copy functions in C
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: vdec_block_c.c,v 1.6 2001/08/22 17:21:45 massiot Exp $
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

#include "vdec_idct.h"

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
void _M( vdec_InitDecode ) ( )
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
 * vdec_AddBlock : add a block
 *****************************************************************************/
void _M( vdec_AddBlock ) ( dctelem_t * p_block, yuv_data_t * p_data,
                           int i_incr )
{
    int i = 8;

    do {
        p_data[0] = pi_crop[ p_data[0] + p_block[0] ];
        p_data[1] = pi_crop[ p_data[1] + p_block[1] ];
        p_data[2] = pi_crop[ p_data[2] + p_block[2] ];
        p_data[3] = pi_crop[ p_data[3] + p_block[3] ];
        p_data[4] = pi_crop[ p_data[4] + p_block[4] ];
        p_data[5] = pi_crop[ p_data[5] + p_block[5] ];
        p_data[6] = pi_crop[ p_data[6] + p_block[6] ];
        p_data[7] = pi_crop[ p_data[7] + p_block[7] ];

        p_data += i_incr;
        p_block += 8;
    } while( --i );
}

/*****************************************************************************
 * vdec_CopyBlock : copy a block
 *****************************************************************************/
void _M( vdec_CopyBlock )( dctelem_t * p_block, yuv_data_t * p_data,
                           int i_incr )
{
    int i = 8;

    do {
        p_data[0] = pi_crop[ p_block[0] ];
        p_data[1] = pi_crop[ p_block[1] ];
        p_data[2] = pi_crop[ p_block[2] ];
        p_data[3] = pi_crop[ p_block[3] ];
        p_data[4] = pi_crop[ p_block[4] ];
        p_data[5] = pi_crop[ p_block[5] ];
        p_data[6] = pi_crop[ p_block[6] ];
        p_data[7] = pi_crop[ p_block[7] ];

        p_data += i_incr;
        p_block += 8;
    } while( --i );
}

