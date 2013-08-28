/*****************************************************************************
 * cc.c : CC 608/708 subtitles decoder
 *****************************************************************************
 * Copyright © 2007-2010 Laurent Aimar, 2011 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar < fenrir # via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
/* The EIA 608 decoder part has been initialy based on ccextractor (GPL)
 * and rewritten */

/* TODO:
 *  On discontinuity reset the decoder state
 *  Check parity
 *  708 decoding
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_charset.h>

#include "substext.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("CC 608/708"))
    set_description( N_("Closed Captions decoder") )
    set_capability( "decoder", 50 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef enum
{
    EIA608_MODE_POPUP = 0,
    EIA608_MODE_ROLLUP_2 = 1,
    EIA608_MODE_ROLLUP_3 = 2,
    EIA608_MODE_ROLLUP_4 = 3,
    EIA608_MODE_PAINTON = 4,
    EIA608_MODE_TEXT = 5
} eia608_mode_t;

typedef enum
{
    EIA608_COLOR_WHITE = 0,
    EIA608_COLOR_GREEN = 1,
    EIA608_COLOR_BLUE = 2,
    EIA608_COLOR_CYAN = 3,
    EIA608_COLOR_RED = 4,
    EIA608_COLOR_YELLOW = 5,
    EIA608_COLOR_MAGENTA = 6,
    EIA608_COLOR_USERDEFINED = 7
} eia608_color_t;

typedef enum
{
    EIA608_FONT_REGULAR    = 0x00,
    EIA608_FONT_ITALICS    = 0x01,
    EIA608_FONT_UNDERLINE  = 0x02,
    EIA608_FONT_UNDERLINE_ITALICS = EIA608_FONT_UNDERLINE | EIA608_FONT_ITALICS
} eia608_font_t;

#define EIA608_SCREEN_ROWS 15
#define EIA608_SCREEN_COLUMNS 32

struct eia608_screen // A CC buffer
{
    uint8_t characters[EIA608_SCREEN_ROWS][EIA608_SCREEN_COLUMNS+1];
    eia608_color_t colors[EIA608_SCREEN_ROWS][EIA608_SCREEN_COLUMNS+1];
    eia608_font_t fonts[EIA608_SCREEN_ROWS][EIA608_SCREEN_COLUMNS+1]; // Extra char at the end for a 0
    int row_used[EIA608_SCREEN_ROWS]; // Any data in row?
};
typedef struct eia608_screen eia608_screen;

typedef struct
{
    /* Current channel (used to reject packet without channel information) */
    int i_channel;

    /* */
    int           i_screen; /* Displayed screen */
    eia608_screen screen[2];

    struct
    {
        int i_row;
        int i_column;
    } cursor;

    /* */
    eia608_mode_t mode;
    eia608_color_t color;
    eia608_font_t font;
    int i_row_rollup;

    /* Last command pair (used to reject duplicated command) */
    struct
    {
        uint8_t d1;
        uint8_t d2;
    } last;
} eia608_t;

static void         Eia608Init( eia608_t * );
static bool   Eia608Parse( eia608_t *h, int i_channel_selected, const uint8_t data[2] );
static char        *Eia608Text( eia608_t *h, bool b_html );

/* It will be enough up to 63 B frames, which is far too high for
 * broadcast environment */
#define CC_MAX_REORDER_SIZE (64)
struct decoder_sys_t
{
    int     i_block;
    block_t *pp_block[CC_MAX_REORDER_SIZE];

    int i_field;
    int i_channel;

    eia608_t eia608;
};

static subpicture_t *Decode( decoder_t *, block_t ** );

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    int i_field;
    int i_channel;

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_FOURCC('c','c','1',' '):
            i_field = 0; i_channel = 1;
            break;
        case VLC_FOURCC('c','c','2',' '):
            i_field = 0; i_channel = 2;
            break;
        case VLC_FOURCC('c','c','3',' '):
            i_field = 1; i_channel = 1;
            break;
        case VLC_FOURCC('c','c','4',' '):
            i_field = 1; i_channel = 2;
            break;

        default:
            return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = Decode;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* init of p_sys */
    p_sys->i_field = i_field;
    p_sys->i_channel = i_channel;

    Eia608Init( &p_sys->eia608 );

    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_TEXT;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************
 *
 ****************************************************************************/
static void     Push( decoder_t *, block_t * );
static block_t *Pop( decoder_t * );
static subpicture_t *Convert( decoder_t *, block_t * );

