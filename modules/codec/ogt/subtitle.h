/*****************************************************************************
 * subtitle.h : Common SVCD and CVD subtitles header
 *****************************************************************************
 * Copyright (C) 2003,2004 VideoLAN
 * $Id: subtitle.h,v 1.9 2004/01/16 13:32:37 rocky Exp $
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

#include "pixmap.h"

#define DECODE_DBG_EXT         1 /* Calls from external routines */
#define DECODE_DBG_CALL        2 /* all calls */
#define DECODE_DBG_PACKET      4 /* packet assembly info */
#define DECODE_DBG_IMAGE       8 /* image bitmaps */
#define DECODE_DBG_TRANSFORM  16 /* bitmap transformations */
#define DECODE_DBG_RENDER     32 /* rendering information */
#define DECODE_DBG_PNG        64 /* Extract subtitles to PNG files. */
#define DECODE_DBG_INFO      128

#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "external call          1\n" \
    "all calls              2\n" \
    "packet assembly info   4\n" \
    "image bitmaps          8\n" \
    "image transformations 16\n" \
    "rendering information 32\n" \
    "extract subtitles     64\n" \
    "misc info            128\n" )

#define SUB_ASPECT_RATIO_TEXT N_("Subtitle aspect-ratio correction")
#define SUB_ASPECT_RATIO_LONGTEXT N_(					\
    "Use this to force the subtitle aspect ratio. If you give a null string " \
    "the right value will be determined automatically. Usually this is what " \
    "you want. For OGT and CVD subtitles this undoes the effect " \
    "of the underlying video scaling. And using a value of 1 will cause " \
    "no correction; subtitles will be scaled with the same aspect " \
    "ratio as as the underlying video (which not correct for OGT or " \
    "CVD subtitles). You can also force another ratio by giving a pair of " \
    "integers x:y where y should between x and twice x. For example 4:3, or " \
    "16:9. Alternatively, you can give a float value expressing pixel " \
    "squareness. For example 1.25 or 1.3333 which mean the same thing as " \
    "4:3 and 16:9 respectively." \
    )

#define DURATION_SCALE_TEXT N_("factor to increase subtitle display interval")
#define DURATION_SCALE_LONGTEXT N_(			    \
  "If you find you need extra time for reading subtitles, " \
  "you can set this higher and it will multiply the display "	\
  "time by that amount. Use 0 to mean until the next "		\
  "subtitle.")

#define HORIZONTAL_CORRECT \
  N_("Add this to starting horizontal position of subtitle.")
#define HORIZONTAL_CORRECT_LONGTEXT N_(			    \
  "If you need to adjust the subtitle starting position horizontally, " \
  "set this. Negative values can be used to shift left. 0 would "	\
  "be no deviation from where the position specified in the subtitle."	\
  )

#define VERTICAL_CORRECT \
  N_("Add this to starting vertical position of subtitle.")
#define VERTICAL_CORRECT_LONGTEXT N_(			    \
  "If you need to adjust the subtitle starting position horizontally, " \
  "set this. Negative values can be used to shift left. 0 would "	\
  "be no deviation from where the position specified in the subtitle."	\
  )

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

#define GETINT16(p) ( (p[0] <<  8) +   p[1] )  ; p +=2;

#define GETINT32(p) ( (p[0] << 24) +  (p[1] << 16) +    \
                      (p[2] <<  8) +  (p[3]) ) ; p += 4;


/* The number of color palette entries allowed in a subtitle. */
#define NUM_SUBTITLE_COLORS 4 

typedef enum  {
  SUBTITLE_BLOCK_EMPTY,
  SUBTITLE_BLOCK_PARTIAL,
  SUBTITLE_BLOCK_COMPLETE
} packet_state_t;

/* The storage used by one pixel */
#define PIXEL_SIZE 4

/* Size in bytes of YUV portion above. */ 
#define YUV_SIZE 3

/* Transparency plane. NOTE: see vlc_video.h for V_PLANE */
#define T_PLANE  V_PLANE+1

struct decoder_sys_t
{
  int            i_debug; /* debugging mask */
  mtime_t        i_pts;   /* Start PTS of subtitle block */
  int            i_spu;
  packet_state_t state;   /* data-gathering state for this subtitle */

  uint16_t       i_image; /* image number in the subtitle stream; 0 is the 
			     first one. */
  uint8_t        i_packet;/* packet number for above image number; 0 is the 
			     first one. */
  block_t        *p_block;/* Bytes of the packet. */
  
  uint8_t buffer[65536 + 20 ]; /* we will never overflow more than 11
				  bytes if I'm right */
  int     b_packetizer;
  int     i_spu_size;	  /* goal for subtitle_data_pos while gathering,
			     size of used subtitle_data later */
  vout_thread_t *p_vout;

  /* FIXME: Remove this? */
  uint8_t *subtitle_data;	/* buffer used to accumulate data from
				   successive packets in the same subtitle */
  int subtitle_data_size;	/* size of the allocated subtitle_data */

  /* Move into subpicture_sys_t? */
  uint16_t i_image_offset;	/* offset from subtitle_data to compressed
				   image data */
  int i_image_length;	        /* size of the compressed image data */
  int first_field_offset;       /* offset of even raster lines. Used
				   only for CVD.
				 */
  int second_field_offset;      /* offset of odd raster lines */
  int metadata_offset;          /* offset to data describing the image */
  int metadata_length;          /* length of metadata */

  int subtitle_data_pos;	/* where to write next chunk */

  mtime_t i_duration;		/* how long to display the image, 0 stands
				   for "until next subtitle" */

  uint16_t i_x_start, i_y_start; /* position of top leftmost pixel of
				     image when displayed */
  uint16_t i_width, i_height;	/* dimensions in pixels of image */

  ogt_yuvt_t p_palette[NUM_SUBTITLE_COLORS];  /* Palette of colors used
						  in subtitle */


  ogt_yuvt_t p_palette_highlight[NUM_SUBTITLE_COLORS]; /* Only used
							   for CVD */

  uint8_t i_options;
  uint8_t i_options2;
  uint8_t i_cmd;
  uint32_t i_cmd_arg;
};

struct subpicture_sys_t
{
  int     i_debug;              /* debugging mask */
  mtime_t i_pts;                /* presentation timestamp */

  uint8_t *p_data;             /* Image data one byte T, Y, U, V */

  /* Link to our input */
  vlc_object_t * p_input;
  
  /* Cropping properties */
  vlc_mutex_t  lock;
  vlc_bool_t   b_crop;
  unsigned int i_x_start, i_y_start, i_x_end, i_y_end;
};
