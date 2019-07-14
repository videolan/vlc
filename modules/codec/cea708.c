/*****************************************************************************
 * cea708.c : CEA708 subtitles decoder
 *****************************************************************************
 * Copyright © 2017 VideoLabs, VideoLAN and VLC authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_subpicture.h>

#include "cea708.h"
#include "substext.h"

#include <assert.h>

#if 0
#define Debug(code) code
#else
#define Debug(code)
#endif

/*****************************************************************************
 * Demuxing / Agreggation
 *****************************************************************************/
struct cea708_demux_t
{
   int8_t  i_pkt_sequence;
   uint8_t i_total_data;
   uint8_t i_data;
   uint8_t data[CEA708_DTVCC_MAX_PKT_SIZE];
   vlc_tick_t i_time;
   service_data_hdlr_t p_callback;
   void *priv;
};

void CEA708_DTVCC_Demuxer_Flush( cea708_demux_t *h )
{
    h->i_pkt_sequence = -1;
    h->i_total_data = h->i_data = 0;
}

void CEA708_DTVCC_Demuxer_Release( cea708_demux_t *h )
{
    free( h );
}

cea708_demux_t * CEA708_DTVCC_Demuxer_New( void *priv, service_data_hdlr_t hdlr )
{
    cea708_demux_t *h = malloc( sizeof(cea708_demux_t) );
    if( h )
    {
        h->priv = priv;
        h->p_callback = hdlr;
        CEA708_DTVCC_Demuxer_Flush( h );
    }
    return h;
}

static void CEA708_DTVCC_Demux_ServiceBlocks( cea708_demux_t *h, vlc_tick_t i_start,
                                              const uint8_t *p_data, size_t i_data )
{
    while( i_data >= 2 )
    {
        uint8_t i_sid = p_data[0] >> 5;
        const uint8_t i_block_size = p_data[0] & 0x1F;

        if( i_block_size == 0 || i_block_size > i_data - 1 )
        {
            return;
        }
        else if( i_sid == 0x07 )
        {
            i_sid = p_data[1] & 0x3F;
            if( i_sid < 0x07 )
                return;
            p_data += 1; i_data -= 1;
        }
        p_data += 1; i_data -= 1;

        h->p_callback( h->priv, i_sid, i_start, p_data, i_block_size );

        p_data += i_block_size;
        i_data -= i_block_size;
    }
}

void CEA708_DTVCC_Demuxer_Push( cea708_demux_t *h, vlc_tick_t i_start, const uint8_t data[3] )
{
    if( (data[0] & 0x03) == 3 ) /* Header packet */
    {
        const int8_t i_pkt_sequence = data[1] >> 6;

        /* pkt loss/discontinuity, trash buffer */
        if( i_pkt_sequence > 0 && ((h->i_pkt_sequence + 1) % 4) != i_pkt_sequence )
        {
            h->i_data = h->i_total_data = 0;
            h->i_pkt_sequence = i_pkt_sequence;
            return;
        }

        uint8_t pktsize = data[1] & 63;
        if( pktsize == 0 )
            pktsize = 127;
        else
            pktsize = pktsize * 2 - 1;

        h->i_pkt_sequence = i_pkt_sequence;
        h->i_total_data = pktsize;
        h->i_data = 0;
        h->i_time = i_start;
        h->data[h->i_data++] = data[2];
    }
    else if( h->i_total_data > 0 ) /* Not synced to pkt header yet */
    {
        h->data[h->i_data++] = data[1];
        h->data[h->i_data++] = data[2];
    }

    /* pkts assembly finished, we have a service block */
    if( h->i_data > 0 && h->i_data >= h->i_total_data )
    {
        if( h->i_data == h->i_total_data ) /* Only if correct */
            CEA708_DTVCC_Demux_ServiceBlocks( h, h->i_time, h->data, h->i_data );
        h->i_total_data = h->i_data = 0;
    }
}

/*****************************************************************************
 * Service Data Decoding
 *****************************************************************************/

#define CEA708_SERVICE_INPUT_BUFFER    128

#define CEA708_WINDOWS_COUNT            8
#define CEA708_PREDEFINED_STYLES        8

#define CEA708_SCREEN_ROWS              75
#define CEA708_SCREEN_COLS_43           160
#define CEA708_SCREEN_COLS_169          210
#define CEA708_SCREEN_SAFE_MARGIN_RATIO 0.10
#define CEA708_SAFE_AREA_REL            (1.0 - CEA708_SCREEN_SAFE_MARGIN_RATIO)

#define CEA708_WINDOW_MAX_COLS          42
#define CEA708_WINDOW_MAX_ROWS          15

#define CEA708_ROW_HEIGHT_STANDARD     (CEA708_SAFE_AREA_REL / \
                                        CEA708_WINDOW_MAX_ROWS)
#define CEA708_FONT_TO_LINE_HEIGHT_RATIO 1.06

#define CEA708_FONTRELSIZE_STANDARD    (100.0 * CEA708_ROW_HEIGHT_STANDARD / \
                                        CEA708_FONT_TO_LINE_HEIGHT_RATIO)
#define CEA708_FONTRELSIZE_SMALL       (CEA708_FONTRELSIZE_STANDARD * 0.7)
#define CEA708_FONTRELSIZE_LARGE       (CEA708_FONTRELSIZE_STANDARD * 1.3)

enum cea708_status_e
{
    CEA708_STATUS_OK       = 1 << 0,
    CEA708_STATUS_STARVING = 1 << 1,
    CEA708_STATUS_OUTPUT   = 1 << 2,
};

enum cea708_c0_codes
{
    CEA708_C0_NUL   = 0x00,
    CEA708_C0_ETX   = 0x03,
    CEA708_C0_BS    = 0x08,
    CEA708_C0_FF    = 0x0C,
    CEA708_C0_CR    = 0x0D,
    CEA708_C0_HCR   = 0x0E,
    CEA708_C0_EXT1  = 0x10,
    CEA708_C0_P16   = 0x18,
};

enum cea708_c1_codes
{
    CEA708_C1_CW0   = 0x80,
    CEA708_C1_CW7   = 0x87,
    CEA708_C1_CLW,
    CEA708_C1_DSW,
    CEA708_C1_HDW,
    CEA708_C1_TGW,
    CEA708_C1_DLW,
    CEA708_C1_DLY,
    CEA708_C1_DLC,
    CEA708_C1_RST,
    CEA708_C1_SPA   = 0x90,
    CEA708_C1_SPC,
    CEA708_C1_SPL,
    CEA708_C1_SWA   = 0x97,
    CEA708_C1_DF0,
    CEA708_C1_DF7   = 0x9F,
};

typedef struct
{
    uint8_t ringbuffer[CEA708_SERVICE_INPUT_BUFFER];
    uint8_t start;
    uint8_t capacity;
} cea708_input_buffer_t;

static void cea708_input_buffer_init(cea708_input_buffer_t *ib)
{
    ib->capacity = 0;
    ib->start = 0;
}

static uint8_t cea708_input_buffer_size(const cea708_input_buffer_t *ib)
{
    return ib->capacity;
}

static uint8_t cea708_input_buffer_remain(const cea708_input_buffer_t *ib)
{
    return CEA708_SERVICE_INPUT_BUFFER - ib->capacity;
}

static void cea708_input_buffer_add(cea708_input_buffer_t *ib, uint8_t a)
{
    if( cea708_input_buffer_remain(ib) > 0 )
        ib->ringbuffer[(ib->start + ib->capacity++) % CEA708_SERVICE_INPUT_BUFFER] = a;
}

static uint8_t cea708_input_buffer_peek(cea708_input_buffer_t *ib, uint8_t off)
{
    if(off + 1 > ib->capacity)
        return 0;
    off = (ib->start + off) % CEA708_SERVICE_INPUT_BUFFER;
    return ib->ringbuffer[off];
}

static uint8_t cea708_input_buffer_get(cea708_input_buffer_t *ib)
{
    uint8_t a = cea708_input_buffer_peek( ib, 0 );
    ib->start = (ib->start + 1) % CEA708_SERVICE_INPUT_BUFFER;
    ib->capacity--;
    return a;
}

enum cea708_opacity_e
{
    CEA708_OPACITY_SOLID = 0,
    CEA708_OPACITY_FLASH,
    CEA708_OPACITY_TRANSLUCENT,
    CEA708_OPACITY_TRANSPARENT,
};

