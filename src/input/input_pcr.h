/*****************************************************************************
 * input_pcr.h: PCR management interface
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* Maximum number of samples used to compute the dynamic average value,
 * it is also the maximum of c_average in the pcr_descriptor_struct.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1) */
#define PCR_MAX_AVERAGE_COUNTER 40

/* Maximum gap allowed between two PCRs. */
#define PCR_MAX_GAP 1000000

/* synchro states */
#define SYNCHRO_NOT_STARTED 1
#define SYNCHRO_START       2
#define SYNCHRO_REINIT      3

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int            input_PcrInit        ( input_thread_t *p_input );
void           input_PcrDecode      ( input_thread_t *p_input, es_descriptor_t* p_es,
                                       u8* p_pcr_data );
void           input_PcrEnd         ( input_thread_t *p_input );
