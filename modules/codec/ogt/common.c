/*****************************************************************************
 * Common SVCD and CVD subtitle routines.
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id$
 *
 * Author: Rocky Bernstein <rocky@panix.com>
 *   based on code from:
 *       Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
 *       Samuel Hocevar <sam@zoy.org>
 *       Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "subtitle.h"
#include "pixmap.h"
#include "common.h"
#ifdef HAVE_LIBPNG
#include "write_png.h"
#endif

/*****************************************************************************
 Free Resources associated with subtitle packet.
 *****************************************************************************/
void VCDSubClose( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

    if( !p_sys->b_packetizer )
    {
        /* FIXME check if it's ok to not lock vout */
        if( p_sys->p_vout != NULL && p_sys->p_vout->p_subpicture != NULL )
        {
            subpicture_t *  p_subpic;
            int             i_subpic;

            for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
            {
                p_subpic = &p_sys->p_vout->p_subpicture[i_subpic];

                if( p_subpic != NULL &&
                    ( ( p_subpic->i_status == RESERVED_SUBPICTURE ) ||
                      ( p_subpic->i_status == READY_SUBPICTURE ) ) )
                {
                    vout_DestroySubPicture( p_sys->p_vout, p_subpic );
                }
            }
        }
    }

    if( p_sys->p_block )
    {
        block_ChainRelease( p_sys->p_block );
    }

    free(p_sys->subtitle_data);
    free( p_sys );
}

/*****************************************************************************

Initialize so the next packet will start off a new one.

 *****************************************************************************/
void 
VCDSubInitSubtitleBlock( decoder_sys_t * p_sys ) 
{
  p_sys->i_spu_size = 0;
  p_sys->state      = SUBTITLE_BLOCK_EMPTY;
  p_sys->i_spu      = 0;
  p_sys->p_block    = NULL;
  p_sys->subtitle_data_pos = 0;

}

void 
VCDSubInitSubtitleData(decoder_sys_t *p_sys)
{
  if ( p_sys->subtitle_data ) {
    if ( p_sys->subtitle_data_size < p_sys->i_spu_size ) {
      p_sys->subtitle_data = realloc(p_sys->subtitle_data,
				    p_sys->i_spu_size);
      p_sys->subtitle_data_size = p_sys->i_spu_size;
    }
  } else {
    p_sys->subtitle_data = malloc(p_sys->i_spu_size);
    p_sys->subtitle_data_size = p_sys->i_spu_size;
    /* FIXME: wrong place to get p_sys */
    p_sys->i_image = 0;
  }
  p_sys->subtitle_data_pos = 0;
}

void 
VCDSubAppendData ( decoder_t *p_dec, uint8_t *buffer, uint32_t buf_len )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  int chunk_length = buf_len;

  if ( chunk_length > p_sys->i_spu_size - p_sys->subtitle_data_pos ) {
    msg_Warn( p_dec, "too much data (%d) expecting at most %u",
	      chunk_length, p_sys->i_spu_size - p_sys->subtitle_data_pos );

    chunk_length = p_sys->i_spu_size - p_sys->subtitle_data_pos;
  }

  if ( chunk_length > 0 ) {
#if 0
    int i;
    int8_t *b=buffer;
    for (i=0; i<chunk_length; i++)
      printf ("%02x", b[i]);
    printf("\n");
#endif
    
    memcpy(p_sys->subtitle_data + p_sys->subtitle_data_pos,
	   buffer, chunk_length);
    p_sys->subtitle_data_pos += chunk_length;
    dbg_print(DECODE_DBG_PACKET, "%d bytes appended, pointer now %d",
	      chunk_length, p_sys->subtitle_data_pos);
  }
}


/*****************************************************************************
 * FindVout: Find a vout or wait for one to be created.
 *****************************************************************************/
