/*****************************************************************************
 * video_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video_text.c,v 1.35 2002/06/01 12:32:02 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                  /* errno */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <fcntl.h>                                                 /* open() */

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                    /* read(), close() */
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#if defined( WIN32 )
#   include <io.h>
#endif

#include "video.h"
#include "video_output.h"
#include "video_text.h"

/*****************************************************************************
 * vout_font_t: bitmap font
 *****************************************************************************
 * This structure is used when the system doesn't provide a convenient function
 * to print simple characters in a buffer.
 * VOUT_FIXED_FONTs are stored in raw mode, character after character, with a
 * first array of characters followed by a second array of borders masks.
 * Therefore the border masks can't be complete if the font has pixels on the
 * border.
 *****************************************************************************/
struct vout_font_s
{
    int                 i_type;                                 /* font type */
    int                 i_width;                /* character width in pixels */
    int                 i_height;              /* character height in pixels */
    int                 i_interspacing; /* characters interspacing in pixels */
    int                 i_bytes_per_line;        /* bytes per character line */
    int                 i_bytes_per_char;             /* bytes per character */
    u16                 i_first;                          /* first character */
    u16                 i_last;                            /* last character */
    byte_t *            p_data;                       /* font character data */
};

/* Font types */
#define VOUT_FIXED_FONT       0                         /* simple fixed font */

/*****************************************************************************
 * vout_put_byte_t: PutByte function
 *****************************************************************************
 * These functions will transform masks in a set of pixels. For each pixel,
 * character, then border and background masks are tested, and the first
 * encountered color is set.
 *****************************************************************************/
typedef void (vout_put_byte_t)( void *p_pic, int i_byte, int i_char, int i_border,
                                int i_bg, u32 i_char_color, u32 i_border_color, u32 i_bg_color );


/*****************************************************************************
 * Macros
 *****************************************************************************/

/* PUT_BYTE_MASK: put pixels from a byte-wide mask. It uses a branching tree
 * to optimize the number of tests. It is used in the PutByte functions.
 * This macro works for 1, 2 and 4 Bpp. */
