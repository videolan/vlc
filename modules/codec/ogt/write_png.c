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

#include "config.h"
#ifdef HAVE_LIBPNG

#include <stdio.h>
#include <stdlib.h>
#include "write_png.h"
#include <setjmp.h>

typedef void (*snapshot_messenger_t)(char *message);

#define _(x) x

/*
 *   Error functions for use as callbacks by the png libraries
 */

void error_msg(char *message) 
{
  printf("error: %s\n", message);
}

void warning_msg(char *message) 
{
  printf("warning: %s\n", message);
}

static snapshot_messenger_t error_msg_cb = error_msg;
static snapshot_messenger_t warning_msg_cb  = warning_msg;

static void 
user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{

  if(error_msg_cb) {
    char uerror[4096]; 

    memset(&uerror, 0, sizeof(uerror));
    sprintf(uerror, _("Error: %s\n"), error_msg);
    error_msg_cb(uerror);
  }
}

static void 
user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
  if(error_msg_cb) {
    char uerror[4096];
    
    memset(&uerror, 0, sizeof(uerror));
    sprintf(uerror, _("Error: %s\n"), warning_msg);
    warning_msg_cb(uerror);
  }
}

/* 
   Dump an image to a Portable Network Graphics (PNG) file. File_name
   is where the file goes, i_height and i_width are the height and
   width in pixels of the image. The data for the image is stored as a
   linear array of one byte for each of red, green, and blue
   components of an RGB pixel. Thus row[i] will begin at rgb_image +
   i*(i_width*3) and the blue pixel at image[i][0] would be rgb_image +
   i*(i_width*3) + 1.
   
 */
void 
write_png(const char *file_name, png_uint_32 i_height, png_uint_32 i_width,
	  void *rgb_image, /*in*/ png_text *text_ptr, int i_text_count )
{
  FILE *fp;
  png_structp png_ptr;
  png_infop info_ptr;
  png_color_8 sig_bit;
  png_bytep *row_pointers;

  unsigned int i,j;

  /* open the file */
  fp = fopen(file_name, "wb");
  if (fp == NULL)
    return;
  
  /* Create and initialize the png_struct with the desired error handler
   * functions.  If you want to use the default stderr and longjump method,
   * you can supply NULL for the last three parameters.  We also check that
   * the library version is compatible with the one used at compile time,
   * in case we are using dynamically linked libraries.  REQUIRED.
   */
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp) NULL,
				    user_error_fn, user_warning_fn);
  
  if (png_ptr == NULL)
    {
      fclose(fp);
      return;
    }
  
  /* Allocate/initialize the image information data.  REQUIRED */
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
    {
      fclose(fp);
      png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
      return;
    }
  
  /* Set error handling.  REQUIRED if you aren't supplying your own
   * error handling functions in the png_create_write_struct() call.
   */
  if (setjmp(png_ptr->jmpbuf))
    {
      /* If we get here, we had a problem writing the file */
      fclose(fp);
      png_destroy_write_struct(&png_ptr,  (png_infopp) &info_ptr);
      return;
   }

   /* Set up the output control using standard C streams. This
      is required. */
   png_init_io(png_ptr, fp);

   /* Set the image information here.  i_width and i_height are up to 2^31,
    * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
    * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
    * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
    * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
    * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
    * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
    */
   png_set_IHDR(png_ptr, info_ptr, i_width, i_height, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, 
		PNG_FILTER_TYPE_BASE);

   /* For color images: */
   sig_bit.red   = 8;
   sig_bit.green = 8;
   sig_bit.blue  = 8;

   if (text_ptr)
     png_set_text(png_ptr, info_ptr, text_ptr, i_text_count);

   /* Write the file header information.  REQUIRED */
   png_write_info(png_ptr, info_ptr);

   /* Once we write out the header, the compression type on the text
    * chunks gets changed to PNG_TEXT_COMPRESSION_NONE_WR or
    * PNG_TEXT_COMPRESSION_zTXt_WR, so it doesn't get written out again
    * at the end.
    */

   /* Shift the pixels up to a legal bit depth and fill in
    * as appropriate to correctly scale the image.
    */
   png_set_shift(png_ptr, &sig_bit);

   /* pack pixels into bytes */
   png_set_packing(png_ptr);

   row_pointers = png_malloc(png_ptr, i_height*sizeof(png_bytep *));
   for (i=0, j=0; i<i_height; i++, j+=i_width*3) {
     row_pointers[i] = rgb_image + j; 
   }
   
   png_set_rows   (png_ptr, info_ptr, row_pointers);
   png_write_image(png_ptr, row_pointers);

   /* You can write optional chunks like tEXt, zTXt, and tIME at the end
    * as well.
    */

   /* It is REQUIRED to call this to finish writing the rest of the file */
   png_write_end(png_ptr, info_ptr);

   /* if you allocated any text comments, free them here */
   /* free image data if allocated. */

   /* clean up after the write, and free any memory allocated */
   png_destroy_info_struct(png_ptr, &info_ptr);

   /* clean up after the write, and free any memory allocated */
   png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

   fclose(fp);

   return;
}

#ifdef STANDALONE
int
main(int argc, char **argv) 
{
  char image_data[3*16 * 3*16 * 3];
  int i,j,k,l,m;
  char r,g,b,t, or,og,ob;

  or=0x00; og=0xFF; ob=0x0;
  m=0;
  for (i=0; i<3; i++) {
    t=or; or=og; og=ob; ob=t;
    for (j=0; j<16; j++) {
      r=or; g=og; b=ob;
      for (k=0; k<3; k++) {
	for (l=0; l<16; l++) {
	  image_data[m++]=r;
	  image_data[m++]=g;
	  image_data[m++]=b;
	}
	t=r; r=g; g=b; b=t;
      }
    }
  }
  
  write_png("/tmp/pngtest.png", 3*16, 3*16, (void *) image_data) ;
  return 0;
}
#endif /*STANDALONE*/

#endif /*HAVE_LIBPNG*/
