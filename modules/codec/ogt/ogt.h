/*****************************************************************************
 * ogt.h : Overlay Graphics Text (SVCD subtitles) decoder thread interface
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ogt.h,v 1.3 2003/12/22 14:32:55 sam Exp $
 *
 * Author: Rocky Bernstein
 *   based on code from:
 *       Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
 *       Sam Hocevar <sam@zoy.org>
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

#define DECODE_DBG_EXT         1 /* Calls from external routines */
#define DECODE_DBG_CALL        2 /* all calls */
#define DECODE_DBG_PACKET      4 /* packet assembly info */
#define DECODE_DBG_INFO        8

#define DECODE_DEBUG 1
#if DECODE_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_sys && p_sys->i_debug & mask) \
     msg_Dbg(p_dec, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...)
#endif

#define LOG_ERR(args...)  msg_Err( p_input, args )
#define LOG_WARN(args...) msg_Warn( p_input, args )

#define GETINT16(p) ( (p[0] <<  8) +  p[1] ); p +=2;

#define GETINT32(p) ( (p[0] << 24) +  (p[1] << 16) +    \
                      (p[2] <<  8) +  (p[3]) ) ; p += 4;

typedef enum  {
  SUBTITLE_BLOCK_EMPTY,
  SUBTITLE_BLOCK_PARTIAL,
  SUBTITLE_BLOCK_COMPLETE
} packet_state_t;

/* Color and transparency of a pixel or a palette (CLUT) entry */
typedef struct ogt_yuvt_val_s {
  uint8_t y;
  uint8_t u;
  uint8_t v;
  uint8_t t;
} ogt_yuvt_val_t;

struct decoder_sys_t
{
  int i_debug;            /* debugging mask */

  int b_packetizer;

  mtime_t i_pts;          /* Start PTS of subtitle block */
  int i_spu_size;         /* size of the allocated subtitle_data */
  int i_spu;
  packet_state_t state;   /* data-gathering state for this subtitle */
  uint16_t i_image;       /* image number in the subtitle stream; 0 is the
                             first one. */
  uint8_t  i_packet;      /* packet number for above image number; 0 is the
                             first one. */

  block_t *p_block;       /* Bytes of the packet. */

  uint8_t buffer[65536 + 20 ]; /* we will never overflow more than 11 bytes if I'm right */

  vout_thread_t *p_vout;

  /* Move into subpicture_sys_t? */
  uint16_t comp_image_offset;   /* offset from subtitle_data to compressed
                                   image data */
  int comp_image_length;        /* size of the compressed image data */
  int first_field_offset;
  int second_field_offset;

  int metadata_offset;          /* offset to data describing the image */
  int metadata_length;          /* length of metadata */

  int subtitle_data_pos;        /* where to write next chunk */
  int subtitle_data_length;     /* goal for subtitle_data_pos while gathering,
                                   length of used subtitle_data later */

  uint32_t i_duration;          /* how long to display the image, 0 stands
                                   for "until next subtitle" */

  uint16_t i_x_start, i_y_start;  /* position of top leftmost pixel of
                                     image when displayed */
  uint16_t i_width, i_height;   /* dimensions in pixels of image */

  ogt_yuvt_val_t pi_palette[4];

  uint8_t i_options;
  uint8_t i_options2;
  uint8_t i_cmd;
  uint32_t i_cmd_arg;
};

struct subpicture_sys_t
{
    mtime_t i_pts;                                 /* presentation timestamp */

    int   pi_offset[2];                              /* byte offsets to data */
    void *p_data;

    /* Color information */
    vlc_bool_t b_palette;
    ogt_yuvt_val_t pi_palette[4];

#ifndef FIXED
    uint8_t    pi_alpha[4];
    uint8_t    pi_yuv[4][3];
#endif

    /* Link to our input */
    vlc_object_t * p_input;

    /* Cropping properties */
    vlc_mutex_t  lock;
    vlc_bool_t   b_crop;
    unsigned int i_x_start, i_y_start, i_x_end, i_y_end;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void E_(ParsePacket)( decoder_t * );

void E_(RenderSPU)  ( vout_thread_t *, picture_t *, const subpicture_t * );

