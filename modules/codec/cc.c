/*****************************************************************************
 * cc.c : CC 608/708 subtitles decoder
 *****************************************************************************
 * Copyright © 2007-2011 Laurent Aimar, VLC authors and VideoLAN
 *             2011-2016 VLC authors and VideoLAN
 *             2016-2017 VideoLabs, VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_charset.h>

#include "substext.h"
#include "cea708.h"

#if 0
#define Debug(code) code
#else
#define Debug(code)
#endif

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define OPAQUE_TEXT N_("Opacity")
#define OPAQUE_LONGTEXT N_("Setting to true " \
        "makes the text to be boxed and maybe easier to read." )

vlc_module_begin ()
    set_shortname( N_("CC 608/708"))
    set_description( N_("Closed Captions decoder") )
    set_capability( "spu decoder", 50 )
    set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, Close )

    add_bool( "cc-opaque", true,
                 OPAQUE_TEXT, OPAQUE_LONGTEXT, false )
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

#define EIA608_MARGIN  0.10
#define EIA608_VISIBLE (1.0 - EIA608_MARGIN * 2)
#define FONT_TO_LINE_HEIGHT_RATIO 1.06

struct eia608_screen // A CC buffer
{
    uint8_t characters[EIA608_SCREEN_ROWS][EIA608_SCREEN_COLUMNS+1];
    eia608_color_t colors[EIA608_SCREEN_ROWS][EIA608_SCREEN_COLUMNS+1];
    eia608_font_t fonts[EIA608_SCREEN_ROWS][EIA608_SCREEN_COLUMNS+1]; // Extra char at the end for a 0
    int row_used[EIA608_SCREEN_ROWS]; // Any data in row?
};
typedef struct eia608_screen eia608_screen;

typedef enum
{
    EIA608_STATUS_DEFAULT         = 0x00,
    EIA608_STATUS_CHANGED         = 0x01, /* current screen has been altered */
    EIA608_STATUS_CAPTION_ENDED   = 0x02, /* screen flip */
    EIA608_STATUS_CAPTION_CLEARED = 0x04, /* active screen erased */
    EIA608_STATUS_DISPLAY         = EIA608_STATUS_CAPTION_CLEARED | EIA608_STATUS_CAPTION_ENDED,
} eia608_status_t;

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

static const int rgi_eia608_colors[] = {
    0xffffff,  // white
    0x00ff00,  // green
    0x0000ff,  // blue
    0x00ffff,  // cyan
    0xff0000,  // red
    0xffff00,  // yellow
    0xff00ff,  // magenta
    0xffffff,  // user defined XXX we use white
};

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
static eia608_status_t Eia608Parse( eia608_t *h, int i_channel_selected, const uint8_t data[2] );
static void         Eia608FillUpdaterRegions( subtext_updater_sys_t *p_updater, eia608_t *h );

/* It will be enough up to 63 B frames, which is far too high for
 * broadcast environment */
#define CC_MAX_REORDER_SIZE (64)
typedef struct
{
    int      i_queue;
    block_t *p_queue;

    int i_field;
    int i_channel;

    int i_reorder_depth;

    cea708_demux_t *p_dtvcc;

    cea708_t *p_cea708;
    eia608_t *p_eia608;
    bool b_opaque;
} decoder_sys_t;

static int Decode( decoder_t *, block_t * );
static void Flush( decoder_t * );

