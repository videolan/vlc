/*****************************************************************************
 * telx.c : Minimalistic Teletext subtitles decoder
 *****************************************************************************
 * Copyright (C) 2007 Vincent Penne
 * Some code converted from ProjectX java dvb decoder (c) 2001-2005 by dvb.matt
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
 *
 * information on teletext format can be found here :
 * http://pdc.ro.nu/teletext.html
 *
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_bits.h>
#include <vlc_codec.h>

/* #define TELX_DEBUG */
#ifdef TELX_DEBUG
#   define dbg( a ) msg_Dbg a
#else
#   define dbg( a ) (void) 0
#endif

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static int  Decode( decoder_t *, block_t * );

#define OVERRIDE_PAGE_TEXT N_("Override page")
#define OVERRIDE_PAGE_LONGTEXT N_("Override the indicated page, try this if " \
        "your subtitles don't appear (-1 = autodetect from TS, " \
        "0 = autodetect from teletext, " \
        ">0 = actual page number, usually 888 or 889).")

#define IGNORE_SUB_FLAG_TEXT N_("Ignore subtitle flag")
#define IGNORE_SUB_FLAG_LONGTEXT N_("Ignore the subtitle flag, try this if " \
        "your subtitles don't appear.")

#define FRENCH_WORKAROUND_TEXT N_("Workaround for France")
#define FRENCH_WORKAROUND_LONGTEXT N_("Some French channels do not flag " \
        "their subtitling pages correctly due to a historical " \
        "interpretation mistake. Try using this wrong interpretation if " \
        "your subtitles don't appear.")