enum cea708_edge_e
{
    CEA708_EDGE_NONE =0,
    CEA708_EDGE_RAISED,
    CEA708_EDGE_DEPRESSED,
    CEA708_EDGE_UNIFORM,
    CEA708_EDGE_LEFT_DROP_SHADOW,
    CEA708_EDGE_RIGHT_DROP_SHADOW,
};

typedef struct
{
    enum
    {
        CEA708_PEN_SIZE_SMALL = 0,
        CEA708_PEN_SIZE_STANDARD,
        CEA708_PEN_SIZE_LARGE,
    } size;
    enum
    {
        CEA708_FONT_UNDEFINED = 0,
        CEA708_FONT_MONOSPACED,
        CEA708_FONT_PROP,
        CEA708_FONT_MONO_SANS_SERIF,
        CEA708_FONT_PROP_SANS_SERIF,
        CEA708_FONT_CASUAL,
        CEA708_FONT_CURSIVE,
        CEA708_FONT_SMALL_CAPS,
    } font;
    enum
    {
        CEA708_TAG_DIALOG = 0,
        CEA708_TAG_SPEAKER,
        CEA708_TAG_SYNTHETIC_VOICE,
        CEA708_TAG_DIALOG_SECONDARY_LANG,
        CEA708_TAG_VOICEOVER,
        CEA708_TAG_AUDIBLE_TRANSLATION,
        CEA708_TAG_SUBTITLE_TRANSLATION,
        CEA708_TAG_VOICE_QUALITY_DESCRIPTION,
        CEA708_TAG_SONG_LYRICS,
        CEA708_TAG_FX_DESCRIPTION,
        CEA708_TAG_SCORE_DESCRIPTION,
        CEA708_TAG_EXPLETIVE,
        CEA708_TAG_NOT_TO_BE_DISPLAYED = 15,
    } text_tag;
    enum
    {
        CEA708_PEN_OFFSET_SUBSCRIPT = 0,
        CEA708_PEN_OFFSET_NORMAL,
        CEA708_PEN_OFFSET_SUPERSCRIPT,
    } offset;
    bool b_italics;
    bool b_underline;
    struct
    {
        uint8_t color;
        enum cea708_opacity_e opacity;
    } foreground, background;
    uint8_t edge_color;
    enum cea708_edge_e edge_type;
} cea708_pen_style_t;

typedef struct
{
    cea708_pen_style_t style;
    uint8_t row;
    uint8_t col;
} cea708_pen_t;

typedef struct
{
    enum
    {
        CEA708_WA_JUSTIFY_LEFT = 0,
        CEA708_WA_JUSTIFY_RIGHT,
        CEA708_WA_JUSTIFY_CENTER,
        CEA708_WA_JUSTIFY_FULL,
    } justify;
    enum
    {
        CEA708_WA_DIRECTION_LTR = 0,
        CEA708_WA_DIRECTION_RTL,
        CEA708_WA_DIRECTION_TB,
        CEA708_WA_DIRECTION_BT,
    } print_direction, scroll_direction, effect_direction;
    bool b_word_wrap;
    enum
    {
        CEA708_WA_EFFECT_SNAP = 0,
        CEA708_WA_EFFECT_FADE,
        CEA708_WA_EFFECT_WIPE,
    } display_effect;
    uint8_t effect_speed;
    uint8_t fill_color_color;
    enum cea708_opacity_e fill_opacity;
    enum cea708_edge_e border_type;
    uint8_t border_color_color;
} cea708_window_style_t;

typedef struct cea708_text_row_t cea708_text_row_t;

struct cea708_text_row_t
{
    uint8_t characters[CEA708_WINDOW_MAX_COLS * 4];
    cea708_pen_style_t styles[CEA708_WINDOW_MAX_COLS];
    uint8_t firstcol;
    uint8_t lastcol;
};

static void cea708_text_row_Delete( cea708_text_row_t *p_row )
{
    free( p_row );
}

static cea708_text_row_t * cea708_text_row_New( void )
{
    cea708_text_row_t *p_row = malloc( sizeof(*p_row) );
    if( p_row )
    {
        p_row->firstcol = CEA708_WINDOW_MAX_COLS;
        p_row->lastcol = 0;
        memset(p_row->characters, 0, 4 * CEA708_WINDOW_MAX_COLS);
    }
    return p_row;
}

typedef struct
{
    cea708_text_row_t * rows[CEA708_WINDOW_MAX_ROWS];
    uint8_t i_firstrow;
    uint8_t i_lastrow;

    uint8_t i_priority;

    enum
    {
        CEA708_ANCHOR_TOP_LEFT = 0,
        CEA708_ANCHOR_TOP_CENTER,
        CEA708_ANCHOR_TOP_RIGHT,
        CEA708_ANCHOR_CENTER_LEFT,
        CEA708_ANCHOR_CENTER_CENTER,
        CEA708_ANCHOR_CENTER_RIGHT,
        CEA708_ANCHOR_BOTTOM_LEFT,
        CEA708_ANCHOR_BOTTOM_CENTER,
        CEA708_ANCHOR_BOTTOM_RIGHT,
    } anchor_point;
    uint8_t i_anchor_offset_v;
    uint8_t i_anchor_offset_h;

    /* Extras row for window scroll */
    uint8_t i_row_count;
    uint8_t i_col_count;

    /* flags */
    uint8_t b_relative;
    uint8_t b_row_lock;
    uint8_t b_column_lock;
    uint8_t b_visible;

    cea708_window_style_t style;
    cea708_pen_style_t    pen;

    uint8_t row;
    uint8_t col;

    bool b_defined;

} cea708_window_t;

struct cea708_t
{
    decoder_t *p_dec;

    /* Defaults */
    cea708_window_t window[CEA708_WINDOWS_COUNT];
    cea708_input_buffer_t input_buffer;

    /* Decoding context */
    cea708_window_t *p_cw; /* current window */
    vlc_tick_t suspended_deadline; /* not VLC_TICK_INVALID when delay is active */
    vlc_tick_t i_clock;
    bool b_text_waiting;
};

static int CEA708_Decode_G0( uint8_t code, cea708_t *p_cea708 );
static int CEA708_Decode_C0( uint8_t code, cea708_t *p_cea708 );
static int CEA708_Decode_G1( uint8_t code, cea708_t *p_cea708 );
static int CEA708_Decode_C1( uint8_t code, cea708_t *p_cea708 );
static int CEA708_Decode_G2G3( uint8_t code, cea708_t *p_cea708 );
static int CEA708_Decode_P16( uint16_t ucs2, cea708_t *p_cea708 );

#define DEFAULT_NTSC_STYLE(font, edge, bgopacity ) \
    {\
        CEA708_PEN_SIZE_STANDARD,\
        font,\
        CEA708_TAG_DIALOG,\
        CEA708_PEN_OFFSET_NORMAL,\
        false,\
        false,\
        {   0x2A,   CEA708_OPACITY_SOLID,   },\
        {   0x00,   bgopacity,              },\
        0x00,\
        edge,\
    }
static const cea708_pen_style_t cea708_default_pen_styles[CEA708_PREDEFINED_STYLES] =
{
    DEFAULT_NTSC_STYLE( CEA708_FONT_UNDEFINED,       CEA708_EDGE_NONE,    CEA708_OPACITY_SOLID ),
    DEFAULT_NTSC_STYLE( CEA708_FONT_MONOSPACED,      CEA708_EDGE_NONE,    CEA708_OPACITY_SOLID ),
    DEFAULT_NTSC_STYLE( CEA708_FONT_PROP,            CEA708_EDGE_NONE,    CEA708_OPACITY_SOLID ),
    DEFAULT_NTSC_STYLE( CEA708_FONT_MONO_SANS_SERIF, CEA708_EDGE_NONE,    CEA708_OPACITY_SOLID ),
    DEFAULT_NTSC_STYLE( CEA708_FONT_PROP_SANS_SERIF, CEA708_EDGE_NONE,    CEA708_OPACITY_SOLID ),
    DEFAULT_NTSC_STYLE( CEA708_FONT_MONO_SANS_SERIF, CEA708_EDGE_UNIFORM, CEA708_OPACITY_TRANSPARENT ),
    DEFAULT_NTSC_STYLE( CEA708_FONT_PROP_SANS_SERIF, CEA708_EDGE_UNIFORM, CEA708_OPACITY_TRANSPARENT ),
};
#undef DEFAULT_NTSC_STYLE