static void DTVCC_ServiceData_Handler( void *priv, uint8_t i_sid, vlc_tick_t i_time,
                                       const uint8_t *p_data, size_t i_data )
{
    decoder_t *p_dec = priv;
    decoder_sys_t *p_sys = p_dec->p_sys;
    //msg_Err( p_dec, "DTVCC_ServiceData_Handler sid %d bytes %ld", i_sid, i_data );
    if( i_sid == 1 + p_dec->fmt_in.subs.cc.i_channel )
        CEA708_Decoder_Push( p_sys->p_cea708, i_time, p_data, i_data );
}

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

    if( ( p_dec->fmt_in.i_codec != VLC_CODEC_CEA608 ||
          p_dec->fmt_in.subs.cc.i_channel > 3 ) &&
        ( p_dec->fmt_in.i_codec != VLC_CODEC_CEA708 ||
          p_dec->fmt_in.subs.cc.i_channel > 63 ) )
        return VLC_EGENERIC;

    p_dec->pf_decode = Decode;
    p_dec->pf_flush  = Flush;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    if( p_dec->fmt_in.i_codec == VLC_CODEC_CEA608 )
    {
        /*  0 -> i_field = 0; i_channel = 1;
            1 -> i_field = 0; i_channel = 2;
            2 -> i_field = 1; i_channel = 1;
            3 -> i_field = 1; i_channel = 2; */
        p_sys->i_field = p_dec->fmt_in.subs.cc.i_channel >> 1;
        p_sys->i_channel = 1 + (p_dec->fmt_in.subs.cc.i_channel & 1);

        p_sys->p_eia608 = malloc(sizeof(*p_sys->p_eia608));
        if( !p_sys->p_eia608 )
        {
            free( p_sys );
            return VLC_ENOMEM;
        }
        Eia608Init( p_sys->p_eia608 );
    }
    else
    {
        p_sys->p_dtvcc = CEA708_DTVCC_Demuxer_New( p_dec, DTVCC_ServiceData_Handler );
        if( !p_sys->p_dtvcc )
        {
            free( p_sys );
            return VLC_ENOMEM;
        }

        p_sys->p_cea708 = CEA708_Decoder_New( p_dec );
        if( !p_sys->p_cea708 )
        {
            CEA708_DTVCC_Demuxer_Release( p_sys->p_dtvcc );
            free( p_sys );
            return VLC_ENOMEM;
        }

         p_sys->i_channel = p_dec->fmt_in.subs.cc.i_channel;
    }

    p_sys->b_opaque = var_InheritBool( p_dec, "cc-opaque" );
    p_sys->i_reorder_depth = p_dec->fmt_in.subs.cc.i_reorder_depth;

    p_dec->fmt_out.i_codec = VLC_CODEC_TEXT;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_eia608 )
    {
        Eia608Init( p_sys->p_eia608 );
    }
    else
    {
        CEA708_DTVCC_Demuxer_Flush( p_sys->p_dtvcc );
        CEA708_Decoder_Flush( p_sys->p_cea708 );
    }

    block_ChainRelease( p_sys->p_queue );
    p_sys->p_queue = NULL;
    p_sys->i_queue = 0;
}

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************
 *
 ****************************************************************************/
static void     Push( decoder_t *, block_t * );
static block_t *Pop( decoder_t *, bool );
static void     Convert( decoder_t *, vlc_tick_t, const uint8_t *, size_t );

static bool DoDecode( decoder_t *p_dec, bool b_drain )
{
    block_t *p_block = Pop( p_dec, b_drain );
    if( !p_block )
        return false;

    Convert( p_dec, p_block->i_pts, p_block->p_buffer, p_block->i_buffer );
    block_Release( p_block );

    return true;
}