#define PUT_BYTE_MASK( i_mask, i_mask_color )                                 \
if( i_mask & 0xf0 )                                       /* one from 1111 */ \
{                                                                             \
    if( i_mask & 0xc0 )                                   /* one from 1100 */ \
    {                                                                         \
        if( i_mask & 0x80 )                                        /* 1000 */ \
        {                                                                     \
            p_pic[0] = i_mask_color;                                          \
            if( i_mask & 0x40 )                                    /* 0100 */ \
            {                                                                 \
                p_pic[1] = i_mask_color;                                      \
            }                                                                 \
        }                                                                     \
        else                                        /* not 1000 means 0100 */ \
        {                                                                     \
            p_pic[1] = i_mask_color;                                          \
        }                                                                     \
        if( i_mask & 0x30 )                               /* one from 0011 */ \
        {                                                                     \
            if( i_mask & 0x20 )                                    /* 0010 */ \
            {                                                                 \
                p_pic[2] = i_mask_color;                                      \
                if( i_mask & 0x10 )                                /* 0001 */ \
                {                                                             \
                    p_pic[3] = i_mask_color;                                  \
                }                                                             \
            }                                                                 \
            else                                    /* not 0010 means 0001 */ \
            {                                                                 \
                 p_pic[3] = i_mask_color;                                     \
            }                                                                 \
        }                                                                     \
    }                                                                         \
    else                                            /* not 1100 means 0011 */ \
    {                                                                         \
        if( i_mask & 0x20 )                                        /* 0010 */ \
        {                                                                     \
            p_pic[2] = i_mask_color;                                          \
            if( i_mask & 0x10 )                                    /* 0001 */ \
            {                                                                 \
                p_pic[3] = i_mask_color;                                      \
            }                                                                 \
        }                                                                     \
        else                                        /* not 0010 means 0001 */ \
        {                                                                     \
            p_pic[3] = i_mask_color;                                          \
        }                                                                     \
    }                                                                         \
}                                                                             \
if( i_mask & 0x0f )                                                           \
{                                                                             \
    if( i_mask & 0x0c )                       /* one from 1100 */             \
    {                                                                         \
        if( i_mask & 0x08 )                                        /* 1000 */ \
        {                                                                     \
            p_pic[4] = i_mask_color;                                          \
            if( i_mask & 0x04 )                                    /* 0100 */ \
            {                                                                 \
                p_pic[5] = i_mask_color;                                      \
            }                                                                 \
        }                                                                     \
        else                                        /* not 1000 means 0100 */ \
        {                                                                     \
            p_pic[5] = i_mask_color;                                          \
        }                                                                     \
        if( i_mask & 0x03 )                               /* one from 0011 */ \
        {                                                                     \
            if( i_mask & 0x02 )                                    /* 0010 */ \
            {                                                                 \
                p_pic[6] = i_mask_color;                                      \
                if( i_mask & 0x01 )                                /* 0001 */ \
                {                                                             \
                    p_pic[7] = i_mask_color;                                  \
                }                                                             \
            }                                                                 \
            else                                    /* not 0010 means 0001 */ \
            {                                                                 \
                 p_pic[7] = i_mask_color;                                     \
            }                                                                 \
        }                                                                     \
    }                                                                         \
    else                                            /* not 1100 means 0011 */ \
    {                                                                         \
        if( i_mask & 0x02 )                                        /* 0010 */ \
        {                                                                     \
            p_pic[6] = i_mask_color;                                          \
            if( i_mask & 0x01 )                                    /* 0001 */ \
            {                                                                 \
                p_pic[7] = i_mask_color;                                      \
            }                                                                 \
        }                                                                     \
        else                                        /* not 0010 means 0001 */ \
        {                                                                     \
            p_pic[7] = i_mask_color;                                          \
        }                                                                     \
    }                                                                         \
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void PutByte8 ( u8 *p_pic, int i_byte, int i_char, int i_border,
                       int i_bg, u32 i_char_color, u32 i_border_color,
                       u32 i_bg_color );
static void PutByte16( u16 *p_pic, int i_byte, int i_char, int i_border,
                       int i_bg, u32 i_char_color, u32 i_border_color,
                       u32 i_bg_color );
static void PutByte24( void *p_pic, int i_byte, byte_t i_char, byte_t i_border,
                       byte_t i_bg, u32 i_char_color, u32 i_border_color,
                       u32 i_bg_color );
static void PutByte32( u32 *p_pic, int i_byte, byte_t i_char, byte_t i_border,
                       byte_t i_bg, u32 i_char_color, u32 i_border_color,
                       u32 i_bg_color );

/*****************************************************************************
 * vout_LoadFont: load a bitmap font from a file
 *****************************************************************************
 * This function will try to open a .psf font and load it. It will return
 * NULL on error.
 *****************************************************************************/
vout_font_t *vout_LoadFont( vout_thread_t *p_vout, const char *psz_name )
{
    static char * path[] = { "share", DATA_PATH, NULL, NULL };

    char **             ppsz_path = path;
    char *              psz_file;
#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
    char *              psz_vlcpath = system_GetProgramPath();
    int                 i_vlclen = strlen( psz_vlcpath );
#endif
    int                 i_char, i_line;        /* character and line indexes */
    int                 i_file = -1;                          /* source file */
    byte_t              pi_buffer[2];                         /* file buffer */
    vout_font_t *       p_font;                           /* the font itself */

    for( ; *ppsz_path != NULL ; ppsz_path++ )
    {
#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
        /* Under BeOS, we need to add beos_GetProgramPath() to access
         * files under the current directory */
        if( strncmp( *ppsz_path, "/", 1 ) )
        {
            psz_file = malloc( strlen( psz_name ) + strlen( *ppsz_path )
                                + i_vlclen + 3 );
            if( psz_file == NULL )
            {
                continue;
            }
            sprintf( psz_file, "%s/%s/%s", psz_vlcpath, *ppsz_path, psz_name );
        }
        else
#endif
        {
            psz_file = malloc( strlen( psz_name ) + strlen( *ppsz_path ) + 2 );
            if( psz_file == NULL )
            {
                continue;
            }
            sprintf( psz_file, "%s/%s", *ppsz_path, psz_name );
        }

        /* Open file */
        i_file = open( psz_file, O_RDONLY );
        free( psz_file );

        if( i_file != -1 )
        {
            break;
        }
    }

    if( i_file == -1 )
    {
        msg_Err( p_vout, "cannot open '%s' (%s)", psz_name, strerror(errno) );
        return( NULL );
    }

    /* Read magic number */
    if( read( i_file, pi_buffer, 2 ) != 2 )
    {
        msg_Err( p_vout, "unexpected end of file in '%s'", psz_name );
        close( i_file );
        return( NULL );
    }

    /* Allocate font descriptor */
    p_font = malloc( sizeof( vout_font_t ) );
    if( p_font == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        close( i_file );
        return( NULL );
    }

    /* Read file */
    switch( ((u16)pi_buffer[0] << 8) | pi_buffer[1] )
    {
    case 0x3604:                                              /* .psf file */
        /*
         * PSF font: simple fixed font. Only the first 256 characters are read.
         * Those fonts are always 1 byte wide, and 256 or 512 characters long.
         */

        /* Read font header - two bytes indicate the font properties */
        if( read( i_file, pi_buffer, 2 ) != 2)
        {
            msg_Err( p_vout, "unexpected end of file in '%s'", psz_name );
            free( p_font );
            close( i_file );
            return( NULL );
        }

        /* Copy font properties */
        p_font->i_type =                VOUT_FIXED_FONT;
        p_font->i_width =               8;
        p_font->i_height =              pi_buffer[1];
        p_font->i_interspacing =        8;
        p_font->i_bytes_per_line =      1;
        p_font->i_bytes_per_char =      pi_buffer[1];
        p_font->i_first =               0;
        p_font->i_last =                255;

        /* Allocate font space */
        p_font->p_data = malloc( 2 * 256 * pi_buffer[1] );
        if( p_font->p_data == NULL )
        {
            msg_Err( p_vout, "out of memory" );
            free( p_font );
            close( i_file );
            return( NULL );
        }

        /* Copy raw data */
        if( read( i_file, p_font->p_data, 256 * pi_buffer[1] ) != 256 * pi_buffer[1] )
        {
            msg_Err( p_vout, "unexpected end of file in '%s'", psz_name );
            free( p_font->p_data );
            free( p_font );
            close( i_file );
            return( NULL );
        }

        /* Compute border masks - remember that masks have the same matrix as
         * characters, so an empty character border is required to have a
         * complete border mask. */
        for( i_char = 0; i_char <= 255; i_char++ )
        {
            for( i_line = 0; i_line < pi_buffer[1]; i_line++ )
            {

                p_font->p_data[ (i_char + 256) * pi_buffer[1] + i_line ] =
                    ((p_font->p_data[ i_char * pi_buffer[1] + i_line ] << 1) |
                     (p_font->p_data[ i_char * pi_buffer[1] + i_line ] >> 1) |
                     (i_line > 0 ? p_font->p_data[ i_char * pi_buffer[1] + i_line - 1]: 0) |
                     (i_line < pi_buffer[1] - 1 ? p_font->p_data[ i_char * pi_buffer[1] + i_line + 1]: 0))
                    & ~p_font->p_data[ i_char * pi_buffer[1] + i_line ];
            }
        }

        break;
    default:
        msg_Err( p_vout, "file '%s' has an unknown format", psz_name );
        free( p_font );
        close( i_file );
        return( NULL );
        break;
    }

    msg_Err( p_vout, "loaded %s, type %d, %d-%dx%d", psz_name, p_font->i_type,
             p_font->i_width, p_font->i_interspacing, p_font->i_height );

    return( p_font );
}

/*****************************************************************************
 * vout_UnloadFont: unload a font
 *****************************************************************************
 * This function free the resources allocated by vout_LoadFont
 *****************************************************************************/
void vout_UnloadFont( vout_font_t *p_font )
{
    /* If no font was loaded, do nothing */
    if( p_font == NULL )
    {
        return;
    }

    free( p_font->p_data );
    free( p_font );
}

/*****************************************************************************
 * vout_TextSize: return the dimensions of a text
 *****************************************************************************
 * This function is used to align text. It returns the width and height of a
 * given text.
 *****************************************************************************/
void vout_TextSize( vout_font_t *p_font, int i_style, const char *psz_text, int *pi_width, int *pi_height )
{
    /* If no font was loaded, do nothing */
    if( p_font == NULL )
    {
        *pi_width = *pi_height = 0;
        return;
    }

    switch( p_font->i_type )
    {
    case VOUT_FIXED_FONT:
        *pi_width  = ((i_style & WIDE_TEXT) ? p_font->i_interspacing * 2 : p_font->i_interspacing) *
            (strlen( psz_text ) - 1) + p_font->i_width;
        *pi_height = p_font->i_height;
        if( i_style & ITALIC_TEXT )
        {
            *pi_width = *pi_height / 3;
        }
        break;
    }
}

/*****************************************************************************
 * vout_Print: low level printing function
 *****************************************************************************
 * This function prints a text, without clipping, in a buffer using a
 * previously loaded bitmap font.
 *****************************************************************************/
void vout_Print( vout_font_t *p_font, byte_t *p_pic, int i_bytes_per_pixel, int i_bytes_per_line,
                 u32 i_char_color, u32 i_border_color, u32 i_bg_color, int i_style, const char *psz_text, int i_percent)
{
    byte_t      *p_char, *p_border;        /* character and border mask data */
    int         i_char_mask, i_border_mask, i_bg_mask;              /* masks */
    int         i_line;                         /* current line in character */
    int         i_byte;                         /* current byte in character */
    int         i_interspacing;                  /* offset between two chars */
    int         i_font_bytes_per_line, i_font_height;     /* font properties */
    int         i_position, i_end;                      /* current position  */
    vout_put_byte_t *p_PutByte;                          /* PutByte function */

    /* If no font was loaded, do nothing */
    if( p_font == NULL )
    {
        return;
    }

    /* FIXME: background: can be something else that whole byte ?? */

    /* Select output function */
    switch( i_bytes_per_pixel )
    {
    case 1:
        p_PutByte = (vout_put_byte_t *) PutByte8;
        break;
    case 2:
        p_PutByte = (vout_put_byte_t *) PutByte16;
        break;
    case 3:
        p_PutByte = (vout_put_byte_t *) PutByte24;
        break;
    case 4:
    default:
        p_PutByte = (vout_put_byte_t *) PutByte32;
        break;
    }

    /* Choose masks and copy font data to local variables */
    i_char_mask =               (i_style & VOID_TEXT) ?         0 : 0xff;
    i_border_mask =             (i_style & OUTLINED_TEXT) ?     0xff : 0;
    i_bg_mask =                 (i_style & OPAQUE_TEXT) ?       0xff : 0;

    i_font_bytes_per_line =     p_font->i_bytes_per_line;
    i_font_height =             p_font->i_height;
    i_interspacing =            i_bytes_per_pixel * ((i_style & WIDE_TEXT) ?
                                                     p_font->i_interspacing * 2 :
                                                     p_font->i_interspacing);

    /* compute where to stop... */
    i_end = (int) (i_percent * strlen(psz_text) / I64C(100));
    if(i_end > strlen(psz_text))
        i_end = strlen(psz_text);
    
    
    /* Print text */
    for( i_position = 0; i_position < i_end; i_position++ ,psz_text++ )
    {
        /* Check that the character is valid */
        if( (*psz_text >= p_font->i_first) && (*psz_text <= p_font->i_last) )
        {
            /* Select character - bytes per char is always valid, event for
             * non fixed fonts */
            p_char =    p_font->p_data + (*psz_text - p_font->i_first) * p_font->i_bytes_per_char;
            p_border =  p_char + (p_font->i_last - p_font->i_first + 1) * p_font->i_bytes_per_char;

            /* Select base address for output */
            switch( p_font->i_type )
            {
            case VOUT_FIXED_FONT:
                /*
                 * Simple fixed width font
                 */

                /* Italic text: shift picture start right */
                if( i_style & ITALIC_TEXT )
                {
                    p_pic += i_bytes_per_pixel * (p_font->i_height / 3);
                }

                /* Print character */
                for( i_line = 0; i_line < i_font_height; i_line ++ )
                {
                    for( i_byte = 0; i_byte < i_font_bytes_per_line; i_byte++, p_char++, p_border++)
                    {
                        /* Put pixels */
                        p_PutByte( p_pic + i_bytes_per_line * i_line, i_byte,
                                   *p_char & i_char_mask, *p_border & i_border_mask, i_bg_mask,
                                   i_char_color, i_border_color, i_bg_color );
                    }

                    /* Italic text: shift picture start left */
                    if( (i_style & ITALIC_TEXT) && !(i_line % 3) )
                    {
                        p_pic -= i_bytes_per_pixel;
                    }
                }

                /* Jump to next character */
                p_pic += i_interspacing;
                break;
            }
        }
    }
}

/* following functions are local */

/*****************************************************************************
 * PutByte8: print a fixed width font character byte in 1 Bpp
 *****************************************************************************/
static void PutByte8( u8 *p_pic, int i_byte, int i_char, int i_border,
                       int i_bg, u32 i_char_color, u32 i_border_color,
                       u32 i_bg_color )
{
    /* Computes position offset and background mask */
    p_pic += 8 * i_byte;
    i_bg &= ~(i_char | i_border);

    /* Put character bits */
    PUT_BYTE_MASK(i_char, i_char_color);
    PUT_BYTE_MASK(i_border, i_border_color);
    PUT_BYTE_MASK(i_bg, i_bg_color);
}

/*****************************************************************************
 * PutByte16: print a fixed width font character byte in 2 Bpp
 *****************************************************************************/
static void PutByte16( u16 *p_pic, int i_byte, int i_char, int i_border,
                       int i_bg, u32 i_char_color, u32 i_border_color,
                       u32 i_bg_color )
{
    /* Computes position offset and background mask */
    p_pic += 8 * i_byte;
    i_bg &= ~(i_char | i_border);

    /* Put character bits */
    PUT_BYTE_MASK(i_char, i_char_color);
    PUT_BYTE_MASK(i_border, i_border_color);
    PUT_BYTE_MASK(i_bg, i_bg_color);
}

/*****************************************************************************
 * PutByte24: print a fixed width font character byte in 3 Bpp
 *****************************************************************************/
static void PutByte24( void *p_pic, int i_byte, byte_t i_char, byte_t i_border, byte_t i_bg,
                       u32 i_char_color, u32 i_border_color, u32 i_bg_color )
{
    /* XXX?? */
}

/*****************************************************************************
 * PutByte32: print a fixed width font character byte in 4 Bpp
 *****************************************************************************/
static void PutByte32( u32 *p_pic, int i_byte, byte_t i_char, byte_t i_border, byte_t i_bg,
                       u32 i_char_color, u32 i_border_color, u32 i_bg_color )
{
    /* Computes position offset and background mask */
    p_pic += 8 * i_byte;
    i_bg &= ~(i_char | i_border);

    /* Put character bits */
    PUT_BYTE_MASK(i_char, i_char_color);
    PUT_BYTE_MASK(i_border, i_border_color);
    PUT_BYTE_MASK(i_bg, i_bg_color);
}

