/*****************************************************************************
 * audio_sys.h : header of the method-dependant functions library
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
 * Required headers:
 * - "common.h" ( byte_t )
 * - "audio_output.h" ( aout_dsp_t )
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  aout_SysOpen            ( aout_thread_t *p_aout );
int  aout_SysReset           ( aout_thread_t *p_aout );
int  aout_SysSetFormat       ( aout_thread_t *p_aout );
int  aout_SysSetChannels     ( aout_thread_t *p_aout );
int  aout_SysSetRate         ( aout_thread_t *p_aout );
long aout_SysGetBufInfo      ( aout_thread_t *p_aout, long l_buffer_info );
void aout_SysPlaySamples     ( aout_thread_t *p_aout, byte_t *buffer, int i_size );
void aout_SysClose           ( aout_thread_t *p_aout );