#define DEFAULT_NTSC_WA_STYLE(just, pd, scroll, wrap, opacity) \
    {\
        just,\
        pd,\
        scroll,\
        CEA708_WA_DIRECTION_LTR,\
        wrap,\
        CEA708_WA_EFFECT_SNAP,\
        1,\
        0x00,\
        opacity,\
        CEA708_EDGE_NONE,\
        0x00,\
    }
static const cea708_window_style_t cea708_default_window_styles[CEA708_PREDEFINED_STYLES] =
{
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_LEFT,   CEA708_WA_DIRECTION_LTR,
                          CEA708_WA_DIRECTION_BT,   false, CEA708_OPACITY_SOLID),
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_LEFT,   CEA708_WA_DIRECTION_LTR,
                          CEA708_WA_DIRECTION_BT,   false, CEA708_OPACITY_TRANSPARENT),
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_CENTER, CEA708_WA_DIRECTION_LTR,
                          CEA708_WA_DIRECTION_BT,   false, CEA708_OPACITY_SOLID),
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_LEFT,   CEA708_WA_DIRECTION_LTR,
                          CEA708_WA_DIRECTION_BT,   true,  CEA708_OPACITY_SOLID),
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_LEFT,   CEA708_WA_DIRECTION_LTR,
                          CEA708_WA_DIRECTION_BT,   true,  CEA708_OPACITY_TRANSPARENT),
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_CENTER, CEA708_WA_DIRECTION_LTR,
                          CEA708_WA_DIRECTION_BT,   true, CEA708_OPACITY_SOLID),
    DEFAULT_NTSC_WA_STYLE(CEA708_WA_JUSTIFY_LEFT,   CEA708_WA_DIRECTION_TB,
                          CEA708_WA_DIRECTION_RTL,  false, CEA708_OPACITY_SOLID),
};
#undef DEFAULT_NTSC_WA_STYLE

static void CEA708_Window_Init( cea708_window_t *p_w )
{
    memset( p_w, 0, sizeof(*p_w) );
    p_w->style = cea708_default_window_styles[0];
    p_w->pen = cea708_default_pen_styles[0];
    p_w->i_firstrow = CEA708_WINDOW_MAX_ROWS;
    p_w->b_row_lock = true;
    p_w->b_column_lock = true;
}

static void CEA708_Window_ClearText( cea708_window_t *p_w )
{
    for( uint8_t i=p_w->i_firstrow; i<=p_w->i_lastrow; i++ )
    {
        cea708_text_row_Delete( p_w->rows[i] );
        p_w->rows[i] = NULL;
    }
    p_w->i_lastrow = 0;
    p_w->i_firstrow = CEA708_WINDOW_MAX_ROWS;
}

static void CEA708_Window_Reset( cea708_window_t *p_w )
{
    CEA708_Window_ClearText( p_w );
    CEA708_Window_Init( p_w );
}

static bool CEA708_Window_BreaksSpace( const cea708_window_t *p_w )
{
    return true;
    if( p_w->style.print_direction == CEA708_WA_DIRECTION_LTR &&
        p_w->style.justify == CEA708_WA_JUSTIFY_LEFT )
        return true;

    if( p_w->style.print_direction == CEA708_WA_DIRECTION_RTL &&
        p_w->style.justify == CEA708_WA_JUSTIFY_RIGHT )
        return true;

    return false;
}

static uint8_t CEA708_Window_MinCol( const cea708_window_t *p_w )
{
    uint8_t i_min = CEA708_WINDOW_MAX_COLS;
    for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
    {
        const cea708_text_row_t *p_row = p_w->rows[p_w->row];
        if( p_row && p_row->firstcol < i_min )
            i_min = p_row->firstcol;
    }
    return i_min;
}

static uint8_t CEA708_Window_MaxCol( const cea708_window_t *p_w )
{
    uint8_t i_max = 0;
    for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
    {
        const cea708_text_row_t *p_row = p_w->rows[p_w->row];
        if( p_row && p_row->lastcol > i_max )
            i_max = p_row->lastcol;
    }
    return i_max;
}

static uint8_t CEA708_Window_ColCount( const cea708_window_t *p_w )
{
    const cea708_text_row_t *p_row = p_w->rows[p_w->row];
    if( !p_row || p_row->firstcol > p_row->lastcol )
        return 0;
    return 1 + p_row->lastcol - p_row->firstcol;
}

static uint8_t CEA708_Window_RowCount( const cea708_window_t *p_w )
{
    if( p_w->i_firstrow > p_w->i_lastrow )
        return 0;
    return 1 + p_w->i_lastrow - p_w->i_firstrow;
}

static void CEA708_Window_Truncate( cea708_window_t *p_w, int i_direction )
{
    switch( i_direction )
    {
        case CEA708_WA_DIRECTION_LTR: /* Deletes all most right col */
        {
            uint8_t i_max = CEA708_Window_MaxCol( p_w );
            for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
            {
                cea708_text_row_t *row = p_w->rows[i];
                if( row->lastcol == i_max )
                {
                    if( row->firstcol >= row->lastcol )
                    {
                        cea708_text_row_Delete( row );
                        p_w->rows[i] = NULL;
                        if( i == p_w->i_firstrow )
                            p_w->i_firstrow++;
                        else if( i == p_w->i_lastrow )
                            p_w->i_lastrow--;
                    }
                }
            }
        }
            break;
        case CEA708_WA_DIRECTION_RTL: /* Deletes all most left col */
        {
            uint8_t i_min = CEA708_Window_MinCol( p_w );
            for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
            {
                cea708_text_row_t *row = p_w->rows[i];
                if( row->firstcol == i_min )
                {
                    if( row->firstcol >= row->lastcol )
                    {
                        cea708_text_row_Delete( row );
                        p_w->rows[i] = NULL;
                        if( i == p_w->i_firstrow )
                            p_w->i_firstrow++;
                        else if( i == p_w->i_lastrow )
                            p_w->i_lastrow--;
                    }
                }
            }
        }
            break;
        case CEA708_WA_DIRECTION_TB: /* Deletes LAST row */
            if( CEA708_Window_RowCount( p_w ) > 0 )
            {
                cea708_text_row_Delete( p_w->rows[p_w->i_lastrow] );
                p_w->rows[p_w->i_lastrow--] = NULL;
            }
            break;
        case CEA708_WA_DIRECTION_BT: /* Deletes First row */
            if( CEA708_Window_RowCount( p_w ) > 0 )
            {
                cea708_text_row_Delete( p_w->rows[p_w->i_firstrow] );
                p_w->rows[p_w->i_firstrow++] = NULL;
            }
            break;
    }
}

static void CEA708_Window_Scroll( cea708_window_t *p_w )
{
    if( CEA708_Window_RowCount( p_w ) == 0 )
        return;

    switch( p_w->style.scroll_direction )
    {
        case CEA708_WA_DIRECTION_LTR:
            /* Move RIGHT */
            if( CEA708_Window_MaxCol( p_w ) == CEA708_WINDOW_MAX_ROWS - 1 )
                CEA708_Window_Truncate( p_w, CEA708_WA_DIRECTION_LTR );
            for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
            {
                cea708_text_row_t *row = p_w->rows[i];
                if( row->lastcol < row->firstcol ) /* should not happen */
                    continue;
                memmove( &row->characters[row->firstcol + 1], &row->characters[row->firstcol],
                         (row->lastcol - row->firstcol + 1) * 4U );
                memmove( &row->styles[row->firstcol + 1], &row->styles[row->firstcol],
                         (row->lastcol - row->firstcol + 1) * sizeof(cea708_pen_style_t) );
                row->firstcol++;
                row->lastcol++;
            }
            break;
        case CEA708_WA_DIRECTION_RTL:
            /* Move LEFT */
            if( CEA708_Window_MinCol( p_w ) == 0 )
                CEA708_Window_Truncate( p_w, CEA708_WA_DIRECTION_RTL );
            for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
            {
                cea708_text_row_t *row = p_w->rows[i];
                if( row->lastcol < row->firstcol ) /* should not happen */
                    continue;
                memmove( &row->characters[row->firstcol - 1], &row->characters[row->firstcol],
                         (row->lastcol - row->firstcol + 1) * 4U );
                memmove( &row->styles[row->firstcol - 1], &row->styles[row->firstcol],
                         (row->lastcol - row->firstcol + 1) * sizeof(cea708_pen_style_t) );
                row->firstcol--;
                row->lastcol--;
            }
            break;
        case CEA708_WA_DIRECTION_TB:
            /* Move DOWN */
            if( p_w->i_lastrow == CEA708_WINDOW_MAX_ROWS - 1 )
                CEA708_Window_Truncate( p_w, CEA708_WA_DIRECTION_TB );
            for( int i=p_w->i_lastrow; i >= p_w->i_firstrow; i-- )
                p_w->rows[i+1] = p_w->rows[i];
            p_w->rows[p_w->i_firstrow] = NULL;
            p_w->i_firstrow++;
            p_w->i_lastrow++;
            break;
        case CEA708_WA_DIRECTION_BT:
            /* Move UP */
            if( p_w->i_firstrow == 0 )
                CEA708_Window_Truncate( p_w, CEA708_WA_DIRECTION_BT );
            for( int i=p_w->i_firstrow; i <= p_w->i_lastrow; i++ )
                p_w->rows[i-1] = p_w->rows[i];
            p_w->rows[p_w->i_lastrow] = NULL;
            p_w->i_firstrow--;
            p_w->i_lastrow--;
            break;
    }
}