vout_thread_t *VCDSubFindVout( decoder_t *p_dec )
{
    vout_thread_t *p_vout = NULL;

    /* Find an available video output */
    do
    {
        if( p_dec->b_die || p_dec->b_error )
        {
            break;
        }

        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout )
        {
            break;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    while( 1 );

    return p_vout;
}



/**
   Remove color palette by expanding pixel entries to contain the
   palette values. We work from the free space at the end to the
   beginning so we can expand inline.
*/
static void
InlinePalette ( /*inout*/ uint8_t *p_dest, decoder_sys_t *p_sys )
{
  const unsigned int i_width  = p_sys->i_width;
  const unsigned int i_height = p_sys->i_height;
  int n = (i_height * i_width) - 1;
  uint8_t    *p_from = p_dest;
  ogt_yuvt_t *p_to   = (ogt_yuvt_t *) p_dest;
  
  for ( ; n >= 0 ; n-- ) {
    p_to[n] = p_sys->p_palette[p_from[n]];
    /*p_to[n] = p_sys->p_palette[p_from[3]];*/
  }
}

/**
   Check to see if user has overridden subtitle aspect ratio. 
   0 is returned for no override which means just counteract any
   scaling effects.
*/
unsigned int 
VCDSubGetAROverride(vlc_object_t * p_input, vout_thread_t *p_vout)
{
  char *psz_string = config_GetPsz( p_input, MODULE_STRING "-aspect-ratio" );

  /* Check whether the user tried to override aspect ratio */
  if( !psz_string ) return 0;

  {
    unsigned int i_new_aspect = 0;
    char *psz_parser = strchr( psz_string, ':' );
    
    if( psz_parser )
      {
	*psz_parser++ = '\0';
	i_new_aspect = atoi( psz_string ) * VOUT_ASPECT_FACTOR
	  / atoi( psz_parser );
      }
    else
      {
	i_new_aspect = p_vout->output.i_width * VOUT_ASPECT_FACTOR
	  * atof( psz_string )
	  / p_vout->output.i_height;
      }
    
    return i_new_aspect;
  }
}


/**
   Scales down (reduces size) of p_dest in the x direction as 
   determined through aspect ratio x_scale by y_scale. Scaling
   is done in place. p_spu->i_width, is updated to new width

   The aspect ratio is assumed to be between 1/2 and 1.

   Note: the scaling truncates the new width rather than rounds it.
   Perhaps something one might want to address.
*/
void
VCDSubScaleX( decoder_t *p_dec, subpicture_t *p_spu, 
	      unsigned int i_scale_x, unsigned int i_scale_y )
{
  int i_row, i_col;

  decoder_sys_t *p_sys = p_dec->p_sys;
  uint8_t *p_src1 = p_spu->p_sys->p_data;
  uint8_t *p_src2 = p_src1 + PIXEL_SIZE;
  uint8_t *p_dst  = p_src1;
  unsigned int i_new_width = (p_spu->i_width * i_scale_x) / i_scale_y ;
  unsigned int i_used=0;  /* Number of bytes used up in p_src1. */

  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_TRANSFORM) , 
	     "aspect ratio %i:%i, Old width: %d, new width: %d", 
	     i_scale_x, i_scale_y, p_spu->i_width, i_new_width);

  if (! (i_scale_x < i_scale_y && i_scale_y < i_scale_x+i_scale_x) )
    {
      msg_Warn( p_dec, "Need x < y < 2x. x: %i, y: %i", i_scale_x, i_scale_y );
      return;
    }
  
  for ( i_row=0; i_row <= p_spu->i_height - 1; i_row++ ) {

    if (i_used != 0) {
      /* Discard the remaining piece of the column of the previous line*/
      i_used=0;
      p_src1 = p_src2;
      p_src2 += PIXEL_SIZE;
    }
    
    for ( i_col=0; i_col <= p_spu->i_width - 2; i_col++ ) {
      unsigned int i;
      unsigned int w1= i_scale_x - i_used;
      unsigned int w2;
      
      if ( i_scale_y - w1 <= i_scale_x ) {
	/* Average spans 2 pixels. */
	w2 = i_scale_y - w1;

	for (i = 0; i < PIXEL_SIZE; i++ ) {
	  *p_dst = ( (*p_src1 * w1) + (*p_src2 * w2) ) / i_scale_y;
	  p_src1++; p_src2++; p_dst++;
	}
      } else {
	/* Average spans 3 pixels. */
	unsigned int w0 = w1;
	unsigned int w1 = i_scale_x;
	uint8_t *p_src0 = p_src1;
	w2 = i_scale_y - w0 - w1;
	p_src1 = p_src2;
	p_src2 += PIXEL_SIZE;
	
	for (i = 0; i < PIXEL_SIZE; i++ ) {
	  *p_dst = ( (*p_src0 * w0) + (*p_src1 * w1) + (*p_src2 * w2) ) 
		     / i_scale_y;
	  p_src0++; p_src1++; p_src2++; p_dst++;
	}
	i_col++;
      }

      i_used = w2;

      if (i_scale_x == i_used) {
	/* End of last pixel was end of p_src2. */
	p_src1 = p_src2;
	p_src2 += PIXEL_SIZE;
	i_col++;
	i_used = 0;
      }
    }
  }
  p_spu->i_width = i_new_width;

  if ( p_sys && p_sys->i_debug & DECODE_DBG_TRANSFORM )
  { 
    ogt_yuvt_t *p_source = (ogt_yuvt_t *) p_spu->p_sys->p_data;
    for ( i_row=0; i_row < p_spu->i_height; i_row++ ) {
      for ( i_col=0; i_col < p_spu->i_width; i_col++ ) {
	printf("%1x", p_source->s.t);
	p_source++;
      }
      printf("\n");
    }
  }

}

