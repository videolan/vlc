/*****************************************************************************
 * downmix.h : A52 downmix types
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: downmix.h,v 1.2 2002/08/07 00:29:36 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Renaud Dartus <reno@videolan.org>
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

VLC_DECLARE_STRUCT(dm_par_t)
VLC_DECLARE_STRUCT(downmix_t)

struct dm_par_t
{
    float unit;
    float clev;
    float slev;
};

struct downmix_t
{
    VLC_COMMON_MEMBERS

    /* Module used and shortcuts */
    module_t * p_module;
    void (*pf_downmix_3f_2r_to_2ch)(float *, dm_par_t * dm_par);
    void (*pf_downmix_3f_1r_to_2ch)(float *, dm_par_t * dm_par);
    void (*pf_downmix_2f_2r_to_2ch)(float *, dm_par_t * dm_par);
    void (*pf_downmix_2f_1r_to_2ch)(float *, dm_par_t * dm_par);
    void (*pf_downmix_3f_0r_to_2ch)(float *, dm_par_t * dm_par);
    void (*pf_stream_sample_2ch_to_s16)(s16 *, float *left, float *right);
    void (*pf_stream_sample_1ch_to_s16)(s16 *, float *center);
};

