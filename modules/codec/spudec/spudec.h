/*****************************************************************************
 * spudec.h : sub picture unit decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: spudec.h,v 1.8 2003/11/22 19:55:47 fenrir Exp $
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

struct decoder_sys_t
{
    int b_packetizer;

    mtime_t i_pts;
    int i_spu_size;
    int i_rle_size;
    int i_spu;

    block_t *p_block;

    uint8_t buffer[65536 + 20 ]; /* we will never overflow more than 11 bytes if I'm right */

    vout_thread_t *p_vout;
};

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
    unsigned int i_x_start, i_y_start, i_x_end, i_y_end;
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
void E_(ParsePacket)( decoder_t * );

void E_(RenderSPU)  ( vout_thread_t *, picture_t *, const subpicture_t * );

