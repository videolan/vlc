/*****************************************************************************
 * Dump an Image to a Portable Network Graphics (PNG) file
 ****************************************************************************
  Copyright (C) 2004 VideoLAN
  Author: Rocky Bernstein
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <png.h>

/* 
   Dump an image to a Portable Network Graphics (PNG) file. File_name
   is where the file goes, i_height and i_width are the height and
   width in pixels of the image. The data for the image is stored as a
   linear array RGB pixel entries: one byte for each of red, green,
   and blue component. Thus row[i] will begin at rgb_image +
   i*(i_width*3) and the blue pixel at image[i][0] would be rgb_image
   + i*(i_width*3) + 1.

   text_ptr contains comments that can be written to the image. It can
   be null. i_text_count is the number of entries in text_ptr.
   
 */
void write_png(const char *file_name, png_uint_32 i_height, 
	       png_uint_32 i_width, void *rgb_image, 
	       /*in*/ png_text *text_ptr, int i_text_count );