vlc_module_begin ()
    set_description( N_("Teletext subtitles decoder") )
    set_shortname( "Teletext" )
    set_capability( "spu decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, Close )

    add_integer( "telx-override-page", -1,
                 OVERRIDE_PAGE_TEXT, OVERRIDE_PAGE_LONGTEXT, true )
    add_bool( "telx-ignore-subtitle-flag", false,
              IGNORE_SUB_FLAG_TEXT, IGNORE_SUB_FLAG_LONGTEXT, true )
    add_bool( "telx-french-workaround", false,
              FRENCH_WORKAROUND_TEXT, FRENCH_WORKAROUND_LONGTEXT, true )

vlc_module_end ()

/****************************************************************************
 * Local structures
 ****************************************************************************/

typedef struct
{
  int         i_align;
  bool        b_is_subtitle[9];
  char        ppsz_lines[32][128];
  char        psz_prev_text[512];
  vlc_tick_t  prev_pts;
  int         i_page[9];
  bool        b_erase[9];
  const uint16_t *  pi_active_national_set[9];
  int         i_wanted_page, i_wanted_magazine;
  bool        b_ignore_sub_flag;
} decoder_sys_t;

/****************************************************************************
 * Local data
 ****************************************************************************/

/*
 * My doc only mentions 13 national characters, but experiments show there
 * are more, in france for example I already found two more (0x9 and 0xb).
 *
 * Conversion is in this order :
 *
 * 0x23 0x24 0x40 0x5b 0x5c 0x5d 0x5e 0x5f 0x60 0x7b 0x7c 0x7d 0x7e
 * (these are the standard ones)
 * 0x08 0x09 0x0a 0x0b 0x0c 0x0d (apparently a control character) 0x0e 0x0f
 */

static const uint16_t ppi_national_subsets[][20] =
{
  { 0x00a3, 0x0024, 0x0040, 0x00ab, 0x00bd, 0x00bb, 0x005e, 0x0023,
    0x002d, 0x00bc, 0x00a6, 0x00be, 0x00f7 }, /* english ,000 */

  { 0x00e9, 0x00ef, 0x00e0, 0x00eb, 0x00ea, 0x00f9, 0x00ee, 0x0023,
    0x00e8, 0x00e2, 0x00f4, 0x00fb, 0x00e7, 0, 0x00eb, 0, 0x00ef }, /* french  ,001 */

  { 0x0023, 0x00a4, 0x00c9, 0x00c4, 0x00d6, 0x00c5, 0x00dc, 0x005f,
    0x00e9, 0x00e4, 0x00f6, 0x00e5, 0x00fc }, /* swedish,finnish,hungarian ,010 */

  { 0x0023, 0x016f, 0x010d, 0x0165, 0x017e, 0x00fd, 0x00ed, 0x0159,
    0x00e9, 0x00e1, 0x011b, 0x00fa, 0x0161 }, /* czech,slovak  ,011 */

  { 0x0023, 0x0024, 0x00a7, 0x00c4, 0x00d6, 0x00dc, 0x005e, 0x005f,
    0x00b0, 0x00e4, 0x00f6, 0x00fc, 0x00df }, /* german ,100 */

  { 0x00e7, 0x0024, 0x00a1, 0x00e1, 0x00e9, 0x00ed, 0x00f3, 0x00fa,
    0x00bf, 0x00fc, 0x00f1, 0x00e8, 0x00e0 }, /* portuguese,spanish ,101 */

  { 0x00a3, 0x0024, 0x00e9, 0x00b0, 0x00e7, 0x00bb, 0x005e, 0x0023,
    0x00f9, 0x00e0, 0x00f2, 0x00e8, 0x00ec }, /* italian  ,110 */

  { 0x0023, 0x00a4, 0x0162, 0x00c2, 0x015e, 0x0102, 0x00ce, 0x0131,
    0x0163, 0x00e2, 0x015f, 0x0103, 0x00ee }, /* rumanian ,111 */

  /* I have these tables too, but I don't know how they can be triggered */
  { 0x0023, 0x0024, 0x0160, 0x0117, 0x0119, 0x017d, 0x010d, 0x016b,
    0x0161, 0x0105, 0x0173, 0x017e, 0x012f }, /* lettish,lithuanian ,1000 */

  { 0x0023, 0x0144, 0x0105, 0x005a, 0x015a, 0x0141, 0x0107, 0x00f3,
    0x0119, 0x017c, 0x015b, 0x0142, 0x017a }, /* polish,  1001 */

  { 0x0023, 0x00cb, 0x010c, 0x0106, 0x017d, 0x0110, 0x0160, 0x00eb,
    0x010d, 0x0107, 0x017e, 0x0111, 0x0161 }, /* serbian,croatian,slovenian, 1010 */

  { 0x0023, 0x00f5, 0x0160, 0x00c4, 0x00d6, 0x017e, 0x00dc, 0x00d5,
    0x0161, 0x00e4, 0x00f6, 0x017e, 0x00fc }, /* estonian  ,1011 */

  { 0x0054, 0x011f, 0x0130, 0x015e, 0x00d6, 0x00c7, 0x00dc, 0x011e,
    0x0131, 0x015f, 0x00f6, 0x00e7, 0x00fc }, /* turkish  ,1100 */
};

enum
{
    FLAG_ERASE_PAGE         = 0x000080,
    FLAG_NEWSFLASH          = 0x004000,
    FLAG_SUBTITLE           = 0x008000,
    FLAG_SUPPRESS_HEADER    = 0x010000,
    FLAG_UPDATE             = 0x020000,
    FLAG_INTERRUPTED        = 0x040000,
    FLAG_INHIBIT_DISPLAY    = 0x080000,
    FLAG_MAGAZINE_SERIAL    = 0x100000,
    FLAG_FRAGMENT           = 0x200000,
    FLAG_PARTIAL_PAGE       = 0x400000,
};

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;
    decoder_sys_t *p_sys = NULL;
    int            i_val;


    if( p_dec->fmt_in.i_codec != VLC_CODEC_TELETEXT)
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode = Decode;
    p_sys = p_dec->p_sys = calloc( 1, sizeof(*p_sys) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_dec->fmt_out.i_codec = 0;

    p_sys->i_align = 0;
    for ( int i = 0; i < 9; i++ )
        p_sys->pi_active_national_set[i] = ppi_national_subsets[1];

    i_val = var_CreateGetInteger( p_dec, "telx-override-page" );
    if( i_val == -1 && p_dec->fmt_in.subs.teletext.i_magazine < 9 &&
        ( p_dec->fmt_in.subs.teletext.i_magazine != 1 ||
          p_dec->fmt_in.subs.teletext.i_page != 0 ) ) /* ignore if TS demux wants page 100 (unlikely to be sub) */
    {
        bool b_val;
        p_sys->i_wanted_magazine = p_dec->fmt_in.subs.teletext.i_magazine;
        p_sys->i_wanted_page = p_dec->fmt_in.subs.teletext.i_page;

        b_val = var_CreateGetBool( p_dec, "telx-french-workaround" );
        if( p_sys->i_wanted_page < 100 &&
            (b_val || (p_sys->i_wanted_page % 16) >= 10))
        {
            /* See http://www.nada.kth.se/~ragge/vdr/ttxtsubs/TROUBLESHOOTING.txt
             * paragraph about French channels - they mix up decimal and
             * hexadecimal */
            p_sys->i_wanted_page = (p_sys->i_wanted_page / 10) * 16 +
                                   (p_sys->i_wanted_page % 10);
        }
    }
    else if( i_val <= 0 )
    {
        p_sys->i_wanted_magazine = -1;
        p_sys->i_wanted_page = -1;
    }
    else
    {
        p_sys->i_wanted_magazine = i_val / 100;
        p_sys->i_wanted_page = (((i_val % 100) / 10) << 4)
                               |((i_val % 100) % 10);
    }
    p_sys->b_ignore_sub_flag = var_CreateGetBool( p_dec,
                                    "telx-ignore-subtitle-flag" );

    msg_Dbg( p_dec, "starting telx on magazine %d page %02x flag %d",
             p_sys->i_wanted_magazine, p_sys->i_wanted_page,
             p_sys->b_ignore_sub_flag );

    return VLC_SUCCESS;

/*  error: */
/*     if (p_sys) { */
/*       free(p_sys); */
/*       p_sys = NULL; */
/*     } */
/*     return VLC_EGENERIC; */
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*) p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys );
}