static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    if( pp_block && *pp_block )
    {
        Push( p_dec, *pp_block );
        *pp_block = NULL;
    }

    for( ;; )
    {
        block_t *p_block = Pop( p_dec );
        if( !p_block )
            break;

        subpicture_t *p_spu = Convert( p_dec, p_block );
        if( p_spu )
            return p_spu;
    }
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    for( int i = 0; i < p_sys->i_block; i++ )
        block_Release( p_sys->pp_block[i] );
    free( p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Push( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->i_block >= CC_MAX_REORDER_SIZE )
    {
        msg_Warn( p_dec, "Trashing a CC entry" );
        memmove( &p_sys->pp_block[0], &p_sys->pp_block[1], sizeof(*p_sys->pp_block) * (CC_MAX_REORDER_SIZE-1) );
        p_sys->i_block--;
    }
    p_sys->pp_block[p_sys->i_block++] = p_block;
}
static block_t *Pop( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    int i_index;
    /* XXX Cc captions data are OUT OF ORDER (because we receive them in the bitstream
     * order (ie ordered by video picture dts) instead of the display order.
     *  We will simulate a simple IPB buffer scheme
     * and reorder with pts.
     * XXX it won't work with H264 which use non out of order B picture or MMCO
     */

    /* Wait for a P and output all *previous* picture by pts order (for
     * hierarchical B frames) */
    if( p_sys->i_block <= 1 ||
        ( p_sys->pp_block[p_sys->i_block-1]->i_flags & BLOCK_FLAG_TYPE_B ) )
        return NULL;

    p_block = p_sys->pp_block[i_index = 0];
    if( p_block->i_pts > VLC_TS_INVALID )
    {
        for( int i = 1; i < p_sys->i_block-1; i++ )
        {
            if( p_sys->pp_block[i]->i_pts > VLC_TS_INVALID && p_block->i_pts > VLC_TS_INVALID &&
                p_sys->pp_block[i]->i_pts < p_block->i_pts )
                p_block = p_sys->pp_block[i_index = i];
        }
    }
    assert( i_index+1 < p_sys->i_block );
    memmove( &p_sys->pp_block[i_index], &p_sys->pp_block[i_index+1], sizeof(*p_sys->pp_block) * ( p_sys->i_block - i_index - 1 ) );
    p_sys->i_block--;

    return p_block;
}

static subpicture_t *Subtitle( decoder_t *p_dec, char *psz_subtitle, char *psz_html, mtime_t i_pts )
{
    //decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;

    /* We cannot display a subpicture with no date */
    if( i_pts <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        free( psz_subtitle );
        free( psz_html );
        return NULL;
    }

    EnsureUTF8( psz_subtitle );
    if( psz_html )
        EnsureUTF8( psz_html );

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        free( psz_subtitle );
        free( psz_html );
        return NULL;
    }
    p_spu->i_start    = i_pts;
    p_spu->i_stop     = i_pts + 10000000;   /* 10s max */
    p_spu->b_ephemer  = true;
    p_spu->b_absolute = false;

    subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    /* The "leavetext" alignment is a special mode where the subpicture
       region itself gets aligned, but the text inside it does not */
    p_spu_sys->align = SUBPICTURE_ALIGN_LEAVETEXT;
    p_spu_sys->text  = psz_subtitle;
    p_spu_sys->html  = psz_html;
    p_spu_sys->i_font_height_percent = 5;
    p_spu_sys->renderbg = true;

    return p_spu;
}

static subpicture_t *Convert( decoder_t *p_dec, block_t *p_block )
{
    assert( p_block );

    decoder_sys_t *p_sys = p_dec->p_sys;
    const int64_t i_pts = p_block->i_pts;
    bool b_changed = false;

    /* TODO do the real decoding here */
    while( p_block->i_buffer >= 3 )
    {
        if( p_block->p_buffer[0] == p_sys->i_field )
            b_changed |= Eia608Parse( &p_sys->eia608, p_sys->i_channel, &p_block->p_buffer[1] );

        p_block->i_buffer -= 3;
        p_block->p_buffer += 3;
    }
    if( p_block )
        block_Release( p_block );

    if( b_changed )
    {
        char *psz_subtitle = Eia608Text( &p_sys->eia608, false );
        char *psz_html = Eia608Text( &p_sys->eia608, true );
        return Subtitle( p_dec, psz_subtitle, psz_html, i_pts );
    }
    return NULL;
}


/*****************************************************************************
 *
 *****************************************************************************/
static const struct {
    eia608_color_t  i_color;
    eia608_font_t   i_font;
    int             i_column;
} pac2_attribs[]= {
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_GREEN,   EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_GREEN,   EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_BLUE,    EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_BLUE,    EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_CYAN,    EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_CYAN,    EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_RED,     EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_RED,     EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_YELLOW,  EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_YELLOW,  EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_MAGENTA, EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_MAGENTA, EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_ITALICS,           0 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE_ITALICS, 0 },

    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,           0 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,         0 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,           4 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,         4 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,           8 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,         8 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,          12 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,        12 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,          16 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,        16 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,          20 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,        20 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,          24 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,        24 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_REGULAR,          28 },
    { EIA608_COLOR_WHITE,   EIA608_FONT_UNDERLINE,        28 } ,
};

