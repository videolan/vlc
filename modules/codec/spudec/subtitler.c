/*****************************************************************************
 * subtitler.c : subtitler font routines
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Andrew Flintham <amf@cus.org.uk>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */
#include <errno.h>                                                  /* errno */
#include <fcntl.h>                                                 /* open() */
#include <ctype.h>                                              /* toascii() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                    /* read(), close() */
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "spudec.h"

/*****************************************************************************
 * subtitler_line : internal structure for an individual line in a subtitle
 *****************************************************************************/
typedef struct subtitler_line_s
{
   struct subtitler_line_s *   p_next;
   char *                      p_text;
} subtitler_line_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static uint16_t *PlotSubtitleLine( char *, subtitler_font_t *, int,
                                   uint16_t * );
static void      DestroySPU      ( subpicture_t * );

/*****************************************************************************
 * subtitler_LoadFont: load a run-length encoded font file into memory
 *****************************************************************************
 * RLE font files have the following format:
 *
 *     2 bytes : magic number: 0x36 0x05
 *     1 byte  : font height in rows
 *
 *     then, per character:
 *     1 byte  : character 
 *     1 byte  : character width in pixels
 *
 *         then, per row:
 *         1 byte  : length of row, in entries
 *         
 *             then, per entry
 *             1 byte : colour
 *             1 byte : number of pixels of that colour
 *
 *     to end:
 *     1 byte : 0xff
 *****************************************************************************/
subtitler_font_t* E_(subtitler_LoadFont)( vout_thread_t * p_vout,
                                      const char * psz_name )
{
    subtitler_font_t * p_font;

    int                i;
    int                i_file;
    int                i_char;
    int                i_length;
    int                i_line;
    int                i_total_length;

    byte_t             pi_buffer[512];                        /* file buffer */

    msg_Dbg( p_vout, "loading font '%s'", psz_name );

    i_file = open( psz_name, O_RDONLY );

    if( i_file == -1 )
    {
        msg_Err( p_vout, "can't open font file '%s' (%s)", psz_name,
                         strerror(errno) );
        return( NULL );
    }

    /* Read magick number */
    if( read( i_file, pi_buffer, 2 ) != 2 )
    {
        msg_Err( p_vout, "unexpected end of font file '%s'", psz_name );
        close( i_file );
        return( NULL );
    }
    if( pi_buffer[0] != 0x36 || pi_buffer[1] != 0x05 )
    {
        msg_Err( p_vout, "file '%s' is not a font file", psz_name );
        close( i_file );
        return( NULL );
    }

    p_font = malloc( sizeof( subtitler_font_t ) );

    if( p_font == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        close( i_file );
        return NULL;
    }

    /* Read font height */
    if( read( i_file, pi_buffer, 1 ) != 1 )
    {
        msg_Err( p_vout, "unexpected end of font file '%s'", psz_name );
        free( p_font ); 
        close( i_file );
        return( NULL );
    }
    p_font->i_height = pi_buffer[0];

    /* Initialise font character data */
    for( i = 0; i < 256; i++ )
    {
        p_font->i_width[i] = 0;
        p_font->i_memory[i] = 0;
        p_font->p_offset[i] = NULL;
        p_font->p_length[i] = NULL;
    }

    while(1)
    {
        /* Read character number */
        if( read( i_file, pi_buffer, 1 ) != 1)
        {
            msg_Err( p_vout, "unexpected end of font file '%s'", psz_name );
            close( i_file );
            E_(subtitler_UnloadFont)( p_vout, p_font );
            return( NULL );
        }
        i_char = pi_buffer[0];

        /* Character 255 signals the end of the font file */
        if(i_char == 255)
        {
            break;
        }

        /* Read character width */
        if( read( i_file, pi_buffer, 1 ) != 1 )
        {
            msg_Err( p_vout, "unexpected end of font file '%s'", psz_name );
            close( i_file );
            E_(subtitler_UnloadFont)( p_vout, p_font );
            return( NULL );
        }
        p_font->i_width[ i_char ] = pi_buffer[0];

        p_font->p_length[ i_char ] = (int *) malloc(
                                     sizeof(int) * p_font->i_height );
        p_font->p_offset[ i_char ] = (uint16_t **) malloc(
                                     sizeof(uint16_t *) * p_font->i_height);

        if( p_font->p_length[ i_char] == NULL ||
            p_font->p_offset[ i_char ] == NULL )
        {
            msg_Err( p_vout, "out of memory" );
            close( i_file );
            E_(subtitler_UnloadFont)( p_vout, p_font );
            return NULL;
        }
        for( i_line=0; i_line < p_font->i_height; i_line ++ )
        {
            p_font->p_offset[ i_char ][ i_line ] = NULL;
        }

        i_total_length=0;
        for( i_line = 0; i_line < p_font->i_height; i_line ++ )
        {
            /* Read line length */
            if( read( i_file, pi_buffer, 1 ) != 1)
            {
                msg_Err( p_vout, "unexpected end of font file '%s'", psz_name);
                E_(subtitler_UnloadFont)( p_vout, p_font );
                close( i_file );
                return( NULL );
            }
            i_length = pi_buffer[0];
            p_font->p_length[ i_char ][ i_line ] = i_length;

            i_total_length += i_length;

            /* Read line RLE data */
            if( read( i_file, pi_buffer, i_length*2 ) != i_length*2)
            {
                msg_Err( p_vout, "unexpected end of font file '%s'", psz_name);
                E_(subtitler_UnloadFont)( p_vout, p_font );
                close( i_file );
                return( NULL );
            }
            p_font->p_offset[ i_char ][ i_line ] =
                (uint16_t *) malloc( sizeof( uint16_t ) * i_length );
            if( p_font->p_offset[ i_char ][ i_line ] == NULL )
            {
                msg_Err( p_vout, "out of memory" );
                close( i_file );
                E_(subtitler_UnloadFont)( p_vout, p_font );
                return NULL;
            }
            for( i = 0; i < i_length; i++ )
            {
                *( p_font->p_offset[ i_char ][ i_line ] + i ) =
                (uint16_t) ( pi_buffer[ i * 2 ] +
                             ( pi_buffer[ i * 2 + 1 ] << 2 ) );
            }

        }

       /* Set total memory size of character */
       p_font->i_memory[ i_char ] = i_total_length;
        
    }

    close(i_file);

    return p_font;
}

