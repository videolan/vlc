/*****************************************************************************
 * video_text.h : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_text.h,v 1.7 2001/03/21 13:42:35 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

/* Text styles - these are primary text styles, used by the vout_Print function.
 * They may be ignored or interpreted by higher level functions */
#define WIDE_TEXT                    1<<0         /* interspacing is doubled */
#define ITALIC_TEXT                  1<<1                          /* italic */
#define OPAQUE_TEXT                  1<<2            /* text with background */
#define OUTLINED_TEXT                1<<3           /* border around letters */
#define VOID_TEXT                    1<<4                   /* no foreground */


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
p_vout_font_t   vout_LoadFont   ( const char *psz_name );
void            vout_UnloadFont ( p_vout_font_t p_font );
void            vout_TextSize   ( p_vout_font_t p_font, int i_style,
                                  const char *psz_text,
                                  int *pi_width, int *pi_height );
void            vout_Print      ( p_vout_font_t p_font, byte_t *p_pic,
                                  int i_bytes_per_pixel, int i_bytes_per_line,
                                  u32 i_char_color, u32 i_border_color, u32 i_bg_color,
                                  int i_style, const char *psz_text, int i_percent );