/**
   The video may be scaled. However subtitle bitmaps assume an 1:1
   aspect ratio. So unless the user has specified otherwise, we
   need to scale to compensate for or undo the effects of video
   output scaling.
   
   Perhaps this should go in the Render routine? The advantage would
   be that it will deal with a dynamically changing aspect ratio.
   The downside is having to scale many times for each render call.

   We also expand palette entries here, unless we are dealing with a 
   palettized chroma (e.g. RGB2).
*/

void 
VCDSubHandleScaling( subpicture_t *p_spu, decoder_t *p_dec )
{
  vlc_object_t * p_input = p_spu->p_sys->p_input;
  vout_thread_t *p_vout = vlc_object_find( p_input, VLC_OBJECT_VOUT, 
                                           FIND_CHILD );
  int i_aspect_x, i_aspect_y;
  uint8_t *p_dest = (uint8_t *)p_spu->p_sys->p_data;

  if (p_vout) {
    /* Check for user-configuration override. */
    unsigned int i_new_aspect;
    
    if ( p_vout->output.i_chroma == VLC_FOURCC('R','G','B','2') ) {
      /* This is an unscaled palettized format. We don't allow 
         user scaling here. And to make the render process faster,
         we don't expand the palette entries into a color value.
       */
      return;
    }
        
    InlinePalette( p_dest, p_dec->p_sys );
    i_new_aspect = VCDSubGetAROverride( p_input, p_vout );

    if (i_new_aspect == VOUT_ASPECT_FACTOR) {
      /* For scaling 1:1, nothing needs to be done. Note this means
         subtitles will get scaled the same way the video does.
      */
      ;
    } else {
      if (0 == i_new_aspect) {
        /* Counteract the effects of background video scaling when
           there is scaling. That's why x and y are reversed from
           the else branch in the call below.
        */
        switch( p_vout->output.i_chroma )
          {
            /* chromas in which scaling is done outside of our
               blending routine, so we need to compensate for those
               effects before blending gets called: */
          case VLC_FOURCC('I','4','2','0'):
          case VLC_FOURCC('I','Y','U','V'):
          case VLC_FOURCC('Y','V','1','2'):
          case VLC_FOURCC('Y','U','Y','2'):
            break;
            
            /* chromas in which scaling is done in our blending 
               routine and thus we don't do it here: */
          case VLC_FOURCC('R','V','1','6'):
          case VLC_FOURCC('R','V','2','4'):
          case VLC_FOURCC('R','V','3','2'):
          case VLC_FOURCC('R','G','B','2'):
            return;
            break;
            
          default:
            msg_Err( p_vout, "unknown chroma %x", 
                     p_vout->output.i_chroma );
            return;
            break;
          }
        /* We get here only for scaled chromas. */
        vlc_reduce( &i_aspect_x, &i_aspect_y, p_vout->render.i_aspect,
                    VOUT_ASPECT_FACTOR, 0 );
      } else {
        /* User knows best? */
        vlc_reduce( &i_aspect_x, &i_aspect_y, p_vout->render.i_aspect,
                    VOUT_ASPECT_FACTOR, 0 );
      }
      VCDSubScaleX( p_dec, p_spu, i_aspect_x, i_aspect_y );
    }
  }
}


/**
 * DestroySPU: subpicture destructor
 */
