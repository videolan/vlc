/*****************************************************************************
 * vdec_block_mmx.c: Macroblock copy functions in MMX assembly
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: vdec_block_mmx.c,v 1.4 2001/06/20 07:43:48 sam Exp $
 *
 * Authors: Gaël Hendryckx <jimmy@via.ecp.fr>
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

#include "vdec_motion.h"
#include "video_decoder.h"

#include "vpar_blocks.h"

#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

#include "vdec_block.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * vdec_InitDecode: initialize video decoder thread
 *****************************************************************************/
void _M( vdec_InitDecode ) ( vdec_thread_t *p_vdec )
{
    ;
}

/*****************************************************************************
 * AddBlock : add a block
 *****************************************************************************/
static __inline__ void AddBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                 yuv_data_t * p_data, int i_incr )
{
    asm __volatile__ ( 
            "pxor       %%mm7,%%mm7\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      (%2),%%mm2\n\t"
            "paddw      8(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      16(%2),%%mm2\n\t"
            "paddw      24(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      32(%2),%%mm2\n\t"
            "paddw      40(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      48(%2),%%mm2\n\t"
            "paddw      56(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      64(%2),%%mm2\n\t"
            "paddw      72(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      80(%2),%%mm2\n\t"
            "paddw      88(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      96(%2),%%mm2\n\t"
            "paddw      104(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %3,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      112(%2),%%mm2\n\t"
            "paddw      120(%2),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"

            //"emms"
            : "=r" (p_data)
            : "0" (p_data), "r" (p_block), "r" (i_incr + 8) );
}

/*****************************************************************************
 * CopyBlock : copy a block
 *****************************************************************************/
static  __inline__ void CopyBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                   yuv_data_t * p_data, int i_incr )
{
    asm __volatile__ (
            "movq         (%2),%%mm0\n\t"
            "packuswb   8(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        16(%2),%%mm0\n\t"
            "packuswb   24(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        32(%2),%%mm0\n\t"
            "packuswb   40(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        48(%2),%%mm0\n\t"
            "packuswb   56(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        64(%2),%%mm0\n\t"
            "packuswb   72(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        80(%2),%%mm0\n\t"
            "packuswb   88(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        96(%2),%%mm0\n\t"
            "packuswb   104(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %3,%0\n\t"

            "movq        112(%2),%%mm0\n\t"
            "packuswb   120(%2),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"

            //"emms"
            : "=r" (p_data)
            : "0" (p_data), "r" (p_block), "r" (i_incr + 8) );
}

void _M( vdec_DecodeMacroblockC ) ( vdec_thread_t *p_vdec,
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

