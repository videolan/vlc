/*****************************************************************************
 * decoder.h: faad decoder modules 
 *
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: decoder.h,v 1.4 2002/10/27 18:06:33 fenrir Exp $
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

typedef struct waveformatex_s
{
    u16 i_formattag;
    u16 i_channels;
    u32 i_samplespersec;
    u32 i_avgbytespersec;
    u16 i_blockalign;
    u16 i_bitspersample;
    u16 i_size; /* the extra size in bytes */
    u8 *p_data; /* The extra data */
} waveformatex_t;

typedef struct adec_thread_s
{

    waveformatex_t  format;

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
    
    u8              *p_buffer;
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


static int i_channels_maps[6] = 
{
    0,
    AOUT_CHAN_MONO,     AOUT_CHAN_STEREO,
    AOUT_CHAN_3F,       AOUT_CHAN_2F2R,
    AOUT_CHAN_3F2R
};


