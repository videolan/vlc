/*****************************************************************************
 * decoder.h: faad decoder modules
 *
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: decoder.h,v 1.6 2003/01/25 18:09:30 fenrir Exp $
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

#define AAC_MAXCHANNELS 64

typedef struct adec_thread_s
{

    /*
     * faad decoder session
     */
    /* faad stuff */
    faacDecHandle *p_handle;

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Input properties
     */
    decoder_fifo_t *p_fifo;

    uint8_t        *p_buffer;
    int             i_buffer;

    /*
     * Output properties
     */
    aout_instance_t *   p_aout;       /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;

    audio_date_t        date;
    mtime_t             pts;

} adec_thread_t;