#define EIA608_COLOR_DEFAULT EIA608_COLOR_WHITE

static void Eia608Cursor( eia608_t *h, int dx )
{
    h->cursor.i_column += dx;
    if( h->cursor.i_column < 0 )
        h->cursor.i_column = 0;
    else if( h->cursor.i_column > EIA608_SCREEN_COLUMNS-1 )
        h->cursor.i_column = EIA608_SCREEN_COLUMNS-1;
}
static void Eia608ClearScreenRowX( eia608_t *h, int i_screen, int i_row, int x )
{
    eia608_screen *screen = &h->screen[i_screen];

    if( x == 0 )
    {
        screen->row_used[i_row] = false;
    }
    else
    {
        screen->row_used[i_row] = false;
        for( int i = 0; i < x; i++ )
        {
            if( screen->characters[i_row][i] != ' ' ||
                screen->colors[i_row][i] != EIA608_COLOR_DEFAULT ||
                screen->fonts[i_row][i] != EIA608_FONT_REGULAR )
            {
                screen->row_used[i_row] = true;
                break;
            }
        }
    }

    for( ; x < EIA608_SCREEN_COLUMNS+1; x++ )
    {
        screen->characters[i_row][x] = x < EIA608_SCREEN_COLUMNS ? ' ' : '\0';
        screen->colors[i_row][x] = EIA608_COLOR_DEFAULT;
        screen->fonts[i_row][x] = EIA608_FONT_REGULAR;
    }
}

static void Eia608ClearScreenRow( eia608_t *h, int i_screen, int i_row )
{
    Eia608ClearScreenRowX( h, i_screen, i_row, 0 );
}

static void Eia608ClearScreen( eia608_t *h, int i_screen )
{
    for( int i = 0; i < EIA608_SCREEN_ROWS; i++ )
        Eia608ClearScreenRow( h, i_screen, i );
}

static int Eia608GetWritingScreenIndex( eia608_t *h )
{
    switch( h->mode )
    {
    case EIA608_MODE_POPUP:    // Non displayed screen
        return 1 - h->i_screen;

    case EIA608_MODE_ROLLUP_2: // Displayed screen
    case EIA608_MODE_ROLLUP_3:
    case EIA608_MODE_ROLLUP_4:
    case EIA608_MODE_PAINTON:
        return h->i_screen;
    default:
        /* It cannot happen, else it is a bug */
        assert( 0 );
        return 0;
    }
}

static void Eia608EraseScreen( eia608_t *h, bool b_displayed )
{
    Eia608ClearScreen( h, b_displayed ? h->i_screen : (1-h->i_screen) );
}

static void Eia608Write( eia608_t *h, const uint8_t c )
{
    const int i_row = h->cursor.i_row;
    const int i_column = h->cursor.i_column;
    eia608_screen *screen;

    if( h->mode == EIA608_MODE_TEXT )
        return;

    screen = &h->screen[Eia608GetWritingScreenIndex( h )];

    screen->characters[i_row][i_column] = c;
    screen->colors[i_row][i_column] = h->color;
    screen->fonts[i_row][i_column] = h->font;
    screen->row_used[i_row] = true;
    Eia608Cursor( h, 1 );
}
static void Eia608Erase( eia608_t *h )
{
    const int i_row = h->cursor.i_row;
    const int i_column = h->cursor.i_column - 1;
    eia608_screen *screen;

    if( h->mode == EIA608_MODE_TEXT )
        return;
    if( i_column < 0 )
        return;

    screen = &h->screen[Eia608GetWritingScreenIndex( h )];

    /* FIXME do we need to reset row_used/colors/font ? */
    screen->characters[i_row][i_column] = ' ';
    Eia608Cursor( h, -1 );
}
static void Eia608EraseToEndOfRow( eia608_t *h )
{
    if( h->mode == EIA608_MODE_TEXT )
        return;

    Eia608ClearScreenRowX( h, Eia608GetWritingScreenIndex( h ), h->cursor.i_row, h->cursor.i_column );
}