/*****************************************************************************
 * subtitler_UnloadFont: unload a run-length encoded font file from memory
 *****************************************************************************/
void E_(subtitler_UnloadFont)( vout_thread_t * p_vout, 
                               subtitler_font_t * p_font )
{
    int i_char;
    int i_line;

    msg_Dbg( p_vout, "unloading font" );

    if( p_font == NULL )
    {
        return;
    }

    for( i_char = 0; i_char < 256; i_char ++ )
    {
        if( p_font->p_offset[ i_char ] != NULL )
        {
            for( i_line = 0; i_line < p_font->i_height; i_line++ )
            {
                if( p_font->p_offset[ i_char ][ i_line ] != NULL )
                {
                    free( p_font->p_offset[ i_char ][ i_line ] );
                }
            }
            free( p_font->p_offset[ i_char ] );
        }
        if( p_font->p_length[ i_char ] != NULL )
        {
            free( p_font->p_length[ i_char ] );
        }
    }

    free( p_font );
}

/*****************************************************************************
 * subtitler_PlotSubtitle: create a subpicture containing the subtitle
 *****************************************************************************/
void subtitler_PlotSubtitle ( vout_thread_t *p_vout , char *psz_subtitle,
                              subtitler_font_t *p_font, mtime_t i_start,
                              mtime_t i_stop )
{
    subpicture_t     * p_spu;

    int                i_x;
    int                i_width;
    int                i_lines;
    int                i_longest_width;
    int                i_total_length;
    int                i_char;

    uint16_t         * p_data;

    char             * p_line_start;
    char             * p_word_start;
    char             * p_char;

    subtitler_line_t * p_first_line;
    subtitler_line_t * p_previous_line;
    subtitler_line_t * p_line;

    if( p_font == NULL )
    {
        msg_Err( p_vout, "attempt to use NULL font in subtitle" );
        return;
    }

    p_first_line = NULL;
    p_previous_line = NULL;

    p_line_start = psz_subtitle;

    while( *p_line_start != 0 )
    {
        i_width = 0;
        p_word_start = p_line_start;
        p_char = p_line_start;

        while( *p_char != '\n' && *p_char != 0 )
        {
            i_width += p_font->i_width[ toascii( *p_char ) ]; 

            if( i_width > p_vout->output.i_width )
            {
                /* If the line has more than one word, break at the end of
                   the previous one. If the line is one very long word,
                   display as much as we can of it */
                if( p_word_start != p_line_start )
                {
                    p_char=p_word_start;
                }
                break;
            }

            if( *p_char == ' ' )
            {
                p_word_start = p_char+1;
            }

            p_char++;
        }

        p_line = malloc(sizeof(subtitler_line_t));

        if( p_line == NULL )
        {
            msg_Err( p_vout, "out of memory" );
            return;
        }

        if( p_first_line == NULL )
        {
            p_first_line = p_line;
        }

        if( p_previous_line != NULL )
        {
            p_previous_line->p_next = p_line;
        }

        p_previous_line = p_line;

        p_line->p_next = NULL;

        p_line->p_text = malloc(( p_char - p_line_start ) +1 );

        if( p_line == NULL )
        {
            msg_Err( p_vout, "out of memory" );
            return;
        }

        /* Copy only the part of the text that is in this line */
        strncpy( p_line->p_text , p_line_start , p_char - p_line_start );
        *( p_line->p_text + ( p_char - p_line_start )) = 0;

        /* If we had to break a line because it was too long, ensure that
           no characters are lost */
        if( *p_char != '\n' && *p_char != 0 )
        {
            p_char--; 
        }

        p_line_start = p_char;
        if( *p_line_start != 0 )
        {
            p_line_start ++;
        }
    }

    i_lines = 0;
    i_longest_width = 0;
    i_total_length = 0;
    p_line = p_first_line;

    /* Find the width of the longest line, count the total number of lines,
       and calculate the amount of memory we need to allocate for the RLE
       data */
    while( p_line != NULL )
    {
        i_lines++;
        i_width = 0;
        for( i_x = 0; i_x < strlen( p_line->p_text ); i_x++ )
        { 
            i_char = toascii(*(( p_line->p_text )+ i_x ));
            i_width += p_font->i_width[ i_char ];
            i_total_length += p_font->i_memory[ i_char ];
        }
        if(i_width > i_longest_width)
        {
            i_longest_width = i_width;
        }
        p_line = p_line->p_next;
    }

    /* Allow space for the padding bytes at either edge */
    i_total_length += p_font->i_height * 2 * i_lines;

    /* Allocate the subpicture internal data. */
    p_spu = vout_CreateSubPicture( p_vout, MEMORY_SUBPICTURE );
    if( p_spu == NULL )
    {
        return;
    }

    /* Rationale for the "p_spudec->i_rle_size * 4": we are going to
     * expand the RLE stuff so that we won't need to read nibbles later
     * on. This will speed things up a lot. Plus, we'll only need to do
     * this stupid interlacing stuff once. */
    p_spu->p_sys = malloc( sizeof( subpicture_sys_t )
                            + i_total_length * sizeof(uint16_t) );
    if( p_spu->p_sys == NULL )
    {
        vout_DestroySubPicture( p_vout, p_spu );
        return;
    }

    /* Fill the p_spu structure */
    p_spu->pf_render = E_(RenderSPU);
    p_spu->pf_destroy = DestroySPU;
    p_spu->p_sys->p_data = (uint8_t *)p_spu->p_sys + sizeof(subpicture_sys_t);

    p_spu->i_start = i_start;
    p_spu->i_stop = i_stop;
 
    p_spu->b_ephemer = i_stop ? VLC_FALSE : VLC_TRUE;

    /* FIXME: Do we need these two? */
    p_spu->p_sys->pi_offset[0] = 0;
    p_spu->p_sys->pi_offset[1] = 0;

    p_spu->p_sys->b_palette = 1;

    /* Colour 0 is transparent */
    p_spu->p_sys->pi_yuv[0][0] = 0xff;
    p_spu->p_sys->pi_yuv[0][1] = 0x80;
    p_spu->p_sys->pi_yuv[0][2] = 0x80;
    p_spu->p_sys->pi_yuv[0][3] = 0x80;
    p_spu->p_sys->pi_alpha[0] = 0x0;

    /* Colour 1 is grey */
    p_spu->p_sys->pi_yuv[1][0] = 0x80;
    p_spu->p_sys->pi_yuv[1][1] = 0x80;
    p_spu->p_sys->pi_yuv[1][2] = 0x80;
    p_spu->p_sys->pi_yuv[1][3] = 0x80;
    p_spu->p_sys->pi_alpha[1] = 0xf;

    /* Colour 2 is white */
    p_spu->p_sys->pi_yuv[2][0] = 0xff;
    p_spu->p_sys->pi_yuv[2][1] = 0xff;
    p_spu->p_sys->pi_yuv[2][2] = 0xff;
    p_spu->p_sys->pi_yuv[2][3] = 0xff;
    p_spu->p_sys->pi_alpha[2] = 0xf;

    /* Colour 3 is black */
    p_spu->p_sys->pi_yuv[3][0] = 0x00;
    p_spu->p_sys->pi_yuv[3][1] = 0x00;
    p_spu->p_sys->pi_yuv[3][2] = 0x00;
    p_spu->p_sys->pi_yuv[3][3] = 0x00;
    p_spu->p_sys->pi_alpha[3] = 0xf;

    p_spu->p_sys->b_crop = VLC_FALSE;

    p_spu->i_x = (p_vout->output.i_width - i_longest_width) / 2;
    p_spu->i_y = p_vout->output.i_height - (p_font->i_height * i_lines); 
    p_spu->i_width = i_longest_width;
    p_spu->i_height = p_font->i_height*i_lines;

    p_data = (uint16_t *)(p_spu->p_sys->p_data);

    p_line = p_first_line;
    while( p_line != NULL )
    {
        p_data = PlotSubtitleLine( p_line->p_text,
                                   p_font, i_longest_width, p_data );
        p_previous_line = p_line;
        p_line = p_line->p_next;
        free( p_previous_line->p_text );
        free( p_previous_line );
    }

    /* SPU is finished - we can ask the video output to display it */
    vout_DisplaySubPicture( p_vout, p_spu );

}