/**************************
 * change bits endianness *
 **************************/
static uint8_t bytereverse( int n )
{
    n = (((n >> 1) & 0x55) | ((n << 1) & 0xaa));
    n = (((n >> 2) & 0x33) | ((n << 2) & 0xcc));
    n = (((n >> 4) & 0x0f) | ((n << 4) & 0xf0));
    return n;
}

static int hamming_8_4( int a )
{
    switch (a) {
    case 0xA8:
        return 0;
    case 0x0B:
        return 1;
    case 0x26:
        return 2;
    case 0x85:
        return 3;
    case 0x92:
        return 4;
    case 0x31:
        return 5;
    case 0x1C:
        return 6;
    case 0xBF:
        return 7;
    case 0x40:
        return 8;
    case 0xE3:
        return 9;
    case 0xCE:
        return 10;
    case 0x6D:
        return 11;
    case 0x7A:
        return 12;
    case 0xD9:
        return 13;
    case 0xF4:
        return 14;
    case 0x57:
        return 15;
    default:
        return -1;     // decoding error , not yet corrected
    }
}

// utc-2 --> utf-8
// this is not a general function, but it's enough for what we do here
// the result buffer need to be at least 4 bytes long
static void to_utf8( char * res, uint16_t ch )
{
    if( ch >= 0x80 )
    {
        if( ch >= 0x800 )
        {
            res[0] = (ch >> 12) | 0xE0;
            res[1] = ((ch >> 6) & 0x3F) | 0x80;
            res[2] = (ch & 0x3F) | 0x80;
            res[3] = 0;
        }
        else
        {
            res[0] = (ch >> 6) | 0xC0;
            res[1] = (ch & 0x3F) | 0x80;
            res[2] = 0;
        }
    }
    else
    {
        res[0] = ch;
        res[1] = 0;
    }
}

