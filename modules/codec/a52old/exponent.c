/*****************************************************************************
 * exponent.c: A52 exponent calculations
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: exponent.c,v 1.1 2002/08/04 17:23:42 sam Exp $
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>

#include "imdct.h"
#include "downmix.h"
#include "adec.h"

#include "internal.h"

#include "exponent.h"

int exponent_unpack (a52dec_t * p_a52dec)
{
    u16 i;

    for (i = 0; i < p_a52dec->bsi.nfchans; i++)
    {
        if (exp_unpack_ch (p_a52dec, UNPACK_FBW, p_a52dec->audblk.chexpstr[i],
                           p_a52dec->audblk.nchgrps[i],
                           p_a52dec->audblk.exps[i][0],
                           &p_a52dec->audblk.exps[i][1],
                           p_a52dec->audblk.fbw_exp[i]))
        {
            return 1;
        }
    }

    if (p_a52dec->audblk.cplinu)
    {
        if (exp_unpack_ch (p_a52dec, UNPACK_CPL, p_a52dec->audblk.cplexpstr,
                           p_a52dec->audblk.ncplgrps,
                           p_a52dec->audblk.cplabsexp << 1,
                           p_a52dec->audblk.cplexps,
                           &p_a52dec->audblk.cpl_exp[p_a52dec->audblk.cplstrtmant]))
        {
            return 1;
        }
    }

    if (p_a52dec->bsi.lfeon)
    {
        if (exp_unpack_ch (p_a52dec, UNPACK_LFE, p_a52dec->audblk.lfeexpstr,
                           2, p_a52dec->audblk.lfeexps[0],
                           &p_a52dec->audblk.lfeexps[1],
                           p_a52dec->audblk.lfe_exp))
        {
            return 1;
        }
    }

    return 0;
}