static void Eia608RollUp( eia608_t *h )
{
    if( h->mode == EIA608_MODE_TEXT )
        return;

    const int i_screen = Eia608GetWritingScreenIndex( h );
    eia608_screen *screen = &h->screen[i_screen];

    int keep_lines;

    /* Window size */
    if( h->mode == EIA608_MODE_ROLLUP_2 )
        keep_lines = 2;
    else if( h->mode == EIA608_MODE_ROLLUP_3 )
        keep_lines = 3;
    else if( h->mode == EIA608_MODE_ROLLUP_4 )
        keep_lines = 4;
    else
        return;

    /* Reset the cursor */
    h->cursor.i_column = 0;

    /* Erase lines above our window */
    for( int i = 0; i < h->cursor.i_row - keep_lines; i++ )
        Eia608ClearScreenRow( h, i_screen, i );

    /* Move up */
    for( int i = 0; i < keep_lines-1; i++ )
    {
        const int i_row = h->cursor.i_row - keep_lines + i + 1;
        if( i_row < 0 )
            continue;
        assert( i_row+1 < EIA608_SCREEN_ROWS );
        memcpy( screen->characters[i_row], screen->characters[i_row+1], sizeof(*screen->characters) );
        memcpy( screen->colors[i_row], screen->colors[i_row+1], sizeof(*screen->colors) );
        memcpy( screen->fonts[i_row], screen->fonts[i_row+1], sizeof(*screen->fonts) );
        screen->row_used[i_row] = screen->row_used[i_row+1];
    }
    /* Reset current row */
    Eia608ClearScreenRow( h, i_screen, h->cursor.i_row );
}
static void Eia608ParseChannel( eia608_t *h, const uint8_t d[2] )
{
    /* Check odd parity */
    static const int p4[16] = {
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
    };
    if( p4[d[0] & 0xf] == p4[d[0] >> 4] ||
        p4[d[1] & 0xf] == p4[ d[1] >> 4] )
    {
        h->i_channel = -1;
        return;
    }

    /* */
    const int d1 = d[0] & 0x7f;
    if( d1 >= 0x10 && d1 <= 0x1f )
        h->i_channel = 1 + ((d1 & 0x08) != 0);
    else if( d1 < 0x10 )
        h->i_channel = 3;
}
static bool Eia608ParseTextAttribute( eia608_t *h, uint8_t d2 )
{
    const int i_index = d2 - 0x20;
    assert( d2 >= 0x20 && d2 <= 0x2f );

    h->color = pac2_attribs[i_index].i_color;
    h->font  = pac2_attribs[i_index].i_font;
    Eia608Cursor( h, 1 );

    return false;
}
static bool Eia608ParseSingle( eia608_t *h, const uint8_t dx )
{
    assert( dx >= 0x20 );
    Eia608Write( h, dx );
    return true;
}
static bool Eia608ParseDouble( eia608_t *h, uint8_t d2 )
{
    assert( d2 >= 0x30 && d2 <= 0x3f );
    Eia608Write( h, d2 + 0x50 ); /* We use charaters 0x80...0x8f */
    return true;
}
static bool Eia608ParseExtended( eia608_t *h, uint8_t d1, uint8_t d2 )
{
    assert( d2 >= 0x20 && d2 <= 0x3f );
    assert( d1 == 0x12 || d1 == 0x13 );
    if( d1 == 0x12 )
        d2 += 0x70; /* We use charaters 0x90-0xaf */
    else
        d2 += 0x90; /* We use charaters 0xb0-0xcf */

    /* The extended characters replace the previous one with a more
     * advanced one */
    Eia608Cursor( h, -1 );
    Eia608Write( h, d2 );
    return true;
}
static bool Eia608ParseCommand0x14( eia608_t *h, uint8_t d2 )
{
    bool b_changed = false;

    switch( d2 )
    {
    case 0x20:  /* Resume caption loading */
        h->mode = EIA608_MODE_POPUP;
        break;
    case 0x21:  /* Backspace */
        Eia608Erase( h );
        b_changed = true;
        break;
    case 0x22:  /* Reserved */
    case 0x23:
        break;
    case 0x24:  /* Delete to end of row */
        Eia608EraseToEndOfRow( h );
        break;
    case 0x25:  /* Rollup 2 */
    case 0x26:  /* Rollup 3 */
    case 0x27:  /* Rollup 4 */
        if( h->mode == EIA608_MODE_POPUP || h->mode == EIA608_MODE_PAINTON )
        {
            Eia608EraseScreen( h, true );
            Eia608EraseScreen( h, false );
            b_changed = true;
        }

        if( d2 == 0x25 )
            h->mode = EIA608_MODE_ROLLUP_2;
        else if( d2 == 0x26 )
            h->mode = EIA608_MODE_ROLLUP_3;
        else
            h->mode = EIA608_MODE_ROLLUP_4;

        h->cursor.i_column = 0;
        h->cursor.i_row = h->i_row_rollup;
        break;
    case 0x28:  /* Flash on */
        /* TODO */
        break;
    case 0x29:  /* Resume direct captionning */
        h->mode = EIA608_MODE_PAINTON;
        break;
    case 0x2a:  /* Text restart */
        /* TODO */
        break;

    case 0x2b: /* Resume text display */
        h->mode = EIA608_MODE_TEXT;
        break;

    case 0x2c: /* Erase displayed memory */
        Eia608EraseScreen( h, true );
        b_changed = true;
        break;
    case 0x2d: /* Carriage return */
        Eia608RollUp(h);
        b_changed = true;
        break;
    case 0x2e: /* Erase non displayed memory */
        Eia608EraseScreen( h, false );
        break;
    case 0x2f: /* End of caption (flip screen if not paint on) */
        if( h->mode != EIA608_MODE_PAINTON )
            h->i_screen = 1 - h->i_screen;
        h->mode = EIA608_MODE_POPUP;
        h->cursor.i_column = 0;
        h->cursor.i_row = 0;
        h->color = EIA608_COLOR_DEFAULT;
        h->font = EIA608_FONT_REGULAR;
        b_changed = true;
        break;
    }
    return b_changed;
}
static bool Eia608ParseCommand0x17( eia608_t *h, uint8_t d2 )
{
    switch( d2 )
    {
    case 0x21:  /* Tab offset 1 */
        Eia608Cursor( h, 1 );
        break;
    case 0x22:  /* Tab offset 2 */
        Eia608Cursor( h, 2 );
        break;
    case 0x23:  /* Tab offset 3 */
        Eia608Cursor( h, 3 );
        break;
    }
    return false;
}
static bool Eia608ParsePac( eia608_t *h, uint8_t d1, uint8_t d2 )
{
    static const int pi_row[] = {
        11, -1, 1, 2, 3, 4, 12, 13, 14, 15, 5, 6, 7, 8, 9, 10
    };
    const int i_row_index = ( (d1<<1) & 0x0e) | ( (d2>>5) & 0x01 );

    assert( d2 >= 0x40 && d2 <= 0x7f );

    if( pi_row[i_row_index] <= 0 )
        return false;

    /* Row */
    if( h->mode != EIA608_MODE_TEXT )
        h->cursor.i_row = pi_row[i_row_index] - 1;
    h->i_row_rollup = pi_row[i_row_index] - 1;
    /* Column */
    if( d2 >= 0x60 )
        d2 -= 0x60;
    else if( d2 >= 0x40 )
        d2 -= 0x40;
    h->cursor.i_column = pac2_attribs[d2].i_column;
    h->color = pac2_attribs[d2].i_color;
    h->font  = pac2_attribs[d2].i_font;

    return false;
}