static void CEA708_Window_CarriageReturn( cea708_window_t *p_w )
{
    switch( p_w->style.scroll_direction )
    {
        case CEA708_WA_DIRECTION_LTR:
            if( p_w->col > 0 &&
                CEA708_Window_ColCount( p_w ) < p_w->i_col_count )
                p_w->col--;
            else
                CEA708_Window_Scroll( p_w );
            p_w->row = (p_w->style.print_direction == CEA708_WA_DIRECTION_TB) ?
                       0 : CEA708_WINDOW_MAX_ROWS - 1;
            break;
        case CEA708_WA_DIRECTION_RTL:
            if( p_w->col + 1 < CEA708_WINDOW_MAX_COLS &&
                CEA708_Window_ColCount( p_w ) < p_w->i_col_count )
                p_w->col++;
            else
                CEA708_Window_Scroll( p_w );
            p_w->row = (p_w->style.print_direction == CEA708_WA_DIRECTION_TB) ?
                       0 : CEA708_WINDOW_MAX_ROWS - 1;
            break;
        case CEA708_WA_DIRECTION_TB:
            if( p_w->row > 0 &&
                CEA708_Window_RowCount( p_w ) < p_w->i_row_count )
                p_w->row--;
            else
                CEA708_Window_Scroll( p_w );
            p_w->col = (p_w->style.print_direction == CEA708_WA_DIRECTION_LTR) ?
                       0 : CEA708_WINDOW_MAX_COLS - 1;
            break;
        case CEA708_WA_DIRECTION_BT:
            if( p_w->row + 1 < p_w->i_row_count )
                p_w->row++;
            else
                CEA708_Window_Scroll( p_w );
            p_w->col = (p_w->style.print_direction == CEA708_WA_DIRECTION_LTR) ?
                       0 : CEA708_WINDOW_MAX_COLS - 1;
            break;
    }
}

static void CEA708_Window_Forward( cea708_window_t *p_w )
{
    switch( p_w->style.print_direction )
    {
        case CEA708_WA_DIRECTION_LTR:
            if( p_w->col + 1 < CEA708_WINDOW_MAX_COLS )
                p_w->col++;
            else
                CEA708_Window_CarriageReturn( p_w );
            break;
        case CEA708_WA_DIRECTION_RTL:
            if( p_w->col > 0 )
                p_w->col--;
            else
                CEA708_Window_CarriageReturn( p_w );
            break;
        case CEA708_WA_DIRECTION_TB:
            if( p_w->row + 1 < CEA708_WINDOW_MAX_ROWS )
                p_w->row++;
            else
                CEA708_Window_CarriageReturn( p_w );
            break;
        case CEA708_WA_DIRECTION_BT:
            if( p_w->row > 0 )
                p_w->row--;
            else
                CEA708_Window_CarriageReturn( p_w );
            break;
    }
}

static void CEA708_Window_Backward( cea708_window_t *p_w )
{
    static const int reverse[] =
    {
        [CEA708_WA_DIRECTION_LTR] = CEA708_WA_DIRECTION_RTL,
        [CEA708_WA_DIRECTION_RTL] = CEA708_WA_DIRECTION_LTR,
        [CEA708_WA_DIRECTION_TB]  = CEA708_WA_DIRECTION_BT,
        [CEA708_WA_DIRECTION_BT]  = CEA708_WA_DIRECTION_TB,
    };
    int save = p_w->style.print_direction;
    p_w->style.print_direction = reverse[p_w->style.print_direction];
    CEA708_Window_Forward( p_w );
    p_w->style.print_direction = save;
}

static void CEA708_Window_Write( const uint8_t c[4], cea708_window_t *p_w )
{
    if( !p_w->b_defined )
        return;


    if( unlikely( p_w->row >= CEA708_WINDOW_MAX_ROWS ||
                  p_w->col >= CEA708_WINDOW_MAX_COLS ) )
    {
        assert( p_w->row < CEA708_WINDOW_MAX_ROWS );
        assert( p_w->col < CEA708_WINDOW_MAX_COLS );
        return;
    }

    cea708_text_row_t *p_row = p_w->rows[p_w->row];
    if( !p_row )
    {
        p_w->rows[p_w->row] = p_row = cea708_text_row_New();
        if( !p_row )
            return;
        if( p_w->row < p_w->i_firstrow )
            p_w->i_firstrow = p_w->row;
        if( p_w->row > p_w->i_lastrow )
            p_w->i_lastrow = p_w->row;
    }

    memcpy( &p_row->characters[p_w->col * 4U], c, 4 );
    p_row->styles[p_w->col] = p_w->pen;
    if( p_w->col < p_row->firstcol )
        p_row->firstcol = p_w->col;
    if( p_w->col > p_row->lastcol )
        p_row->lastcol = p_w->col;

    CEA708_Window_Forward( p_w );

    Debug(printf("\033[0;33m%s\033[0m", c));
}

static uint32_t CEA708ColorConvert( uint8_t c )
{
    const uint32_t value[4] = {0x00,0x3F,0xF0,0xFF};
    c = c & 0x3F;
    return (value[(c >> 4) & 0x03] << 16) |
           (value[(c >> 2) & 0x03] << 8) |
           value[c & 0x03];
}

static uint8_t CEA708AlphaConvert( uint8_t c )
{
    if( c == CEA708_OPACITY_TRANSLUCENT )
        return STYLE_ALPHA_OPAQUE / 2;
    else if( c == CEA708_OPACITY_TRANSPARENT )
        return STYLE_ALPHA_TRANSPARENT;
    else
        return STYLE_ALPHA_OPAQUE;
}

static void CEA708PenStyleToSegment( const cea708_pen_style_t *ps, text_style_t *s )
{
    if( ps->background.opacity != CEA708_OPACITY_TRANSPARENT )
    {
        s->i_background_alpha = CEA708AlphaConvert( ps->background.opacity );
        s->i_style_flags |= STYLE_BACKGROUND;
        s->i_background_color = CEA708ColorConvert( ps->background.color );
        s->i_features |= STYLE_HAS_BACKGROUND_COLOR|STYLE_HAS_BACKGROUND_ALPHA;
        if( ps->background.opacity == CEA708_OPACITY_FLASH )
            s->i_style_flags |= STYLE_BLINK_BACKGROUND;
    }
    s->i_font_color = CEA708ColorConvert( ps->foreground.color );
    s->i_font_alpha = CEA708AlphaConvert( ps->foreground.opacity );
    s->i_features |= STYLE_HAS_FONT_ALPHA|STYLE_HAS_FONT_COLOR;
    if( ps->foreground.opacity == CEA708_OPACITY_FLASH )
        s->i_style_flags |= STYLE_BLINK_FOREGROUND;

    if( ps->b_italics )
        s->i_style_flags |= STYLE_ITALIC;
    if( ps->b_underline )
        s->i_style_flags |= STYLE_UNDERLINE;

    switch( ps->font )
    {
        default:
        case CEA708_FONT_UNDEFINED:
        case CEA708_FONT_MONOSPACED:
        case CEA708_FONT_MONO_SANS_SERIF:
            s->i_style_flags |= STYLE_MONOSPACED;
            break;
        case CEA708_FONT_PROP:
        case CEA708_FONT_PROP_SANS_SERIF:
        case CEA708_FONT_CASUAL:
        case CEA708_FONT_CURSIVE:
        case CEA708_FONT_SMALL_CAPS:
            break;
    }

    switch( ps->size )
    {
        case CEA708_PEN_SIZE_SMALL:
            s->f_font_relsize = CEA708_FONTRELSIZE_SMALL;
            break;
        case CEA708_PEN_SIZE_LARGE:
            s->f_font_relsize = CEA708_FONTRELSIZE_LARGE;
            break;
        default:
            s->f_font_relsize = CEA708_FONTRELSIZE_STANDARD;
            break;
    }
}