static void decode_string( char * res, int res_len,
                           decoder_sys_t *p_sys, int magazine,
                           const uint8_t * packet, int len )
{
    char utf8[7];
    char * pt = res;

    for ( int i = 0; i < len; i++ )
    {
        int in = bytereverse( packet[i] ) & 0x7f;
        uint16_t out = 32;
        size_t l;

        switch ( in )
        {
        /* special national characters */
        case 0x23:
            out = p_sys->pi_active_national_set[magazine][0];
            break;
        case 0x24:
            out = p_sys->pi_active_national_set[magazine][1];
            break;
        case 0x40:
            out = p_sys->pi_active_national_set[magazine][2];
            break;
        case 0x5b:
            out = p_sys->pi_active_national_set[magazine][3];
            break;
        case 0x5c:
            out = p_sys->pi_active_national_set[magazine][4];
            break;
        case 0x5d:
            out = p_sys->pi_active_national_set[magazine][5];
            break;
        case 0x5e:
            out = p_sys->pi_active_national_set[magazine][6];
            break;
        case 0x5f:
            out = p_sys->pi_active_national_set[magazine][7];
            break;
        case 0x60:
            out = p_sys->pi_active_national_set[magazine][8];
            break;
        case 0x7b:
            out = p_sys->pi_active_national_set[magazine][9];
            break;
        case 0x7c:
            out = p_sys->pi_active_national_set[magazine][10];
            break;
        case 0x7d:
            out = p_sys->pi_active_national_set[magazine][11];
            break;
        case 0x7e:
            out = p_sys->pi_active_national_set[magazine][12];
            break;

        /* some special control characters (empirical) */
        case 0x0d:
            /* apparently this starts a sequence that ends with 0xb 0xb */
            while ( i + 1 < len && (bytereverse( packet[i+1] ) & 0x7f) != 0x0b )
                i++;
            i += 2;
            break;
            /* goto skip; */

        default:
            /* non documented national range 0x08 - 0x0f */
            if ( in >= 0x08 && in <= 0x0f )
            {
                out = p_sys->pi_active_national_set[magazine][13 + in - 8];
                break;
            }

            /* normal ascii */
            if ( in > 32 && in < 0x7f )
                out = in;
        }

        /* handle undefined national characters */
        if ( out == 0 )
            out = 32;

        /* convert to utf-8 */
        to_utf8( utf8, out );
        l = strlen( utf8 );
        if ( pt + l < res + res_len - 1 )
        {
            strcpy(pt, utf8);
            pt += l;
        }

        /* skip: ; */
    }
    /* end: */
    *pt++ = 0;
}

static bool DecodePageHeaderPacket( decoder_t *p_dec, const uint8_t *packet,
                                    int magazine )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    int flag = 0;
    char psz_line[256];

    for ( int a = 0; a < 6; a++ )
    {
        flag |= (0xF & (bytereverse( hamming_8_4(packet[8 + a]) ) >> 4))
                  << (a * 4);
    }

/*         if (!p_sys->b_ignore_sub_flag && !(flag & FLAG_SUBTITLE)) */
/*           return false; */

    p_sys->i_page[magazine] = (0xF0 & bytereverse( hamming_8_4(packet[7]) )) | /* tens */
                              (0x0F & (bytereverse( hamming_8_4(packet[6]) ) >> 4) ); /* units */

    decode_string( psz_line, sizeof(psz_line), p_sys, magazine,
                   packet + 14, 40 - 14 );

    dbg((p_dec, "mag %d flags %x page %x character set %d subtitles %d %s", magazine, flag,
         p_sys->i_page[magazine],
         7 & flag>>21, !!(flag & FLAG_SUBTITLE), psz_line));

    p_sys->pi_active_national_set[magazine] =
                         ppi_national_subsets[7 & (flag >> 21)];

    int subtitlesflags = FLAG_SUBTITLE;
    if( !p_sys->b_ignore_sub_flag && p_sys->i_wanted_magazine != 0x07 )
        subtitlesflags |= FLAG_SUPPRESS_HEADER;

    p_sys->b_is_subtitle[magazine] = !((flag & subtitlesflags) != subtitlesflags);

    dbg(( p_dec, "FLAGS%s%s%s%s%s%s%s mag_ser %d",
          (flag & FLAG_ERASE_PAGE)     ? " erase" : "",
          (flag & FLAG_NEWSFLASH)      ? " news" : "",
          (flag & FLAG_SUBTITLE)       ? " subtitle" : "",
          (flag & FLAG_SUPPRESS_HEADER)? " suppressed_head" : "",
          (flag & FLAG_UPDATE)         ? " update" : "",
          (flag & FLAG_INTERRUPTED)    ? " interrupt" : "",
          (flag & FLAG_INHIBIT_DISPLAY)? " inhibit" : "",
        !!(flag & FLAG_MAGAZINE_SERIAL) ));

    if ( (p_sys->i_wanted_page != -1
           && p_sys->i_page[magazine] != p_sys->i_wanted_page)
           || !p_sys->b_is_subtitle[magazine] )
        return false;

    p_sys->b_erase[magazine] = !!(flag & FLAG_ERASE_PAGE);

    /* kludge here :
     * we ignore the erase flag if it happens less than 1.5 seconds
     * before last caption
     * TODO   make this time configurable
     * UPDATE the kludge seems to be no more necessary
     *        so it's commented out*/
    if ( /*p_block->i_pts > p_sys->prev_pts + 1500000 && */
         p_sys->b_erase[magazine] )
    {
        dbg((p_dec, "ERASE !"));

        p_sys->b_erase[magazine] = 0;
        for ( int i = 1; i < 32; i++ )
        {
            if ( !p_sys->ppsz_lines[i][0] ) continue;
            /* b_update = true; */
            p_sys->ppsz_lines[i][0] = 0;
        }
    }

    /* replace the row if it's different */
    if ( strcmp(psz_line, p_sys->ppsz_lines[0]) )
    {
        strncpy( p_sys->ppsz_lines[0], psz_line,
                 sizeof(p_sys->ppsz_lines[0]) - 1);
    }

    return true;
}

