/*****************************************************************************
 * audio.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: audio.h,v 1.3 2003/01/25 16:59:49 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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


/* for an audio stream */
typedef struct waveformatex_s
{
    u16 i_formattag;
    u16 i_nb_channels;
    u32 i_samplespersec;
    u32 i_avgbytespersec;
    u16 i_blockalign;
    u16 i_bitspersample;
    u16 i_size; /* the extra size in bytes */

    u8  *p_data; /* The extra data */
} waveformatex_t;

typedef struct adec_thread_s
{
    DECODER_THREAD_COMMON

//    waveformatex_t  format;

    /*
     * Output properties
     */

    uint8_t *             p_output;

    aout_instance_t *     p_aout;       /* opaque */
    aout_input_t *        p_aout_input; /* opaque */
    audio_sample_format_t output_format;

    audio_date_t          date;

} adec_thread_t;

/*
 * Local prototypes
 */
int      E_( InitThread_Audio )   ( adec_thread_t * );
void     E_( EndThread_Audio )    ( adec_thread_t * );
void     E_( DecodeThread_Audio ) ( adec_thread_t * );