static text_segment_t * CEA708CharsToSegment( const cea708_text_row_t *p_row,
                                              uint8_t i_start, uint8_t i_end,
                                              bool b_newline )
{
    text_segment_t *p_segment = text_segment_New( NULL );
    if( !p_segment )
        return NULL;

    p_segment->style = text_style_Create( STYLE_NO_DEFAULTS );
    if( p_segment->style )
        CEA708PenStyleToSegment( &p_row->styles[i_start], p_segment->style );

    p_segment->psz_text = malloc( 1U + !!b_newline + (i_end - i_start + 1) * 4U );
    if( !p_segment->psz_text )
    {
        text_segment_Delete( p_segment );
        return NULL;
    }

    size_t offsetw = 0;
    for( uint8_t i=i_start; i<=i_end; i++ )
    {
        for( size_t j=0; j<4; j++ )
        {
            if( p_row->characters[i * 4 + j] != 0 )
                p_segment->psz_text[offsetw++] = p_row->characters[i * 4 + j];
            else if( j == 0 )
                p_segment->psz_text[offsetw++] = ' ';
            else
                break;
        }
    }

    if( b_newline )
        p_segment->psz_text[offsetw++] = '\n';
    p_segment->psz_text[offsetw] = '\0';

    return p_segment;
}

static text_segment_t * CEA708RowToSegments( const cea708_text_row_t *p_row,
                                             bool b_addnewline )
{
    text_segment_t *p_segments = NULL;
    text_segment_t **pp_last = &p_segments;

    uint8_t i_start = p_row->firstcol;
    for( uint8_t i=i_start; i<=p_row->lastcol; i++ )
    {
        if( i == p_row->lastcol ||
            memcmp( &p_row->styles[i], &p_row->styles[i+1], sizeof(cea708_pen_style_t) ) )
        {
            *pp_last = CEA708CharsToSegment( p_row, i_start, i,
                                             b_addnewline && (i == p_row->lastcol) );
            if( *pp_last )
                pp_last  = &((*pp_last)->p_next);
            i_start = i+1;
        }
    }

    return p_segments;
}

static void CEA708SpuConvert( const cea708_window_t *p_w,
                              substext_updater_region_t *p_region )
{
    if( !p_w->b_visible || CEA708_Window_RowCount( p_w ) == 0 )
        return;

    if( p_region == NULL && !(p_region = SubpictureUpdaterSysRegionNew()) )
        return;

    int first, last;

    if (p_w->style.scroll_direction == CEA708_WA_DIRECTION_BT) {
        /* BT is a bit of a special case since we need to grab the last N
           rows between first and last, rather than the first... */
        last = p_w->i_lastrow;
        if (p_w->i_lastrow - p_w->i_row_count < p_w->i_firstrow)
            first = p_w->i_firstrow;
        else
            first = p_w->i_lastrow - p_w->i_row_count + 1;

    } else {
        first = p_w->i_firstrow;
        if (p_w->i_firstrow + p_w->i_row_count > p_w->i_lastrow)
            last = p_w->i_lastrow;
        else
            last = p_w->i_firstrow + p_w->i_row_count - 1;
    }

    text_segment_t **pp_last = &p_region->p_segments;
    for( uint8_t i=first; i<=last; i++ )
    {
        if( !p_w->rows[i] )
            continue;

        *pp_last = CEA708RowToSegments( p_w->rows[i], i < p_w->i_lastrow );
        if( *pp_last )
            pp_last  = &((*pp_last)->p_next);
    }

    if( p_w->b_relative )
    {
        /* FIXME: take into account left/right anchors */
        p_region->origin.x = p_w->i_anchor_offset_h / 100.0;

        switch (p_w->anchor_point) {
        case CEA708_ANCHOR_TOP_LEFT:
        case CEA708_ANCHOR_TOP_CENTER:
        case CEA708_ANCHOR_TOP_RIGHT:
            p_region->origin.y = p_w->i_anchor_offset_v / 100.0;
            break;
        case CEA708_ANCHOR_BOTTOM_LEFT:
        case CEA708_ANCHOR_BOTTOM_CENTER:
        case CEA708_ANCHOR_BOTTOM_RIGHT:
            p_region->origin.y = 1.0 - (p_w->i_anchor_offset_v / 100.0);
            break;
        default:
            /* FIXME: for CENTER vertical justified, just position as top */
            p_region->origin.y = p_w->i_anchor_offset_v / 100.0;
            break;
        }
    }
    else
    {
        p_region->origin.x = (float)p_w->i_anchor_offset_h / CEA708_SCREEN_COLS_169;
        p_region->origin.y = (float)p_w->i_anchor_offset_v /
                             (CEA708_SCREEN_ROWS * CEA708_FONT_TO_LINE_HEIGHT_RATIO);
    }
    p_region->flags |= UPDT_REGION_ORIGIN_X_IS_RATIO|UPDT_REGION_ORIGIN_Y_IS_RATIO;

    if( p_w->i_firstrow <= p_w->i_lastrow )
    {
        p_region->origin.y += p_w->i_firstrow * CEA708_ROW_HEIGHT_STANDARD;
        /*const uint8_t i_min = CEA708_Window_MinCol( p_w );
        if( i_min < CEA708_WINDOW_MAX_COLS )
            p_region->origin.x += (float) i_min / CEA708_WINDOW_MAX_COLS;*/
    }

    if( p_w->anchor_point <= CEA708_ANCHOR_BOTTOM_RIGHT )
    {
        static const int vlc_subpicture_aligns[] =
        {
            [CEA708_ANCHOR_TOP_LEFT]        = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT,
            [CEA708_ANCHOR_TOP_CENTER]      = SUBPICTURE_ALIGN_TOP,
            [CEA708_ANCHOR_TOP_RIGHT]       = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_RIGHT,
            [CEA708_ANCHOR_CENTER_LEFT]     = SUBPICTURE_ALIGN_LEFT,
            [CEA708_ANCHOR_CENTER_CENTER]   = 0,
            [CEA708_ANCHOR_CENTER_RIGHT]    = SUBPICTURE_ALIGN_RIGHT,
            [CEA708_ANCHOR_BOTTOM_LEFT]     = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_LEFT,
            [CEA708_ANCHOR_BOTTOM_CENTER]   = SUBPICTURE_ALIGN_BOTTOM,
            [CEA708_ANCHOR_BOTTOM_RIGHT]    = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT,
        };
        p_region->align = vlc_subpicture_aligns[p_w->anchor_point];
    }
    p_region->inner_align = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_LEFT;
}

static subpicture_t *CEA708_BuildSubtitle( cea708_t *p_cea708 )
{
    subpicture_t *p_spu = decoder_NewSubpictureText( p_cea708->p_dec );
    if( !p_spu )
        return NULL;

    subtext_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;
    substext_updater_region_t *p_region = &p_spu_sys->region;

    p_spu_sys->margin_ratio = CEA708_SCREEN_SAFE_MARGIN_RATIO;

    bool first = true;

    for(size_t i=0; i<CEA708_WINDOWS_COUNT; i++)
    {
        cea708_window_t *p_w = &p_cea708->window[i];
        if( p_w->b_defined && p_w->b_visible && CEA708_Window_RowCount( p_w ) )
        {
            if( !first )
            {
                substext_updater_region_t *p_newregion =
                        SubpictureUpdaterSysRegionNew();
                if( p_newregion == NULL )
                    break;
                SubpictureUpdaterSysRegionAdd( p_region, p_newregion );
                p_region = p_newregion;
            }
            first = false;

            /* Fill region */
            CEA708SpuConvert( p_w, p_region );
        }
    }

    p_spu->i_start    = p_cea708->i_clock;
    p_spu->i_stop     = p_cea708->i_clock + VLC_TICK_FROM_SEC(10);   /* 10s max */

    p_spu->b_ephemer  = true;
    p_spu->b_absolute = false;
    p_spu->b_subtitle = true;

    return p_spu;
}