/*****************************************************************************
 * PlotSubtitleLine: plot a single line of a subtitle
 *****************************************************************************/
static uint16_t * PlotSubtitleLine ( char *psz_line, subtitler_font_t *p_font,
                                     int i_total_width, uint16_t *p_data )
{
    int   i_x;
    int   i_y;
    int   i_length;
    int   i_line_width;
    int   i_char;

    uint16_t * p_rle;

    i_line_width = 0;
    for( i_x = 0; i_x< strlen( psz_line ); i_x++ )
    {
        i_line_width += p_font->i_width[ toascii( *( psz_line+i_x ) ) ]; }

    for( i_y = 0; i_y < p_font->i_height; i_y++ )
    {
        /* Pad line to fit box */
        if( i_line_width < i_total_width )
        {
            *p_data++ = ((( i_total_width - i_line_width)/2) << 2 );
        }

        for(i_x = 0; i_x < strlen(psz_line); i_x ++)
        {
            i_char = toascii( *(psz_line + i_x) );

            if( p_font->i_width[ i_char ] != 0 )
            {
                p_rle = p_font->p_offset[ i_char ][ i_y ];
                i_length = p_font->p_length[ i_char ][ i_y ];

                if(p_rle != NULL )
                {
                    memcpy(p_data, p_rle, i_length * sizeof(uint16_t) ); 
                    p_data+=i_length;
                }
            }
        }
    
        /* Pad line to fit box */
        if( i_line_width < i_total_width )
        {
            *p_data++ = ((( i_total_width - i_line_width)
                        - (( i_total_width - i_line_width)/2)) << 2 );
        }
    }

    return p_data;
}

/*****************************************************************************
 * DestroySPU: subpicture destructor
 *****************************************************************************/
static void DestroySPU( subpicture_t *p_spu )
{
    free( p_spu->p_sys );
}
