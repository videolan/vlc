/*****************************************************************************
 * spudec.h : sub picture unit decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: spudec.h,v 1.4 2002/11/06 21:48:24 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

typedef struct spudec_thread_t spudec_thread_t;

struct subpicture_sys_t
{
    mtime_t i_pts;                                 /* presentation timestamp */

    int   pi_offset[2];                              /* byte offsets to data */
    void *p_data;

    /* Color information */
    vlc_bool_t b_palette;
    uint8_t    pi_alpha[4];
    uint8_t    pi_yuv[4][3];

    /* Link to our input */
    vlc_object_t * p_input;

    /* Cropping properties */
    vlc_mutex_t  lock;
    vlc_bool_t   b_crop;
    int          i_x_start, i_y_start, i_x_end, i_y_end;
};

/*****************************************************************************
 * subtitler_font_t : proportional font
 *****************************************************************************/
typedef struct subtitler_font_s
{
    int                 i_height;              /* character height in pixels */
    int                 i_width[256];          /* character widths in pixels */
    int                 i_memory[256]; /* amount of memory used by character */
    int *               p_length[256];                   /* line byte widths */
    u16 **              p_offset[256];                /* pointer to RLE data */
} subtitler_font_t;

/*****************************************************************************
 * spudec_thread_t : sub picture unit decoder thread descriptor
 *****************************************************************************/
struct spudec_thread_t
{
    /*
     * Thread properties and locks
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Output properties
     */
    vout_thread_t *     p_vout;          /* needed to create the spu objects */

    /*
     * Private properties
     */
    int                 i_spu_size;            /* size of current SPU packet */
    int                 i_rle_size;                  /* size of the RLE part */
};

/*****************************************************************************
 * Amount of bytes we GetChunk() in one go
 *****************************************************************************/
#define SPU_CHUNK_SIZE              0x200

/*****************************************************************************
 * SPU commands
 *****************************************************************************/
#define SPU_CMD_FORCE_DISPLAY       0x00
#define SPU_CMD_START_DISPLAY       0x01
#define SPU_CMD_STOP_DISPLAY        0x02
#define SPU_CMD_SET_PALETTE         0x03
#define SPU_CMD_SET_ALPHACHANNEL    0x04
#define SPU_CMD_SET_COORDINATES     0x05
#define SPU_CMD_SET_OFFSETS         0x06
#define SPU_CMD_END                 0xff

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  E_(SyncPacket)           ( spudec_thread_t * );
void E_(ParsePacket)          ( spudec_thread_t * );

void E_(RenderSPU)            ( vout_thread_t *, picture_t *,
                                const subpicture_t * );

void E_(ParseText)            ( spudec_thread_t *, subtitler_font_t * );

subtitler_font_t *E_(subtitler_LoadFont) ( vout_thread_t *, const char * );
void E_(subtitler_UnloadFont)   ( vout_thread_t *, subtitler_font_t * );
void E_(subtitler_PlotSubtitle) ( vout_thread_t *, char *, subtitler_font_t *,
                                  mtime_t, mtime_t );