static bool DecodePacketX1_X23( decoder_t *p_dec, const uint8_t *packet,
                                int magazine, int row, vlc_tick_t pts )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    bool b_update = false;
    char psz_line[256];
    char * t;
    int i;

    if ( p_sys->i_wanted_page == -1 && p_sys->i_page[magazine] > 0x99)
        return false;

    decode_string( psz_line, sizeof(psz_line), p_sys, magazine,
                   packet + 6, 40 );
    t = psz_line;

    /* remove starting spaces */
    while ( *t == 32 ) t++;

    /* remove trailing spaces */
    for ( i = strlen(t) - 1; i >= 0 && t[i] == 32; i-- );
    t[i + 1] = 0;

    /* replace the row if it's different */
    if ( strcmp( t, p_sys->ppsz_lines[row] ) )
    {
        strncpy( p_sys->ppsz_lines[row], t,
                 sizeof(p_sys->ppsz_lines[row]) - 1 );
        b_update = true;
    }

    if (t[0])
        p_sys->prev_pts = pts;

    dbg((p_dec, "%d %d : ", magazine, row));
    dbg((p_dec, "%s", t));

#ifdef TELX_DEBUG
    {
        char dbg[256];
        dbg[0] = 0;
        for ( i = 0; i < 40; i++ )
        {
            int in = bytereverse(packet[6 + i]) & 0x7f;
            sprintf(dbg + strlen(dbg), "%02x ", in);
        }
        dbg((p_dec, "%s", dbg));
        dbg[0] = 0;
        for ( i = 0; i < 40; i++ )
        {
            decode_string( psz_line, sizeof(psz_line), p_sys, magazine,
                           packet + 6 + i, 1 );
            sprintf( dbg + strlen(dbg), "%s  ", psz_line );
        }
        dbg((p_dec, "%s", dbg));
    }
#endif
    return b_update;
}


static bool DecodePacketX25( decoder_t *p_dec, const uint8_t *packet,
                             int magazine )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* row 25 : alternate header line */
    char psz_line[256];
    decode_string( psz_line, sizeof(psz_line), p_sys, magazine,
                   packet + 6, 40 );

    /* replace the row if it's different */
    if ( strcmp( psz_line, p_sys->ppsz_lines[0] ) )
    {
        strncpy( p_sys->ppsz_lines[0], psz_line,
                 sizeof(p_sys->ppsz_lines[0]) - 1 );
        /* return true; */
    }

    return false;
}

static bool DecodeNormalPacket( decoder_t *p_dec, const uint8_t *packet,
                                int magazine, int row, vlc_tick_t pts )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( (p_sys->i_wanted_page != -1 &&
         p_sys->i_page[magazine] != p_sys->i_wanted_page)
         || !p_sys->b_is_subtitle[magazine] )
        return false;

    if( row < 24 ) /* row 1-23 : normal lines */
        return DecodePacketX1_X23( p_dec, packet, magazine, row, pts );
    else if( row == 25 ) /* row 25 : alternate header line */
        return DecodePacketX25( p_dec, packet, magazine );
    else
        return false;
}


/*****************************************************************************
 * Decode:
 *****************************************************************************/