void VCDSubDestroySPU( subpicture_t *p_spu )
{
    if( p_spu->p_sys->p_input )
    {
        /* Detach from our input thread */
        vlc_object_release( p_spu->p_sys->p_input );
    }

    vlc_mutex_destroy( &p_spu->p_sys->lock );
    free( p_spu->p_sys );
}

/*****************************************************************************
  This callback is called from the input thread when we need cropping
 *****************************************************************************/
int VCDSubCropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VCDSubUpdateSPU( (subpicture_t *)p_data, p_object );

    return VLC_SUCCESS;
}


/*****************************************************************************
  update subpicture settings
 *****************************************************************************
  This function is called from CropCallback and at initialization time, to
  retrieve crop information from the input.
 *****************************************************************************/
void VCDSubUpdateSPU( subpicture_t *p_spu, vlc_object_t *p_object )
{
    vlc_value_t val;

    p_spu->p_sys->b_crop = val.b_bool;
    if( !p_spu->p_sys->b_crop )
    {
        return;
    }

    if ( VLC_SUCCESS == var_Get( p_object, "x-start", &val ) )
      p_spu->p_sys->i_x_start = val.i_int;
    if ( VLC_SUCCESS == var_Get( p_object, "y-start", &val ) )
      p_spu->p_sys->i_y_start = val.i_int;
    if ( VLC_SUCCESS == var_Get( p_object, "x-end", &val ) )
      p_spu->p_sys->i_x_end = val.i_int;
    if ( VLC_SUCCESS == var_Get( p_object, "y-end", &val ) )
      p_spu->p_sys->i_y_end = val.i_int;

}

/* 
   Dump an a subtitle image to standard output - for debugging.
 */
void VCDSubDumpImage( uint8_t *p_image, uint32_t i_height, uint32_t i_width )
{
  uint8_t *p = p_image;
  unsigned int i_row;    /* scanline row number */
  unsigned int i_column; /* scanline column number */

  printf("-------------------------------------\n++");
  for ( i_row=0; i_row < i_height; i_row ++ ) {
    for ( i_column=0; i_column<i_width; i_column++ ) {
      printf("%1d", *p++ & 0x03);
    }
    printf("\n++");
  }
  printf("\n-------------------------------------\n");
}

#ifdef HAVE_LIBPNG

#define PALETTE_SIZE  4
/* Note the below assumes the above is a power of 2 */
#define PALETTE_SIZE_MASK (PALETTE_SIZE-1)

/* 
   Dump an a subtitle image to a Portable Network Graphics (PNG) file.
   All we do here is convert YUV palette entries to RGB, expand
   the image into a linear RGB pixel array, and call the routine
   that does the PNG writing.
 */

void 
VCDSubDumpPNG( uint8_t *p_image, decoder_t *p_dec,
	       uint32_t i_height, uint32_t i_width, const char *filename,
	       png_text *text_ptr, int i_text_count )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  uint8_t *p = p_image;
  uint8_t *image_data = malloc(RGB_SIZE * i_height * i_width );
  uint8_t *q = image_data;
  unsigned int i_row;    /* scanline row number */
  unsigned int i_column; /* scanline column number */
  uint8_t rgb_palette[PALETTE_SIZE * RGB_SIZE];
  int i;

  dbg_print( (DECODE_DBG_CALL), "%s", filename);
  
  if (NULL == image_data) return;

  /* Convert palette YUV into RGB. */
  for (i=0; i<PALETTE_SIZE; i++) {
    ogt_yuvt_t *p_yuv     = &(p_sys->p_palette[i]);
    uint8_t   *p_rgb_out  = &(rgb_palette[i*RGB_SIZE]);
    yuv2rgb( p_yuv, p_rgb_out );
  }
  
  /* Convert palette entries into linear RGB array. */
  for ( i_row=0; i_row < i_height; i_row ++ ) {
    for ( i_column=0; i_column<i_width; i_column++ ) {
      uint8_t *p_rgb = &rgb_palette[ ((*p)&PALETTE_SIZE_MASK)*RGB_SIZE ];
      *q++ = p_rgb[0];
      *q++ = p_rgb[1];
      *q++ = p_rgb[2];
      p++;
    }
  }
  
  write_png( filename, i_height, i_width, image_data, text_ptr, i_text_count );
  free(image_data);
}
#endif /*HAVE_LIBPNG*/


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
