/*****************************************************************************
 * lpcm_decoder.h : lpcm decoder interface
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

typedef struct lpcmdec_s lpcmdec_t;

typedef struct lpcm_sync_info_s {
    int sample_rate;	/* sample rate in Hz */
    int frame_size;	/* frame size in bytes */
    int bit_rate;	/* nominal bit rate in kbps */
} lpcm_sync_info_t;

typedef struct lpcm_byte_stream_s {
    u8 * p_byte;
    u8 * p_end;
    void * info;
} lpcm_byte_stream_t;


int lpcm_init (lpcmdec_t * p_lpcmdec);
int lpcm_sync_frame (lpcmdec_t * p_lpcmdec, lpcm_sync_info_t * p_sync_info);
int lpcm_decode_frame (lpcmdec_t * p_lcpmdec, s16 * buffer);
//static lpcm_byte_stream_t * lpcm_byte_stream (lcpmdec_t * p_lpcmdec);


void lpcm_byte_stream_next (lpcm_byte_stream_t * p_byte_stream);

typedef struct lpcm_bit_stream_s {
    u32 buffer;
    int i_available;
    lpcm_byte_stream_t byte_stream;

} lpcm_bit_stream_t;

struct lpcmdec_s {
    /*
     * Input properties
     */

    /* The bit stream structure handles the PES stream at the bit level */
    lpcm_bit_stream_t	bit_stream;
};


static lpcm_byte_stream_t * lpcm_byte_stream (lpcmdec_t * p_lpcmdec)
{
    return &(p_lpcmdec->bit_stream.byte_stream);
}
