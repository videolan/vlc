/*****************************************************************************
 * ac3_exponent.c: ac3 exponent calculations
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_exponent.c,v 1.2 2001/11/25 22:52:21 gbazin Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
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

#include <string.h>                                    /* memcpy(), memset() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"

#include "modules.h"
#include "modules_export.h"

#include "stream_control.h"
#include "input_ext-dec.h"


#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"

#include "ac3_internal.h"

#include "ac3_exponent.h"

int exponent_unpack (ac3dec_t * p_ac3dec)
{
    u16 i;

    for (i = 0; i < p_ac3dec->bsi.nfchans; i++)
    {
        if (exp_unpack_ch (p_ac3dec, UNPACK_FBW, p_ac3dec->audblk.chexpstr[i],
                           p_ac3dec->audblk.nchgrps[i],
                           p_ac3dec->audblk.exps[i][0],
                           &p_ac3dec->audblk.exps[i][1],
                           p_ac3dec->audblk.fbw_exp[i]))
        {
            return 1;
        }
    }

    if (p_ac3dec->audblk.cplinu)
    {
        if (exp_unpack_ch (p_ac3dec, UNPACK_CPL, p_ac3dec->audblk.cplexpstr,
                           p_ac3dec->audblk.ncplgrps,
                           p_ac3dec->audblk.cplabsexp << 1,
                           p_ac3dec->audblk.cplexps,
                           &p_ac3dec->audblk.cpl_exp[p_ac3dec->audblk.cplstrtmant]))
        {
            return 1;
        }
    }

    if (p_ac3dec->bsi.lfeon)
    {
        if (exp_unpack_ch (p_ac3dec, UNPACK_LFE, p_ac3dec->audblk.lfeexpstr,
                           2, p_ac3dec->audblk.lfeexps[0],
                           &p_ac3dec->audblk.lfeexps[1],
                           p_ac3dec->audblk.lfe_exp))
        {
            return 1;
        }
    }

    return 0;
}