static void CEA708_Decoder_Init( cea708_t *p_cea708 )
{
    cea708_input_buffer_init( &p_cea708->input_buffer );
    for(size_t i=0; i<CEA708_WINDOWS_COUNT; i++)
        CEA708_Window_Init( &p_cea708->window[i] );
    p_cea708->p_cw = &p_cea708->window[0];
    p_cea708->suspended_deadline = VLC_TICK_INVALID;
    p_cea708->b_text_waiting = false;
    p_cea708->i_clock = 0;
}

static void CEA708_Decoder_Reset( cea708_t *p_cea708 )
{
    for(size_t i=0; i<CEA708_WINDOWS_COUNT; i++)
        CEA708_Window_Reset( &p_cea708->window[i] );
    CEA708_Decoder_Init( p_cea708 );
}

void CEA708_Decoder_Flush( cea708_t *p_cea708 )
{
    CEA708_Decoder_Reset( p_cea708 );
}

void CEA708_Decoder_Release( cea708_t *p_cea708 )
{
    CEA708_Decoder_Reset( p_cea708 );
    free( p_cea708 );
}

cea708_t * CEA708_Decoder_New( decoder_t *p_dec )
{
    cea708_t *p_cea708 = malloc( sizeof(cea708_t) );
    if( p_cea708 )
    {
        CEA708_Decoder_Init( p_cea708 );
        p_cea708->p_dec = p_dec;
    }
    return p_cea708;
}

#define POP_COMMAND() (void) cea708_input_buffer_get( ib )
#define POP_ARGS(n) for(size_t pops=0; pops<(size_t)n;pops++) POP_COMMAND()
#define REQUIRE_ARGS(n) if(cea708_input_buffer_size( ib ) < n + 1)\
                            return CEA708_STATUS_STARVING
#define REQUIRE_ARGS_AND_POP_COMMAND(n) REQUIRE_ARGS(n); else POP_COMMAND()

static void CEA708_Output( cea708_t *p_cea708 )
{
    Debug(printf("@%ld ms\n", MS_FROM_VLC_TICK(p_cea708->i_clock)));
    subpicture_t *p_spu = CEA708_BuildSubtitle( p_cea708 );
    if( p_spu )
        decoder_QueueSub( p_cea708->p_dec, p_spu );
}

static int CEA708_Decode_C0( uint8_t code, cea708_t *p_cea708 )
{
    uint8_t v, i;
    uint16_t u16;
    cea708_input_buffer_t *ib = &p_cea708->input_buffer;
    int i_ret = CEA708_STATUS_OK;

    switch( code )
    {
        case CEA708_C0_NUL:
            POP_COMMAND();
            break;
        case CEA708_C0_ETX:
            POP_COMMAND();
            if( p_cea708->b_text_waiting )
            {
                i_ret |= CEA708_STATUS_OUTPUT;
                p_cea708->b_text_waiting = false;
            }
            break;
        case CEA708_C0_BS:
            POP_COMMAND();
            if( !p_cea708->p_cw->b_defined )
                break;
            CEA708_Window_Backward( p_cea708->p_cw );
            p_cea708->b_text_waiting = true;
            break;
        case CEA708_C0_FF:
            POP_COMMAND();
            if( !p_cea708->p_cw->b_defined )
                break;
            CEA708_Window_ClearText( p_cea708->p_cw );
            p_cea708->p_cw->col = 0;
            p_cea708->p_cw->row = 0;
            p_cea708->b_text_waiting = true;
            break;
        case CEA708_C0_CR:
            POP_COMMAND();
            if( !p_cea708->p_cw->b_defined )
                break;
            if( p_cea708->p_cw->style.print_direction <= CEA708_WA_DIRECTION_RTL )
            {
                CEA708_Window_CarriageReturn( p_cea708->p_cw );
                if( p_cea708->p_cw->b_visible )
                    i_ret |= CEA708_STATUS_OUTPUT;
            }
            break;
        case CEA708_C0_HCR:
            POP_COMMAND();
            if( !p_cea708->p_cw->b_defined )
                break;
            if( p_cea708->p_cw->style.print_direction > CEA708_WA_DIRECTION_RTL )
            {
                CEA708_Window_CarriageReturn( p_cea708->p_cw );
                if( p_cea708->p_cw->b_visible )
                    i_ret |= CEA708_STATUS_OUTPUT;
            }
            break;
        case CEA708_C0_EXT1: /* Special extended table case */
            if( cea708_input_buffer_size( ib ) >= 2 )
            {
                v = cea708_input_buffer_peek( ib, 1 );
                /* C2 extended code set */
                if( v < 0x20 )
                {
                    if( v > 0x17 )
                        i = 3;
                    else if( v > 0x0f )
                        i = 2;
                    else if( v > 0x07 )
                        i = 1;
                    else
                        i = 0;
                    if( cea708_input_buffer_size( ib ) < 2 + i )
                        return CEA708_STATUS_STARVING;
                    POP_COMMAND();
                    POP_ARGS(1 + i);
                }
                /* C3 extended code set */
                else if( v > 0x7f && v < 0xa0 )
                {
                    if( v > 0x87 )
                        i = 5;
                    else
                        i = 4;
                    if( cea708_input_buffer_size( ib ) < 2 + i )
                        return CEA708_STATUS_STARVING;
                    POP_COMMAND();
                    POP_ARGS(1 + i);
                }
                else
                {
                    POP_COMMAND();
                    v = cea708_input_buffer_get( ib );
                    if( p_cea708->p_cw->b_defined )
                        i_ret |= CEA708_Decode_G2G3( v, p_cea708 );
                }
            }
            else return CEA708_STATUS_STARVING;
            break;
        case CEA708_C0_P16:
            REQUIRE_ARGS_AND_POP_COMMAND(2);
            u16 = cea708_input_buffer_get( ib ) << 8;
            u16 |= cea708_input_buffer_get( ib );
            i_ret |= CEA708_Decode_P16( u16, p_cea708 );
            Debug(printf("[P16 %x]", u16));
            break;
        default:
            POP_COMMAND();
            Debug(printf("[UNK %2.2x]", code));
            break;
    }
    Debug(printf("[C0 %x]", code));
    return i_ret;
}

static int CEA708_Decode_G0( uint8_t code, cea708_t *p_cea708 )
{
    cea708_input_buffer_t *ib = &p_cea708->input_buffer;
    POP_COMMAND();
    int i_ret = CEA708_STATUS_OK;

    if( !p_cea708->p_cw->b_defined )
        return i_ret;

    uint8_t utf8[4] = {code,0x00,0x00,0x00};

    if(code == 0x7F) // Music note
    {
        utf8[0] = 0xe2;
        utf8[1] = 0x99;
        utf8[2] = 0xaa;
    }

    CEA708_Window_Write( utf8, p_cea708->p_cw );

    if( code == 0x20 &&
        p_cea708->b_text_waiting &&
        CEA708_Window_BreaksSpace( p_cea708->p_cw ) )
    {
        i_ret |= CEA708_STATUS_OUTPUT;
    }


    p_cea708->b_text_waiting |= p_cea708->p_cw->b_visible;

    return i_ret;
}