static int Decode( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_block )
    {
        /* Reset decoder if needed */
        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED) )
        {
            /* Drain */
            for( ; DoDecode( p_dec, true ) ; );
            if( p_sys->p_eia608 )
            {
                Eia608Init( p_sys->p_eia608 );
            }
            else
            {
                CEA708_DTVCC_Demuxer_Flush( p_sys->p_dtvcc );
                CEA708_Decoder_Flush( p_sys->p_cea708 );
            }

            if( (p_block->i_flags & BLOCK_FLAG_CORRUPTED) || p_block->i_buffer < 1 )
            {
                block_Release( p_block );
                return VLCDEC_SUCCESS;
            }
        }

        /* XXX Cc captions data are OUT OF ORDER (because we receive them in the bitstream
         * order (ie ordered by video picture dts) instead of the display order.
         *  We will simulate a simple IPB buffer scheme
         * and reorder with pts.
         * XXX it won't work with H264 which use non out of order B picture or MMCO */
        if( p_sys->i_reorder_depth == 0 )
        {
            /* Wait for a P and output all *previous* picture by pts order (for
             * hierarchical B frames) */
            if( (p_block->i_flags & BLOCK_FLAG_TYPE_B) == 0 )
                for( ; DoDecode( p_dec, true ); );
        }

        Push( p_dec, p_block );
    }

    const bool b_no_reorder = (p_dec->fmt_in.subs.cc.i_reorder_depth < 0);
    for( ; DoDecode( p_dec, (p_block == NULL) || b_no_reorder ); );

    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys->p_eia608 );
    if( p_sys->p_cea708 )
    {
        CEA708_Decoder_Release( p_sys->p_cea708 );
        CEA708_DTVCC_Demuxer_Release( p_sys->p_dtvcc );
    }

    block_ChainRelease( p_sys->p_queue );
    free( p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Push( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->i_queue >= CC_MAX_REORDER_SIZE )
    {
        block_Release( Pop( p_dec, true ) );
        msg_Warn( p_dec, "Trashing a CC entry" );
    }

    block_t **pp_block;
    /* find insertion point */
    for( pp_block = &p_sys->p_queue; *pp_block ; pp_block = &((*pp_block)->p_next) )
    {
        if( p_block->i_pts == VLC_TICK_INVALID || (*pp_block)->i_pts == VLC_TICK_INVALID )
            continue;
        if( p_block->i_pts < (*pp_block)->i_pts )
        {
            if( p_sys->i_reorder_depth > 0 &&
                p_sys->i_queue < p_sys->i_reorder_depth &&
                pp_block == &p_sys->p_queue )
            {
                msg_Info( p_dec, "Increasing reorder depth to %d", ++p_sys->i_reorder_depth );
            }
            break;
        }
    }
    /* Insert, keeping a pts and/or fifo ordered list */
    p_block->p_next = *pp_block ? *pp_block : NULL;
    *pp_block = p_block;
    p_sys->i_queue++;
}

static block_t *Pop( decoder_t *p_dec, bool b_forced )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

     if( p_sys->i_queue == 0 )
         return NULL;

     if( !b_forced && p_sys->i_queue < CC_MAX_REORDER_SIZE )
     {
        if( p_sys->i_queue < p_sys->i_reorder_depth || p_sys->i_reorder_depth == 0 )
            return NULL;
     }

     /* dequeue head */
     p_block = p_sys->p_queue;
     p_sys->p_queue = p_block->p_next;
     p_block->p_next = NULL;
     p_sys->i_queue--;

    return p_block;
}

static subpicture_t *Subtitle( decoder_t *p_dec, eia608_t *h, vlc_tick_t i_pts )
{
    //decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;

    /* We cannot display a subpicture with no date */
    if( i_pts == VLC_TICK_INVALID )
        return NULL;

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
        return NULL;

    p_spu->i_start    = i_pts;
    p_spu->i_stop     = i_pts + VLC_TICK_FROM_SEC(10);   /* 10s max */
    p_spu->b_ephemer  = true;
    p_spu->b_absolute = false;

    subtext_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;
    decoder_sys_t *p_dec_sys = p_dec->p_sys;

    /* Set first region defaults */
    /* The "leavetext" alignment is a special mode where the subpicture
       region itself gets aligned, but the text inside it does not */
    p_spu_sys->region.align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
    p_spu_sys->region.inner_align = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_LEFT;
    p_spu_sys->region.flags = UPDT_REGION_IGNORE_BACKGROUND | UPDT_REGION_USES_GRID_COORDINATES;

    /* Set style defaults (will be added to segments if none set) */
    p_spu_sys->p_default_style->i_style_flags |= STYLE_MONOSPACED;
    if( p_dec_sys->b_opaque )
    {
        p_spu_sys->p_default_style->i_background_alpha = STYLE_ALPHA_OPAQUE;
        p_spu_sys->p_default_style->i_features |= STYLE_HAS_BACKGROUND_ALPHA;
        p_spu_sys->p_default_style->i_style_flags |= STYLE_BACKGROUND;
    }
    p_spu_sys->margin_ratio = EIA608_MARGIN;
    p_spu_sys->p_default_style->i_font_color = rgi_eia608_colors[EIA608_COLOR_DEFAULT];
    /* FCC defined "safe area" for EIA-608 captions is 80% of the height of the display */
    p_spu_sys->p_default_style->f_font_relsize = EIA608_VISIBLE * 100 / EIA608_SCREEN_ROWS /
                                                 FONT_TO_LINE_HEIGHT_RATIO;
    p_spu_sys->p_default_style->i_features |= (STYLE_HAS_FONT_COLOR | STYLE_HAS_FLAGS);

    Eia608FillUpdaterRegions( p_spu_sys, h );

    return p_spu;
}