static bool Eia608ParseData( eia608_t *h, uint8_t d1, uint8_t d2 )
{
    bool b_changed = false;

    if( d1 >= 0x18 && d1 <= 0x1f )
        d1 -= 8;

#define ON( d2min, d2max, cmd ) do { if( d2 >= d2min && d2 <= d2max ) b_changed = cmd; } while(0)
    switch( d1 )
    {
    case 0x11:
        ON( 0x20, 0x2f, Eia608ParseTextAttribute( h, d2 ) );
        ON( 0x30, 0x3f, Eia608ParseDouble( h, d2 ) );
        break;
    case 0x12: case 0x13:
        ON( 0x20, 0x3f, Eia608ParseExtended( h, d1, d2 ) );
        break;
    case 0x14: case 0x15:
        ON( 0x20, 0x2f, Eia608ParseCommand0x14( h, d2 ) );
        break;
    case 0x17:
        ON( 0x21, 0x22, Eia608ParseCommand0x17( h, d2 ) );
        ON( 0x2e, 0x2f, Eia608ParseTextAttribute( h, d2 ) );
        break;
    }
    if( d1 == 0x10 )
        ON( 0x40, 0x5f, Eia608ParsePac( h, d1, d2 ) );
    else if( d1 >= 0x11 && d1 <= 0x17 )
        ON( 0x40, 0x7f, Eia608ParsePac( h, d1, d2 ) );
#undef ON
    if( d1 >= 0x20 )
    {
        b_changed = Eia608ParseSingle( h, d1 );
        if( d2 >= 0x20 )
            b_changed |= Eia608ParseSingle( h, d2 );
    }
    return b_changed;
}