static int CEA708_Decode_C1( uint8_t code, cea708_t *p_cea708 )
{
    uint8_t v, i;
    cea708_input_buffer_t *ib = &p_cea708->input_buffer;
    int i_ret = CEA708_STATUS_OK;

    if( p_cea708->b_text_waiting )
    {
        i_ret |= CEA708_STATUS_OUTPUT;
        p_cea708->b_text_waiting = false;
    }

    switch( code )
    {
        case CEA708_C1_CLW:
            REQUIRE_ARGS_AND_POP_COMMAND(1);
            Debug(printf("[CLW"));
            for( i = 0, v = cea708_input_buffer_get( ib ); v; v = v >> 1, i++ )
                if( v & 1 )
                {
                    if( p_cea708->window[i].b_defined &&
                        p_cea708->window[i].b_visible )
                        i_ret |= CEA708_STATUS_OUTPUT;
                    CEA708_Window_ClearText( &p_cea708->window[i] );
                    Debug(printf("%d", i));
                }
            Debug(printf("]"));
            break;
        case CEA708_C1_DSW:
            REQUIRE_ARGS_AND_POP_COMMAND(1);
            Debug(printf("[DSW"));
            for( i = 0, v = cea708_input_buffer_get( ib ); v; v = v >> 1, i++ )
                if( v & 1 )
                {
                    if( p_cea708->window[i].b_defined )
                    {
                        if( !p_cea708->window[i].b_visible )
                            i_ret |= CEA708_STATUS_OUTPUT;
                        p_cea708->window[i].b_visible = true;
                    }
                    Debug(printf("%d", i));
                }
            Debug(printf("]"));
            break;
        case CEA708_C1_HDW:
            REQUIRE_ARGS_AND_POP_COMMAND(1);
            Debug(printf("[HDW"));
            for( i = 0, v = cea708_input_buffer_get( ib ); v; v = v >> 1, i++ )
                if( v & 1 )
                {
                    if( p_cea708->window[i].b_defined )
                    {
                        if( p_cea708->window[i].b_visible )
                            i_ret |= CEA708_STATUS_OUTPUT;
                        p_cea708->window[i].b_visible = false;
                    }
                    Debug(printf("%d", i));
                }
            Debug(printf("]"));
            break;
        case CEA708_C1_TGW:
            REQUIRE_ARGS_AND_POP_COMMAND(1);
            Debug(printf("[TGW"));
            for( i = 0, v = cea708_input_buffer_get( ib ); v; v = v >> 1, i++ )
                if( v & 1 )
                {
                    if( p_cea708->window[i].b_defined )
                    {
                        i_ret |= CEA708_STATUS_OUTPUT;
                        p_cea708->window[i].b_visible = !p_cea708->window[i].b_visible;
                    }
                    Debug(printf("%d", i));
                }
            Debug(printf("]"));
            break;
        case CEA708_C1_DLW:
            REQUIRE_ARGS_AND_POP_COMMAND(1);
            Debug(printf("[DLW"));
            for( i = 0, v = cea708_input_buffer_get( ib ); v; v = v >> 1, i++ )
                if( v & 1 )
                {
                    if( p_cea708->window[i].b_defined )
                    {
                        if( p_cea708->window[i].b_visible )
                            i_ret |= CEA708_STATUS_OUTPUT;
                        CEA708_Window_Reset( &p_cea708->window[i] );
                    }
                    Debug(printf("%d", i));
                }
            Debug(printf("]"));
            break;
        case CEA708_C1_DLY:
            REQUIRE_ARGS_AND_POP_COMMAND(1);
            p_cea708->suspended_deadline = p_cea708->i_clock +
                    VLC_TICK_FROM_MS( cea708_input_buffer_get( ib ) * 100 );
            Debug(printf("[DLY]"));
            break;
        case CEA708_C1_DLC:
            POP_COMMAND();
            p_cea708->suspended_deadline = VLC_TICK_INVALID;
            Debug(printf("[DLC]"));
            break;
        case CEA708_C1_RST:
            POP_COMMAND();
            i_ret |= CEA708_STATUS_OUTPUT;
            /* FIXME */
            break;
        case CEA708_C1_SPA:
            REQUIRE_ARGS_AND_POP_COMMAND(2);
            if( !p_cea708->p_cw->b_defined )
            {
                POP_ARGS(2);
                break;
            }
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->pen.text_tag = v >> 4;
            p_cea708->p_cw->pen.offset = (v >> 2) & 0x03;
            p_cea708->p_cw->pen.size = v & 0x03;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->pen.b_italics = v & 0x80;
            p_cea708->p_cw->pen.b_underline = v & 0x40;
            p_cea708->p_cw->pen.edge_type = (v >> 3) & 0x07;
            p_cea708->p_cw->pen.font = v & 0x07;
            Debug(printf("[SPA]"));
            break;
        case CEA708_C1_SPC:
            REQUIRE_ARGS_AND_POP_COMMAND(3);
            if( !p_cea708->p_cw->b_defined )
            {
                POP_ARGS(3);
                break;
            }
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->pen.foreground.opacity = v >> 6;
            p_cea708->p_cw->pen.foreground.color = v & 0x3F;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->pen.background.opacity = v >> 6;
            p_cea708->p_cw->pen.background.color = v & 0x3F;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->pen.edge_color = v & 0x3F;
            Debug(printf("[SPC]"));
            break;
        case CEA708_C1_SPL:
            REQUIRE_ARGS_AND_POP_COMMAND(2);
            if( !p_cea708->p_cw->b_defined )
            {
                POP_ARGS(2);
                break;
            }
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->row = (v & 0x0F) % CEA708_WINDOW_MAX_ROWS;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->col = (v & 0x3F) % CEA708_WINDOW_MAX_COLS;
            Debug(printf("[SPL r%d c%d]", p_cea708->p_cw->row, p_cea708->p_cw->col));
            break;
        case CEA708_C1_SWA:
            REQUIRE_ARGS_AND_POP_COMMAND(4);
            if( !p_cea708->p_cw->b_defined )
            {
                POP_ARGS(4);
                break;
            }
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->style.fill_opacity = v >> 6;
            p_cea708->p_cw->style.fill_color_color = v & 0x3F;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->style.border_color_color = v & 0x3F;
            p_cea708->p_cw->style.border_type = v >> 6;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->style.border_type |= ((v & 0x80) >> 5);
            p_cea708->p_cw->style.b_word_wrap = v & 0x40;
            p_cea708->p_cw->style.print_direction = (v >> 4) & 0x03;
            p_cea708->p_cw->style.scroll_direction = (v >> 2) & 0x03;
            p_cea708->p_cw->style.justify = v & 0x03;
            v = cea708_input_buffer_get( ib );
            p_cea708->p_cw->style.effect_speed = v >> 4;
            p_cea708->p_cw->style.effect_direction = (v >> 2) & 0x03;
            p_cea708->p_cw->style.display_effect = v & 0x03;
            Debug(printf("[SWA]"));
            break;

        default:
            if( code >= CEA708_C1_CW0 && code <= CEA708_C1_CW7 )
            {
                POP_COMMAND();
                Debug(printf("[CW%d]", code - CEA708_C1_CW0));
                if( p_cea708->window[code - CEA708_C1_CW0].b_defined )
                    p_cea708->p_cw = &p_cea708->window[code - CEA708_C1_CW0];
            }
            else if( code >= CEA708_C1_DF0 && code <= CEA708_C1_DF7 )
            {
                REQUIRE_ARGS_AND_POP_COMMAND(6);
                Debug(printf("[DF%d]", code - CEA708_C1_DF0));
                /* also sets current window */
                p_cea708->p_cw = &p_cea708->window[code - CEA708_C1_DF0];
                v = cea708_input_buffer_get( ib );
                if( p_cea708->p_cw->b_defined &&
                   !p_cea708->p_cw->b_visible != !(v & 0x20) )
                    i_ret |= CEA708_STATUS_OUTPUT;
                p_cea708->p_cw->b_visible = v & 0x20;
                p_cea708->p_cw->b_row_lock = v & 0x10;
                p_cea708->p_cw->b_column_lock = v & 0x08;
                p_cea708->p_cw->i_priority = v & 0x07;
                v = cea708_input_buffer_get( ib );
                p_cea708->p_cw->b_relative = v & 0x80;
                p_cea708->p_cw->i_anchor_offset_v = v & 0x7F;
                v = cea708_input_buffer_get( ib );
                p_cea708->p_cw->i_anchor_offset_h = v;
                v = cea708_input_buffer_get( ib );
                p_cea708->p_cw->anchor_point = v >> 4;
                p_cea708->p_cw->i_row_count = (v & 0x0F) + 1;
                v = cea708_input_buffer_get( ib );
                p_cea708->p_cw->i_col_count = v & 0x3F;
                v = cea708_input_buffer_get( ib );
                /* zero values style set on init, avoid dealing with updt case */
                i = (v >> 3) & 0x07; /* Window style id */
                if( i > 0 && !p_cea708->p_cw->b_defined )
                    p_cea708->p_cw->style = cea708_default_window_styles[i];
                i = v & 0x07; /* Pen style id */
                if( i > 0 && !p_cea708->p_cw->b_defined )
                    p_cea708->p_cw->pen = cea708_default_pen_styles[i];
                p_cea708->p_cw->b_defined = true;
            }
            else
            {
                Debug(printf("{%2.2x}", code));
                POP_COMMAND();
            }
    }

    return i_ret;
}