static void Convert( decoder_t *p_dec, vlc_tick_t i_pts,
                     const uint8_t *p_buffer, size_t i_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    unsigned i_ticks = 0;
    while( i_buffer >= 3 )
    {
        if( (p_buffer[0] & 0x04) /* Valid bit */ )
        {
            const vlc_tick_t i_spupts = i_pts + vlc_tick_from_samples(i_ticks, 1200/3);
            /* Mask off the specific i_field bit, else some sequences can be lost. */
            if ( p_sys->p_eia608 &&
                (p_buffer[0] & 0x03) == p_sys->i_field )
            {
                eia608_status_t i_status = Eia608Parse( p_sys->p_eia608,
                                                        p_sys->i_channel, &p_buffer[1] );

                /* a caption is ready or removed, process its screen */
                /*
                 * In case of rollup/painton with 1 packet/frame, we need
                 * to update on Changed status.
                 * Batch decoding might be incorrect if those in
                 * large number of commands (mp4, ...) then.
                 * see CEAv1.2zero.trp tests */
                if( i_status & (EIA608_STATUS_DISPLAY | EIA608_STATUS_CHANGED) )
                {
                    Debug(printf("\n"));
                    subpicture_t *p_spu = Subtitle( p_dec, p_sys->p_eia608, i_spupts );
                    if( p_spu )
                        decoder_QueueSub( p_dec, p_spu );
                }
            }
            else if( p_sys->p_cea708 && (p_buffer[0] & 0x03) >= 2 )
            {
                CEA708_DTVCC_Demuxer_Push( p_sys->p_dtvcc, i_spupts, p_buffer );
            }
        }

        i_ticks++;

        i_buffer -= 3;
        p_buffer += 3;
    }
}


