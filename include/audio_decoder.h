/*****************************************************************************
 * audio_decoder.h : audio decoder interface
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

/**** audio decoder API - public audio decoder structures */

typedef struct audiodec_s audiodec_t;

typedef struct adec_sync_info_s {
    int sample_rate;	/* sample rate in Hz */
    int frame_size;	/* frame size in bytes */
    int bit_rate;	/* nominal bit rate in kbps */
} adec_sync_info_t;

typedef struct adec_byte_stream_s {
    u8 * p_byte;
    u8 * p_end;
    void * info;
} adec_byte_stream_t;

/**** audio decoder API - functions publically provided by the audio dec. ****/

int adec_init (audiodec_t * p_adec);
int adec_sync_frame (audiodec_t * p_adec, adec_sync_info_t * p_sync_info);
int adec_decode_frame (audiodec_t * p_adec, s16 * buffer);
static adec_byte_stream_t * adec_byte_stream (audiodec_t * p_adec);

/**** audio decoder API - user functions to be provided to the audio dec. ****/

void adec_byte_stream_next (adec_byte_stream_t * p_byte_stream);

/**** EVERYTHING AFTER THIS POINT IS PRIVATE ! DO NOT USE DIRECTLY ****/

/**** audio decoder internal structures ****/

typedef struct adec_bank_s {
    float               v1[512];
    float               v2[512];
    float *             actual;
    int                 pos;
} adec_bank_t;

typedef struct adec_bit_stream_s {
    u32 buffer;
    int i_available;
    adec_byte_stream_t byte_stream;
    int total_bytes_read;
} adec_bit_stream_t;

struct audiodec_s {
    /*
     * Input properties
     */

    /* The bit stream structure handles the PES stream at the bit level */
    adec_bit_stream_t	bit_stream;

    /*
     * Decoder properties
     */
    u32			header;
    int			frame_size;
    adec_bank_t         bank_0;
    adec_bank_t         bank_1;
};

/**** audio decoder inline functions ****/

static adec_byte_stream_t * adec_byte_stream (audiodec_t * p_adec)
{
    return &(p_adec->bit_stream.byte_stream);
}