static int CEA708_Decode_G1( uint8_t code, cea708_t *p_cea708 )
{
    cea708_input_buffer_t *ib = &p_cea708->input_buffer;
    POP_COMMAND();

    if( !p_cea708->p_cw->b_defined )
        return CEA708_STATUS_OK;

    uint8_t utf8[4] = {0xc0 | (code & 0xc0) >> 6,
                       0x80 | (code & 0x3f),
                       0, 0};

    CEA708_Window_Write( utf8, p_cea708->p_cw );
    p_cea708->b_text_waiting |= p_cea708->p_cw->b_visible;

    return CEA708_STATUS_OK;
}

static int CEA708_Decode_G2G3( uint8_t code, cea708_t *p_cea708 )
{
    if( !p_cea708->p_cw->b_defined )
        return CEA708_STATUS_OK;

    uint8_t out[4] = { '?', 0, 0, 0 };
    static const struct {
        uint8_t c;
        uint8_t utf8[4];
    } code2utf8[] = {
        /* G2 */
        { 0x20,     { 0x20 } },// transparent space [*** will need special handling]
        { 0x21,     { 0x20 } },// non breaking transparent space [*** will need special handling]
        { 0x25,     { 0xe2,0x80,0xa6 } },// HORIZONTAL ELLIPSIS
        { 0x2a,     { 0xc5,0xa0 } },// LATIN CAPITAL LETTER S WITH CARON
        { 0x2c,     { 0xc5,0x92 } },// LATIN CAPITAL LIGATURE OE
        { 0x30,     { 0xe2,0x96,0x88 } },// FULL BLOCK
        { 0x31,     { 0xe2,0x80,0x98 } },// LEFT SINGLE QUOTATION MARK
        { 0x32,     { 0xe2,0x80,0x99 } },// RIGHT SINGLE QUOTATION MARK
        { 0x33,     { 0xe2,0x80,0x9c } },// LEFT DOUBLE QUOTATION MARK
        { 0x34,     { 0xe2,0x80,0x9d } },// RIGHT DOUBLE QUOTATION MARK
        { 0x35,     { 0xe2,0x80,0xa2 } },// BULLET
        { 0x39,     { 0xe2,0x84,0xa2 } },// Trademark symbol (TM)
        { 0x3a,     { 0xc5,0xa1 } },// LATIN SMALL LETTER S WITH CARON
        { 0x3c,     { 0xc5,0x93 } },// LATIN SMALL LIGATURE OE
        { 0x3d,     { 0xe2,0x84,0xa0 } },// SERVICE MARK
        { 0x3f,     { 0xc5,0xb8 } },// LATIN CAPITAL LETTER Y WITH DIAERESIS
        { 0x76,     { 0xe2,0x85,0x9b } },// VULGAR FRACTION ONE EIGHTH
        { 0x77,     { 0xe2,0x85,0x9c } },// VULGAR FRACTION THREE EIGHTHS
        { 0x78,     { 0xe2,0x85,0x9d } },// VULGAR FRACTION FIVE EIGHTHS
        { 0x79,     { 0xe2,0x85,0x9e } },// VULGAR FRACTION SEVEN EIGHTHS
        { 0x7a,     { 0xe2,0x94,0x82 } },// BOX DRAWINGS LIGHT VERTICAL
        { 0x7b,     { 0xe2,0x94,0x90 } },// BOX DRAWINGS LIGHT DOWN AND LEFT
        { 0x7c,     { 0xe2,0x94,0x94 } },// BOX DRAWINGS LIGHT UP AND RIGHT
        { 0x7d,     { 0xe2,0x94,0x80 } },// BOX DRAWINGS LIGHT HORIZONTAL
        { 0x7e,     { 0xe2,0x94,0x98 } },// BOX DRAWINGS LIGHT UP AND LEFT
        { 0x7f,     { 0xe2,0x94,0x8c } },// BOX DRAWINGS LIGHT DOWN AND RIGHT
        /* G3 */
        { 0xa0,     { 0xf0,0x9f,0x85,0xb2 } },// CC (replaced with NEGATIVE SQUARED LATIN CAPITAL LETTER C)
    };

    for( size_t i = 0; i < ARRAY_SIZE(code2utf8) ; i++ )
    {
        if( code2utf8[i].c == code )
        {
            memcpy( out, code2utf8[i].utf8, 4 );
            if(out[0] < 0xf0)
            {
                if(out[0] < 0x80)
                    out[1] = 0;
                else if(out[0] < 0xe0)
                    out[2] = 0;
                else
                    out[3] = 0;
            }
            break;
        }
    }

    CEA708_Window_Write( out, p_cea708->p_cw );

    p_cea708->b_text_waiting |= p_cea708->p_cw->b_visible;

    return CEA708_STATUS_OK;
}

static int CEA708_Decode_P16( uint16_t ucs2, cea708_t *p_cea708 )
{
    if( !p_cea708->p_cw->b_defined )
        return CEA708_STATUS_OK;

    uint8_t out[4] = { '?', 0, 0, 0 };

    /* adapted from codepoint conversion from strings.h */
    if( ucs2 <= 0x7F )
    {
        out[0] = ucs2;
    }
    else if( ucs2 <= 0x7FF )
    {
        out[0] = 0xC0 |  (ucs2 >>  6);
        out[1] = 0x80 |  (ucs2        & 0x3F);
    }
    else
    {
        out[0] = 0xE0 |  (ucs2 >> 12);
        out[1] = 0x80 | ((ucs2 >>  6) & 0x3F);
        out[2] = 0x80 |  (ucs2        & 0x3F);
    }

    CEA708_Window_Write( out, p_cea708->p_cw );

    p_cea708->b_text_waiting |= p_cea708->p_cw->b_visible;

    return CEA708_STATUS_OK;
}

static void CEA708_Decode_ServiceBuffer( cea708_t *h )
{
    for( ;; )
    {
        const uint8_t i_in = cea708_input_buffer_size( &h->input_buffer );
        if( i_in == 0 )
            break;

        int i_ret;
        uint8_t c = cea708_input_buffer_peek( &h->input_buffer, 0 );

        if( c < 0x20 )
            i_ret = CEA708_Decode_C0( c, h );
        else if( c <= 0x7F )
            i_ret = CEA708_Decode_G0( c, h );
        else if( c <= 0x9F )
            i_ret = CEA708_Decode_C1( c, h );
        else
            i_ret = CEA708_Decode_G1( c, h );

        if( i_ret & CEA708_STATUS_OUTPUT )
            CEA708_Output( h );

        if( i_ret & CEA708_STATUS_STARVING )
            break;

        /* Update internal clock */
        const uint8_t i_consumed = i_in - cea708_input_buffer_size( &h->input_buffer );
        if( i_consumed )
            h->i_clock += vlc_tick_from_samples(1, 9600) * i_consumed;
    }
}

void CEA708_Decoder_Push( cea708_t *h, vlc_tick_t i_time,
                          const uint8_t *p_data, size_t i_data )
{
    /* Set new buffer start time */
    h->i_clock = i_time;

    for( size_t i=0; i<i_data; )
    {
        /* Never push more than buffer */
        size_t i_push = cea708_input_buffer_remain(&h->input_buffer);
        if( (i_data - i) < i_push )
            i_push = (i_data - i);
        else
            h->suspended_deadline = VLC_TICK_INVALID; /* Full buffer cancels pause */

        for( size_t j=0; j<i_push; j++ )
        {
            uint8_t byte = p_data[i+j];
            cea708_input_buffer_add( &h->input_buffer, byte );
        }

        if( h->suspended_deadline != VLC_TICK_INVALID )
        {
            /* Decoding is paused */
            if ( h->suspended_deadline > h->i_clock )
            {
                /* Increase internal clock */
                if( i_push )
                    h->i_clock += vlc_tick_from_samples(1, 1200) * i_push;
                continue;
            }
            h->suspended_deadline = VLC_TICK_INVALID;
        }

        /* Decode Buffer */
        CEA708_Decode_ServiceBuffer( h );

        i += i_push;
    }
}