static void Eia608TextUtf8( char *psz_utf8, uint8_t c ) // Returns number of bytes used
{
#define E1(c,u) { c, { u, '\0' } }
#define E2(c,u1,u2) { c, { u1, u2, '\0' } }
#define E3(c,u1,u2,u3) { c, { u1, u2, u3, '\0' } }
    static const struct {
        uint8_t c;
        char utf8[3+1];
    } c2utf8[] = {
        // Regular line-21 character set, mostly ASCII except these exceptions
        E2( 0x2a, 0xc3,0xa1), // lowercase a, acute accent
        E2( 0x5c, 0xc3,0xa9), // lowercase e, acute accent
        E2( 0x5e, 0xc3,0xad), // lowercase i, acute accent
        E2( 0x5f, 0xc3,0xb3), // lowercase o, acute accent
        E2( 0x60, 0xc3,0xba), // lowercase u, acute accent
        E2( 0x7b, 0xc3,0xa7), // lowercase c with cedilla
        E2( 0x7c, 0xc3,0xb7), // division symbol
        E2( 0x7d, 0xc3,0x91), // uppercase N tilde
        E2( 0x7e, 0xc3,0xb1), // lowercase n tilde
        // THIS BLOCK INCLUDES THE 16 EXTENDED (TWO-BYTE) LINE 21 CHARACTERS
        // THAT COME FROM HI BYTE=0x11 AND LOW BETWEEN 0x30 AND 0x3F
        E2( 0x80, 0xc2,0xae), // Registered symbol (R)
        E2( 0x81, 0xc2,0xb0), // degree sign
        E2( 0x82, 0xc2,0xbd), // 1/2 symbol
        E2( 0x83, 0xc2,0xbf), // Inverted (open) question mark
        E3( 0x84, 0xe2,0x84,0xa2), // Trademark symbol (TM)
        E2( 0x85, 0xc2,0xa2), // Cents symbol
        E2( 0x86, 0xc2,0xa3), // Pounds sterling
        E3( 0x87, 0xe2,0x99,0xaa), // Music note
        E2( 0x88, 0xc3,0xa0), // lowercase a, grave accent
        E2( 0x89, 0xc2,0xa0), // transparent space
        E2( 0x8a, 0xc3,0xa8), // lowercase e, grave accent
        E2( 0x8b, 0xc3,0xa2), // lowercase a, circumflex accent
        E2( 0x8c, 0xc3,0xaa), // lowercase e, circumflex accent
        E2( 0x8d, 0xc3,0xae), // lowercase i, circumflex accent
        E2( 0x8e, 0xc3,0xb4), // lowercase o, circumflex accent
        E2( 0x8f, 0xc3,0xbb), // lowercase u, circumflex accent
        // THIS BLOCK INCLUDES THE 32 EXTENDED (TWO-BYTE) LINE 21 CHARACTERS
        // THAT COME FROM HI BYTE=0x12 AND LOW BETWEEN 0x20 AND 0x3F
        E2( 0x90, 0xc3,0x81), // capital letter A with acute
        E2( 0x91, 0xc3,0x89), // capital letter E with acute
        E2( 0x92, 0xc3,0x93), // capital letter O with acute
        E2( 0x93, 0xc3,0x9a), // capital letter U with acute
        E2( 0x94, 0xc3,0x9c), // capital letter U with diaresis
        E2( 0x95, 0xc3,0xbc), // lowercase letter U with diaeresis
        E1( 0x96, 0x27), // apostrophe
        E2( 0x97, 0xc2,0xa1), // inverted exclamation mark
        E1( 0x98, 0x2a), // asterisk
        E1( 0x99, 0x27), // apostrophe (yes, duped). See CCADI source code.
        E1( 0x9a, 0x2d), // hyphen-minus
        E2( 0x9b, 0xc2,0xa9), // copyright sign
        E3( 0x9c, 0xe2,0x84,0xa0), // Service mark
        E1( 0x9d, 0x2e), // Full stop (.)
        E3( 0x9e, 0xe2,0x80,0x9c), // Quotation mark
        E3( 0x9f, 0xe2,0x80,0x9d), // Quotation mark
        E2( 0xa0, 0xc3,0x80), // uppercase A, grave accent
        E2( 0xa1, 0xc3,0x82), // uppercase A, circumflex
        E2( 0xa2, 0xc3,0x87), // uppercase C with cedilla
        E2( 0xa3, 0xc3,0x88), // uppercase E, grave accent
        E2( 0xa4, 0xc3,0x8a), // uppercase E, circumflex
        E2( 0xa5, 0xc3,0x8b), // capital letter E with diaresis
        E2( 0xa6, 0xc3,0xab), // lowercase letter e with diaresis
        E2( 0xa7, 0xc3,0x8e), // uppercase I, circumflex
        E2( 0xa8, 0xc3,0x8f), // uppercase I, with diaresis
        E2( 0xa9, 0xc3,0xaf), // lowercase i, with diaresis
        E2( 0xaa, 0xc3,0x94), // uppercase O, circumflex
        E2( 0xab, 0xc3,0x99), // uppercase U, grave accent
        E2( 0xac, 0xc3,0xb9), // lowercase u, grave accent
        E2( 0xad, 0xc3,0x9b), // uppercase U, circumflex
        E2( 0xae, 0xc2,0xab), // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
        E2( 0xaf, 0xc2,0xbb), // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
        // THIS BLOCK INCLUDES THE 32 EXTENDED (TWO-BYTE) LINE 21 CHARACTERS
        // THAT COME FROM HI BYTE=0x13 AND LOW BETWEEN 0x20 AND 0x3F
        E2( 0xb0, 0xc3,0x83), // Uppercase A, tilde
        E2( 0xb1, 0xc3,0xa3), // Lowercase a, tilde
        E2( 0xb2, 0xc3,0x8d), // Uppercase I, acute accent
        E2( 0xb3, 0xc3,0x8c), // Uppercase I, grave accent
        E2( 0xb4, 0xc3,0xac), // Lowercase i, grave accent
        E2( 0xb5, 0xc3,0x92), // Uppercase O, grave accent
        E2( 0xb6, 0xc3,0xb2), // Lowercase o, grave accent
        E2( 0xb7, 0xc3,0x95), // Uppercase O, tilde
        E2( 0xb8, 0xc3,0xb5), // Lowercase o, tilde
        E1( 0xb9, 0x7b), // Open curly brace
        E1( 0xba, 0x7d), // Closing curly brace
        E1( 0xbb, 0x5c), // Backslash
        E1( 0xbc, 0x5e), // Caret
        E1( 0xbd, 0x5f), // Underscore
        E2( 0xbe, 0xc2,0xa6), // Pipe (broken bar)
        E1( 0xbf, 0x7e), // Tilde (utf8 code unsure)
        E2( 0xc0, 0xc3,0x84), // Uppercase A, umlaut
        E2( 0xc1, 0xc3,0xa4), // Lowercase A, umlaut
        E2( 0xc2, 0xc3,0x96), // Uppercase O, umlaut
        E2( 0xc3, 0xc3,0xb6), // Lowercase o, umlaut
        E2( 0xc4, 0xc3,0x9f), // Esszett (sharp S)
        E2( 0xc5, 0xc2,0xa5), // Yen symbol
        E2( 0xc6, 0xc2,0xa4), // Currency symbol
        E1( 0xc7, 0x7c), // Vertical bar
        E2( 0xc8, 0xc3,0x85), // Uppercase A, ring
        E2( 0xc9, 0xc3,0xa5), // Lowercase A, ring
        E2( 0xca, 0xc3,0x98), // Uppercase O, slash
        E2( 0xcb, 0xc3,0xb8), // Lowercase o, slash
        E3( 0xcc, 0xe2,0x8c,0x9c), // Upper left corner
        E3( 0xcd, 0xe2,0x8c,0x9d), // Upper right corner
        E3( 0xce, 0xe2,0x8c,0x9e), // Lower left corner
        E3( 0xcf, 0xe2,0x8c,0x9f), // Lower right corner

        E1(0,0)
    };
#undef E3
#undef E2
#undef E1

    for( size_t i = 0; i < ARRAY_SIZE(c2utf8) ; i++ )
        if( c2utf8[i].c == c ) {
            strcpy( psz_utf8, c2utf8[i].utf8 );
            return;
        }

    psz_utf8[0] = c < 0x80 ? c : '?';   /* Normal : Unsupported */
    psz_utf8[1] = '\0';
}