/*****************************************************************************
 *
 *****************************************************************************/
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
        vlc_assert_unreachable();
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
static eia608_status_t Eia608ParseTextAttribute( eia608_t *h, uint8_t d2 )
{
    const int i_index = d2 - 0x20;
    assert( d2 >= 0x20 && d2 <= 0x2f );

    Debug(printf("[TA %d]", i_index));
    h->color = pac2_attribs[i_index].i_color;
    h->font  = pac2_attribs[i_index].i_font;
    Eia608Cursor( h, 1 );

    return EIA608_STATUS_DEFAULT;
}
static eia608_status_t Eia608ParseSingle( eia608_t *h, const uint8_t dx )
{
    assert( dx >= 0x20 );
    Eia608Write( h, dx );
    return EIA608_STATUS_CHANGED;
}
static eia608_status_t Eia608ParseDouble( eia608_t *h, uint8_t d2 )
{
    assert( d2 >= 0x30 && d2 <= 0x3f );
    Debug(printf("\033[0;33m%s\033[0m", d2 + 0x50));
    Eia608Write( h, d2 + 0x50 ); /* We use charaters 0x80...0x8f */
    return EIA608_STATUS_CHANGED;
}
static eia608_status_t Eia608ParseExtended( eia608_t *h, uint8_t d1, uint8_t d2 )
{
    assert( d2 >= 0x20 && d2 <= 0x3f );
    assert( d1 == 0x12 || d1 == 0x13 );
    if( d1 == 0x12 )
        d2 += 0x70; /* We use charaters 0x90-0xaf */
    else
        d2 += 0x90; /* We use charaters 0xb0-0xcf */

    Debug(printf("[EXT %x->'%c']", d2, d2));
    /* The extended characters replace the previous one with a more
     * advanced one */
    Eia608Cursor( h, -1 );
    Eia608Write( h, d2 );
    return EIA608_STATUS_CHANGED;
}
static eia608_status_t Eia608ParseCommand0x14( eia608_t *h, uint8_t d2 )
{
    eia608_status_t i_status = EIA608_STATUS_DEFAULT;
    eia608_mode_t proposed_mode;

    switch( d2 )
    {
    case 0x20:  /* Resume caption loading */
        Debug(printf("[RCL]"));
        h->mode = EIA608_MODE_POPUP;
        break;
    case 0x21:  /* Backspace */
        Debug(printf("[BS]"));
        Eia608Erase( h );
        i_status = EIA608_STATUS_CHANGED;
        break;
    case 0x22:  /* Reserved */
    case 0x23:
        Debug(printf("[ALARM %d]", d2 - 0x22));
        break;
    case 0x24:  /* Delete to end of row */
        Debug(printf("[DER]"));
        Eia608EraseToEndOfRow( h );
        break;
    case 0x25:  /* Rollup 2 */
    case 0x26:  /* Rollup 3 */
    case 0x27:  /* Rollup 4 */
        Debug(printf("[RU%d]", d2 - 0x23));
        if( h->mode == EIA608_MODE_POPUP || h->mode == EIA608_MODE_PAINTON )
        {
            Eia608EraseScreen( h, true );
            Eia608EraseScreen( h, false );
            i_status = EIA608_STATUS_CHANGED | EIA608_STATUS_CAPTION_CLEARED;
        }

        if( d2 == 0x25 )
            proposed_mode = EIA608_MODE_ROLLUP_2;
        else if( d2 == 0x26 )
            proposed_mode = EIA608_MODE_ROLLUP_3;
        else
            proposed_mode = EIA608_MODE_ROLLUP_4;

        if ( proposed_mode != h->mode )
        {
            h->mode = proposed_mode;
            h->cursor.i_column = 0;
            h->cursor.i_row = h->i_row_rollup;
        }
        break;
    case 0x28:  /* Flash on */
        Debug(printf("[FON]"));
        /* TODO */
        break;
    case 0x29:  /* Resume direct captionning */
        Debug(printf("[RDC]"));
        h->mode = EIA608_MODE_PAINTON;
        break;
    case 0x2a:  /* Text restart */
        Debug(printf("[TR]"));
        /* TODO */
        break;

    case 0x2b: /* Resume text display */
        Debug(printf("[RTD]"));
        h->mode = EIA608_MODE_TEXT;
        break;

    case 0x2c: /* Erase displayed memory */
        Debug(printf("[EDM]"));
        Eia608EraseScreen( h, true );
        i_status = EIA608_STATUS_CHANGED | EIA608_STATUS_CAPTION_CLEARED;
        break;
    case 0x2d: /* Carriage return */
        Debug(printf("[CR]"));
        Eia608RollUp(h);
        i_status = EIA608_STATUS_CHANGED;
        break;
    case 0x2e: /* Erase non displayed memory */
        Debug(printf("[ENM]"));
        Eia608EraseScreen( h, false );
        break;
    case 0x2f: /* End of caption (flip screen if not paint on) */
        Debug(printf("[EOC]"));
        if( h->mode != EIA608_MODE_PAINTON )
            h->i_screen = 1 - h->i_screen;
        h->mode = EIA608_MODE_POPUP;
        h->cursor.i_column = 0;
        h->cursor.i_row = 0;
        h->color = EIA608_COLOR_DEFAULT;
        h->font = EIA608_FONT_REGULAR;
        i_status = EIA608_STATUS_CHANGED | EIA608_STATUS_CAPTION_ENDED;
        break;
    }
    return i_status;
}
static bool Eia608ParseCommand0x17( eia608_t *h, uint8_t d2 )
{
    switch( d2 )
    {
    case 0x21:  /* Tab offset 1 */
    case 0x22:  /* Tab offset 2 */
    case 0x23:  /* Tab offset 3 */
        Debug(printf("[TO%d]", d2 - 0x20));
        Eia608Cursor( h, d2 - 0x20 );
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

    Debug(printf("[PAC,%d]", i_row_index));
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

static eia608_status_t Eia608ParseData( eia608_t *h, uint8_t d1, uint8_t d2 )
{
    eia608_status_t i_status = EIA608_STATUS_DEFAULT;

    if( d1 >= 0x18 && d1 <= 0x1f )
        d1 -= 8;

#define ON( d2min, d2max, cmd ) do { if( d2 >= d2min && d2 <= d2max ) i_status = cmd; } while(0)
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
        ON( 0x21, 0x23, Eia608ParseCommand0x17( h, d2 ) );
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
        Debug(printf("\033[0;33m%c", d1));
        i_status = Eia608ParseSingle( h, d1 );
        if( d2 >= 0x20 )
        {
            Debug(printf("%c", d2));
            i_status |= Eia608ParseSingle( h, d2 );
        }
        Debug(printf("\033[0m"));
    }

    /* Ignore changes occuring to doublebuffer */
    if( h->mode == EIA608_MODE_POPUP && i_status == EIA608_STATUS_CHANGED )
        i_status = EIA608_STATUS_DEFAULT;

    return i_status;
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

static text_segment_t * Eia608TextLine( struct eia608_screen *screen, int i_row )
{
    const uint8_t *p_char = screen->characters[i_row];
    const eia608_color_t *p_color = screen->colors[i_row];
    const eia608_font_t *p_font = screen->fonts[i_row];
    int i_start;
    int i_end;
    int x;
    eia608_color_t prev_color = EIA608_COLOR_DEFAULT;
    eia608_font_t prev_font = EIA608_FONT_REGULAR;

    char utf8[4];
    const unsigned i_text_max = 4 * EIA608_SCREEN_COLUMNS + 1;
    char psz_text[i_text_max + 1];
    psz_text[0] = '\0';

    /* Search the start */
    i_start = 0;

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
    if( i_start > i_end ) /* Nothing to render */
        return NULL;

    text_segment_t *p_segment, *p_segments_head = p_segment = text_segment_New( NULL );
    if(!p_segment)
        return NULL;

    p_segment->style = text_style_Create( STYLE_NO_DEFAULTS );
    if(!p_segment->style)
    {
        text_segment_Delete(p_segment);
        return NULL;
    }
    /* Ensure we get a monospaced font (required for accurate positioning */
    p_segment->style->i_style_flags |= STYLE_MONOSPACED;

    for( x = i_start; x <= i_end; x++ )
    {
        eia608_color_t color = p_color[x];
        eia608_font_t font = p_font[x];

        if(font != prev_font || color != prev_color)
        {
            EnsureUTF8(psz_text);
            p_segment->psz_text = strdup(psz_text);
            psz_text[0] = '\0';
            p_segment->p_next = text_segment_New( NULL );
            p_segment = p_segment->p_next;
            if(!p_segment)
                return p_segments_head;

            p_segment->style = text_style_Create( STYLE_NO_DEFAULTS );
            if(!p_segment->style)
            {
                text_segment_Delete(p_segment);
                return p_segments_head;
            }
            p_segment->style->i_style_flags |= STYLE_MONOSPACED;

            /* start segment with new style */
            if(font & EIA608_FONT_ITALICS)
            {
                p_segment->style->i_style_flags |= STYLE_ITALIC;
                p_segment->style->i_features |= STYLE_HAS_FLAGS;
            }
            if(font & EIA608_FONT_UNDERLINE)
            {
                p_segment->style->i_style_flags |= STYLE_UNDERLINE;
                p_segment->style->i_features |= STYLE_HAS_FLAGS;
            }

            if(color != EIA608_COLOR_DEFAULT)
            {
                p_segment->style->i_font_color = rgi_eia608_colors[color];
                p_segment->style->i_features |= STYLE_HAS_FONT_COLOR;
            }
        }

        Eia608TextUtf8( utf8, p_char[x] );
        CAT( utf8 );

        /* */
        prev_font = font;
        prev_color = color;
    }

#undef CAT

    if( p_segment )
    {
        assert(!p_segment->psz_text); // shouldn't happen
        EnsureUTF8(psz_text);
        p_segment->psz_text = strdup(psz_text);
    }

    return p_segments_head;
}

static void Eia608FillUpdaterRegions( subtext_updater_sys_t *p_updater, eia608_t *h )
{
    struct eia608_screen *screen = &h->screen[h->i_screen];
    substext_updater_region_t *p_region = &p_updater->region;
    text_segment_t **pp_last = &p_region->p_segments;
    bool b_newregion = false;

    for( int i = 0; i < EIA608_SCREEN_ROWS; i++ )
    {
        if( !screen->row_used[i] )
            continue;

        text_segment_t *p_segments = Eia608TextLine( screen, i );
        if( p_segments )
        {
            if( b_newregion )
            {
                substext_updater_region_t *p_newregion;
                p_newregion = SubpictureUpdaterSysRegionNew();
                if( !p_newregion )
                {
                    text_segment_ChainDelete( p_segments );
                    return;
                }
                /* Copy defaults */
                p_newregion->align = p_region->align;
                p_newregion->inner_align = p_region->inner_align;
                p_newregion->flags = p_region->flags;
                SubpictureUpdaterSysRegionAdd( p_region, p_newregion );
                p_region = p_newregion;
                pp_last = &p_region->p_segments;
                b_newregion = false;
            }

            if( p_region->p_segments == NULL ) /* First segment in the [new] region */
            {
                p_region->origin.y = (float) i /* start line number */
                                     / (EIA608_SCREEN_ROWS * FONT_TO_LINE_HEIGHT_RATIO);
                p_region->flags |= UPDT_REGION_ORIGIN_Y_IS_RATIO;
            }
            else /* Insert line break between region lines */
            {
                *pp_last = text_segment_New( "\n" );
                if( *pp_last )
                    pp_last = &((*pp_last)->p_next);
            }

            *pp_last = p_segments;
            do { pp_last = &((*pp_last)->p_next); } while ( *pp_last != NULL );
        }
        else
        {
            b_newregion = !!p_region->p_segments;
        }
    }
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
static eia608_status_t Eia608Parse( eia608_t *h, int i_channel_selected, const uint8_t data[2] )
{
    const uint8_t d1 = data[0] & 0x7f; /* Removed parity bit */
    const uint8_t d2 = data[1] & 0x7f;
    eia608_status_t i_screen_status = EIA608_STATUS_DEFAULT;

    if( d1 == 0 && d2 == 0 )
        return EIA608_STATUS_DEFAULT;   /* Ignore padding (parity check are sometimes invalid on them) */

    Eia608ParseChannel( h, data );
    if( h->i_channel != i_channel_selected )
        return false;
    //fprintf( stderr, "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC %x %x\n", data[0], data[1] );

    if( d1 >= 0x10 )
    {
        if( d1 >= 0x20 ||
            d1 != h->last.d1 || d2 != h->last.d2 ) /* Command codes can be repeated */
            i_screen_status = Eia608ParseData( h, d1,d2 );

        h->last.d1 = d1;
        h->last.d2 = d2;
    }
    else if( ( d1 >= 0x01 && d1 <= 0x0E ) || d1 == 0x0F )
    {
        /* XDS block / End of XDS block */
    }
    return i_screen_status;
}