static int Decode( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t  *p_spu = NULL;
    video_format_t fmt;
    /* int erase = 0; */
#if 0
    int i_wanted_magazine = i_conf_wanted_page / 100;
    int i_wanted_page = 0x10 * ((i_conf_wanted_page % 100) / 10)
                         | (i_conf_wanted_page % 10);
#endif
    bool b_update = false;
    char psz_text[512], *pt = psz_text;
    int total;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

//    dbg((p_dec, "start of telx packet with header %2x",
//                * (uint8_t *) p_block->p_buffer));
    for ( size_t offset = 1; offset + 46 <= p_block->i_buffer; offset += 46 )
    {
        const uint8_t *packet = &p_block->p_buffer[offset];
//        int vbi = ((0x20 & packet[2]) != 0 ? 0 : 313) + (0x1F & packet[2]);

//        dbg((p_dec, "vbi %d header %02x %02x %02x", vbi, packet[0], packet[1], packet[2]));
        if ( packet[0] == 0xFF ) continue;

/*      if (packet[1] != 0x2C) { */
/*         printf("wrong header\n"); */
/*         //goto error; */
/*         continue; */
/*       } */

        /* See EN.300.706 7.1.4 */
        int mpag = (hamming_8_4( packet[4] ) << 4) | hamming_8_4( packet[5] );
        int row, magazine;
        if ( mpag < 0 )
        {
            /* decode error */
            dbg((p_dec, "mpag hamming error"));
            continue;
        }

        /* magazine number 0-7, row 0-31 (== packet number... or Y) */
        row = 0xFF & bytereverse(mpag);
        magazine = (7 & row) == 0 ? 8 : (7 & row);
        row >>= 3;

        if ( p_sys->i_wanted_page != -1
              && magazine != p_sys->i_wanted_magazine )
            continue;

        if ( row == 0 ) /* Page Header Packet */
        {
            /* row 0 : flags and header line */
            b_update |= DecodePageHeaderPacket( p_dec, packet, magazine );
            if( b_update )
                dbg((p_dec, "%ld --> %ld", (long int) p_block->i_pts,
                                           (long int)(p_sys->prev_pts+1500000)));
        }
        else if ( row < 26 ) /* Normal Packet */
        {
            b_update |= DecodeNormalPacket( p_dec, packet, magazine, row,
                                            p_block->i_pts );
        }
//       else if (row >= 26) { /* Non Displayable Packet */
//         // row 26 : TV listings
//            dbg((p_dec, "%d %d : %s", magazine, row, decode_string(p_sys, magazine, packet+6, 40)));
//       }
    }

    if ( !b_update )
        goto error;

    total = 0;
    for ( int i = 1; i < 24; i++ )
    {
        size_t l = strlen( p_sys->ppsz_lines[i] );

        if ( l > sizeof(psz_text) - total - 1 )
            l = sizeof(psz_text) - total - 1;

        if ( l > 0 )
        {
            memcpy( pt, p_sys->ppsz_lines[i], l );
            total += l;
            pt += l;
            if ( sizeof(psz_text) - total - 1 > 0 )
            {
                *pt++ = '\n';
                total++;
            }
        }
    }
    *pt = 0;

    if ( !strcmp(psz_text, p_sys->psz_prev_text) )
        goto error;

    dbg((p_dec, "UPDATE TELETEXT PICTURE"));

    assert( sizeof(p_sys->psz_prev_text) >= sizeof(psz_text) );
    strcpy( p_sys->psz_prev_text, psz_text );

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpicture( p_dec, NULL );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        goto error;
    }

    /* Create a new subpicture region */
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    p_spu->p_region = subpicture_region_New( &fmt );
    if( p_spu->p_region == NULL )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        goto error;
    }

    /* Normal text subs, easy markup */
    p_spu->p_region->i_align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
    p_spu->p_region->i_x = p_sys->i_align ? 20 : 0;
    p_spu->p_region->i_y = 10;
    p_spu->p_region->p_text = text_segment_New(psz_text);

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer = (p_block->i_length == 0);
    p_spu->b_absolute = false;
    dbg((p_dec, "%ld --> %ld", (long int) p_block->i_pts/100000, (long int)p_block->i_length/100000));

    block_Release( p_block );
    if( p_spu != NULL )
        decoder_QueueSub( p_dec, p_spu );
    return VLCDEC_SUCCESS;

error:
    if ( p_spu != NULL )
    {
        subpicture_Delete( p_spu );
        p_spu = NULL;
    }

    block_Release( p_block );
    return VLCDEC_SUCCESS;
}