static void Eia608Strlcat( char *d, const char *s, int i_max )
{
    if( i_max > 1 )
        strncat( d, s, i_max-1 - strnlen(d, i_max-1));
    if( i_max > 0 )
        d[i_max-1] = '\0';
}

#define CAT(t) Eia608Strlcat( psz_text, t, i_text_max )

static void Eia608TextLine( struct eia608_screen *screen, char *psz_text, int i_text_max, int i_row, bool b_html )
{
    const uint8_t *p_char = screen->characters[i_row];
    const eia608_color_t *p_color = screen->colors[i_row];
    const eia608_font_t *p_font = screen->fonts[i_row];
    int i_start;
    int i_end;
    int x;
    eia608_color_t last_color = EIA608_COLOR_DEFAULT;
    bool     b_last_italics = false;
    bool     b_last_underline = false;
    char utf8[4];

    /* Search the start */
    i_start = 0;

    /* Ensure we get a monospaced font (required for accurate positioning */
    if( b_html )
        CAT( "<tt>" );

    /* Convert leading spaces to non-breaking so that they don't get
       stripped by the RenderHtml routine as regular whitespace */
    while( i_start < EIA608_SCREEN_COLUMNS && p_char[i_start] == ' ' ) {
        Eia608TextUtf8( utf8, 0x89 );
        CAT( utf8 );
        i_start++;
    }

    /* Search the end */
    i_end = EIA608_SCREEN_COLUMNS-1;
    while( i_end > i_start && p_char[i_end] == ' ' )
        i_end--;

    /* */
    for( x = i_start; x <= i_end; x++ )
    {
        eia608_color_t color = p_color[x];
        bool b_italics = p_font[x] & EIA608_FONT_ITALICS;
        bool b_underline = p_font[x] & EIA608_FONT_UNDERLINE;

        /* */
        if( b_html )
        {
            bool b_close_color, b_close_italics, b_close_underline;

            /* We create the tags font / i / u in that orders */
            b_close_color = color != last_color && last_color != EIA608_COLOR_DEFAULT;
            b_close_italics = !b_italics && b_last_italics;
            b_close_underline = !b_underline && b_last_underline;

            /* Be sure to create valid html */
            b_close_italics |= b_last_italics && b_close_color;
            b_close_underline |= b_last_underline && ( b_close_italics || b_close_color );

            if( b_close_underline )
                CAT( "</u>" );
            if( b_close_italics )
                CAT( "</i>" );
            if( b_close_color )
                CAT( "</font>" );

            if( color != EIA608_COLOR_DEFAULT && color != last_color)
            {
                static const char *ppsz_color[] = {
                    "#ffffff",  // white
                    "#00ff00",  // green
                    "#0000ff",  // blue
                    "#00ffff",  // cyan
                    "#ff0000",  // red
                    "#ffff00",  // yellow
                    "#ff00ff",  // magenta
                    "#ffffff",  // user defined XXX we use white
                };
                CAT( "<font color=\"" );
                CAT( ppsz_color[color] );
                CAT( "\">" );
            }
            if( ( b_close_italics && b_italics ) || ( b_italics && !b_last_italics ) )
                CAT( "<i>" );
            if( ( b_close_underline && b_underline ) || ( b_underline && !b_last_underline ) )
                CAT( "<u>" );
        }

        if( b_html ) {
            /* Escape XML reserved characters
               http://www.w3.org/TR/xml/#syntax */
            switch (p_char[x]) {
            case '>':
                CAT( "&gt;" );
                break;
            case '<':
                CAT( "&lt;" );
                break;
            case '"':
                CAT( "&quot;" );
                break;
            case '\'':
                CAT( "&apos;" );
                break;
            case '&':
                CAT( "&amp;" );
                break;
            default:
                Eia608TextUtf8( utf8, p_char[x] );
                CAT( utf8 );
                break;
            }
        } else {
            Eia608TextUtf8( utf8, p_char[x] );
            CAT( utf8 );
        }

        /* */
        b_last_underline = b_underline;
        b_last_italics = b_italics;
        last_color = color;
    }
    if( b_html )
    {
        if( b_last_underline )
            CAT( "</u>" );
        if( b_last_italics )
            CAT( "</i>" );
        if( last_color != EIA608_COLOR_DEFAULT )
            CAT( "</font>" );
        CAT( "</tt>" );
    }
#undef CAT
}

