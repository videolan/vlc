/*****************************************************************************
 * video_text.c : text manipulation functions
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "video_text.h"
#include "intf_msg.h"

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
typedef struct vout_font_s
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
} vout_font_t;

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
vout_font_t *vout_LoadFont( const char *psz_name )
{
    int                 i_char, i_line;        /* character and line indexes */
    int                 i_file;                               /* source file */
    byte_t              pi_buffer[2];                         /* file buffer */
    vout_font_t *       p_font;                           /* the font itself */

    /* Open file */
    i_file = open( psz_name, O_RDONLY );
    if( i_file == -1 )
    {
        intf_ErrMsg("error: can't open file '%s' (%s)\n", psz_name, strerror(errno));
        return( NULL );
    }

    /* Read magick number */
    if( read( i_file, pi_buffer, 2 ) != 2 )
    {
        intf_ErrMsg("error: unexpected end of file '%s'\n", psz_name );
        close( i_file );
        return( NULL );
    }

    /* Allocate font descriptor */
    p_font = malloc( sizeof( vout_font_t ) );
    if( p_font == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        close( i_file );
        return( NULL );
    }

    /* Read file */
    switch( ((u16)pi_buffer[0] << 8) | pi_buffer[1] )
    {
    case 0x3604:                                              /* .psf file */
        /*
         * PSF font: simple fixed font. Only the first 256 characters are read.
         * Those fonts are always 1 byte width, and 256 or 512 characters long.
         */

        /* Read font header - two bytes indicate the font properties */
        if( read( i_file, pi_buffer, 2 ) != 2)
        {
            intf_ErrMsg("error: unexpected end of file '%s'\n", psz_name );
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
            intf_ErrMsg("error: %s\n", strerror(ENOMEM));
            free( p_font );
            close( i_file );
            return( NULL );
        }

        /* Copy raw data */
        if( read( i_file, p_font->p_data, 256 * pi_buffer[1] ) != 256 * pi_buffer[1] )
        {
            intf_ErrMsg("error: unexpected end of file '%s'\n", psz_name );
            free( p_font->p_data );
            free( p_font );
            close( i_file );
            return( NULL );
        }

        /* Computes border masks - remember that masks have the same matrix as
         * characters, so an empty character border is required to have a complete
         * border mask. */
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
        intf_ErrMsg("error: file '%s' has an unknown format\n", psz_name );
        free( p_font );
        close( i_file );
        return( NULL );
        break;
    }


    intf_DbgMsg( "loaded %s: type %d, %d-%dx%d\n", psz_name, p_font->i_type,
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
    intf_DbgMsg( "font %p\n", p_font );
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
#ifdef DEBUG
    default:
        intf_DbgMsg("error: unknown font type %d\n", p_font->i_type );
        break;
#endif
    }
}

/*****************************************************************************
 * vout_Print: low level printing function
 *****************************************************************************
 * This function prints a text, without clipping, in a buffer using a
 * previously loaded bitmap font.
 *****************************************************************************/
void vout_Print( vout_font_t *p_font, byte_t *p_pic, int i_bytes_per_pixel, int i_bytes_per_line,
                 u32 i_char_color, u32 i_border_color, u32 i_bg_color, int i_style, const char *psz_text )
{
    byte_t      *p_char, *p_border;        /* character and border mask data */
    int         i_char_mask, i_border_mask, i_bg_mask;              /* masks */
    int         i_line;                         /* current line in character */
    int         i_byte;                         /* current byte in character */
    int         i_interspacing;                  /* offset between two chars */
    int         i_font_bytes_per_line, i_font_height;     /* font properties */
    vout_put_byte_t *p_PutByte;                          /* PutByte function */

    //?? background: can be something else that whole byte

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

    /* Print text */
    for( ; *psz_text != '\0'; psz_text++ )
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
#ifdef DEBUG
            default:
                intf_DbgMsg("error: unknown font type %d\n", p_font->i_type );
                break;
#endif
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
    //??
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

