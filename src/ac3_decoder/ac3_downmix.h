/*****************************************************************************
 * ac3_downmix.h: ac3 downmix functions
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: ac3_downmix.h,v 1.6 2001/04/30 21:04:20 reno Exp $
 *
 * Authors: Renaud Dartus <reno@videolan.org>
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

/* C functions */
void downmix_3f_2r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_3f_1r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_2f_2r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_2f_1r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_3f_0r_to_2ch_c(float *samples, dm_par_t * dm_par);            
void stream_sample_2ch_to_s16_c(s16 *s16_samples, float *left, float *right);
void stream_sample_1ch_to_s16_c(s16 *s16_samples, float *center); 

#if 0
/* Kni functions */
void downmix_3f_2r_to_2ch_kni(float *samples, dm_par_t * dm_par);
void downmix_3f_1r_to_2ch_kni(float *samples, dm_par_t * dm_par);
void downmix_2f_2r_to_2ch_kni(float *samples, dm_par_t * dm_par);
void downmix_2f_1r_to_2ch_kni(float *samples, dm_par_t * dm_par);
void downmix_3f_0r_to_2ch_kni(float *samples, dm_par_t * dm_par);            
void stream_sample_2ch_to_s16_kni(s16 *s16_samples, float *left, float *right);
void stream_sample_1ch_to_s16_kni(s16 *s16_samples, float *center);  
#endif