/* */
static void Eia608Init( eia608_t *h )
{
    memset( h, 0, sizeof(*h) );

    /* */
    h->i_channel = -1;

    h->i_screen = 0;
    Eia608ClearScreen( h, 0 );
    Eia608ClearScreen( h, 1 );

    /* Cursor for writing text */
    h->cursor.i_column = 0;
    h->cursor.i_row = 0;

    h->last.d1 = 0x00;
    h->last.d2 = 0x00;
    h->mode = EIA608_MODE_POPUP;
    h->color = EIA608_COLOR_DEFAULT;
    h->font = EIA608_FONT_REGULAR;
    h->i_row_rollup = EIA608_SCREEN_ROWS-1;
}
static bool Eia608Parse( eia608_t *h, int i_channel_selected, const uint8_t data[2] )
{
    const uint8_t d1 = data[0] & 0x7f; /* Removed parity bit */
    const uint8_t d2 = data[1] & 0x7f;
    bool b_screen_changed = false;

    if( d1 == 0 && d2 == 0 )
        return false;   /* Ignore padding (parity check are sometimes invalid on them) */

    Eia608ParseChannel( h, data );
    if( h->i_channel != i_channel_selected )
        return false;
    //fprintf( stderr, "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC %x %x\n", data[0], data[1] );

    if( d1 >= 0x10 )
    {
        if( d1 >= 0x20 ||
            d1 != h->last.d1 || d2 != h->last.d2 ) /* Command codes can be repeated */
            b_screen_changed = Eia608ParseData( h, d1,d2 );

        h->last.d1 = d1;
        h->last.d2 = d2;
    }
    else if( ( d1 >= 0x01 && d1 <= 0x0E ) || d1 == 0x0F )
    {
        /* XDS block / End of XDS block */
    }
    return b_screen_changed;
}

static char *Eia608Text( eia608_t *h, bool b_html )
{
    const int i_size = EIA608_SCREEN_ROWS * 10 * EIA608_SCREEN_COLUMNS+1;
    struct eia608_screen *screen = &h->screen[h->i_screen];
    bool b_first = true;
    char *psz;

    /* We allocate a buffer big enough for normal case */
    psz = malloc( i_size );
    if( !psz )
        return NULL;
    *psz = '\0';
    if( b_html )
        Eia608Strlcat( psz, "<text>", i_size );
    for( int i = 0; i < EIA608_SCREEN_ROWS; i++ )
    {
        if( !b_first )
            Eia608Strlcat( psz, b_html ? "<br />" : "\n", i_size );
        b_first = false;

        Eia608TextLine( screen, psz, i_size, i, b_html );
    }
    if( b_html )
        Eia608Strlcat( psz, "</text>", i_size );
    return psz;
}
