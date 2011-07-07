/*****************************************************************************
 * libc.c: Extra libc function for some systems.
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem à videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>

#include <errno.h>

#undef iconv_t
#undef iconv_open
#undef iconv
#undef iconv_close

#if defined(HAVE_ICONV)
#   include <iconv.h>
#endif

#ifdef HAVE_FORK
#   include <signal.h>
#   include <unistd.h>
#   include <sys/wait.h>
#   include <sys/socket.h>
#   include <sys/poll.h>
#   ifndef PF_LOCAL
#       define PF_LOCAL PF_UNIX
#   endif
#endif

#if defined(WIN32) || defined(UNDER_CE)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif



/*****************************************************************************
 * Local conversion routine from ISO_6937 to UTF-8 charset. Support for this
 * is still missing in libiconv, hence multiple operating systems lack it.
 * The conversion table adds Euro sign (0xA4) as per ETSI EN 300 468 Annex A
 *****************************************************************************/
#ifndef __linux__
static const uint16_t to_ucs4[128] =
{
  /* 0x80 */ 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
  /* 0x88 */ 0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
  /* 0x90 */ 0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
  /* 0x98 */ 0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
  /* 0xa0 */ 0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x20ac, 0x00a5, 0x0000, 0x00a7,
  /* 0xa8 */ 0x00a4, 0x2018, 0x201c, 0x00ab, 0x2190, 0x2191, 0x2192, 0x2193,
  /* 0xb0 */ 0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00d7, 0x00b5, 0x00b6, 0x00b7,
  /* 0xb8 */ 0x00f7, 0x2019, 0x201d, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
  /* 0xc0 */ 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  /* 0xc8 */ 0xffff, 0x0000, 0xffff, 0xffff, 0x0000, 0xffff, 0xffff, 0xffff,
  /* 0xd0 */ 0x2014, 0x00b9, 0x00ae, 0x00a9, 0x2122, 0x266a, 0x00ac, 0x00a6,
  /* 0xd8 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x215b, 0x215c, 0x215d, 0x215e,
  /* 0xe0 */ 0x2126, 0x00c6, 0x00d0, 0x00aa, 0x0126, 0x0000, 0x0132, 0x013f,
  /* 0xe8 */ 0x0141, 0x00d8, 0x0152, 0x00ba, 0x00de, 0x0166, 0x014a, 0x0149,
  /* 0xf0 */ 0x0138, 0x00e6, 0x0111, 0x00f0, 0x0127, 0x0131, 0x0133, 0x0140,
  /* 0xf8 */ 0x0142, 0x00f8, 0x0153, 0x00df, 0x00fe, 0x0167, 0x014b, 0x00ad
};

/* The outer array range runs from 0xc1 to 0xcf, the inner range from 0x40
   to 0x7f.  */
static const uint16_t to_ucs4_comb[15][64] =
{
  /* 0xc1 */
  {
    /* 0x40 */ 0x0000, 0x00c0, 0x0000, 0x0000, 0x0000, 0x00c8, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x00cc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d2,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d9, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x00e0, 0x0000, 0x0000, 0x0000, 0x00e8, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x00ec, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f2,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f9, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc2 */
  {
    /* 0x40 */ 0x0000, 0x00c1, 0x0000, 0x0106, 0x0000, 0x00c9, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x00cd, 0x0000, 0x0000, 0x0139, 0x0000, 0x0143, 0x00d3,
    /* 0x50 */ 0x0000, 0x0000, 0x0154, 0x015a, 0x0000, 0x00da, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x00dd, 0x0179, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x00e1, 0x0000, 0x0107, 0x0000, 0x00e9, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x00ed, 0x0000, 0x0000, 0x013a, 0x0000, 0x0144, 0x00f3,
    /* 0x70 */ 0x0000, 0x0000, 0x0155, 0x015b, 0x0000, 0x00fa, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x00fd, 0x017a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc3 */
  {
    /* 0x40 */ 0x0000, 0x00c2, 0x0000, 0x0108, 0x0000, 0x00ca, 0x0000, 0x011c,
    /* 0x48 */ 0x0124, 0x00ce, 0x0134, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d4,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x015c, 0x0000, 0x00db, 0x0000, 0x0174,
    /* 0x58 */ 0x0000, 0x0176, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x00e2, 0x0000, 0x0109, 0x0000, 0x00ea, 0x0000, 0x011d,
    /* 0x68 */ 0x0125, 0x00ee, 0x0135, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f4,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x015d, 0x0000, 0x00fb, 0x0000, 0x0175,
    /* 0x78 */ 0x0000, 0x0177, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc4 */
  {
    /* 0x40 */ 0x0000, 0x00c3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x0128, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d1, 0x00d5,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0168, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x00e3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x0129, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f1, 0x00f5,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0169, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc5 */
  {
    /* 0x40 */ 0x0000, 0x0100, 0x0000, 0x0000, 0x0000, 0x0112, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x012a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x014c,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016a, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0101, 0x0000, 0x0000, 0x0000, 0x0113, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x012b, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x014d,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016b, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc6 */
  {
    /* 0x40 */ 0x0000, 0x0102, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x011e,
    /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016c, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0103, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x011f,
    /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016d, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc7 */
  {
    /* 0x40 */ 0x0000, 0x0000, 0x0000, 0x010a, 0x0000, 0x0116, 0x0000, 0x0120,
    /* 0x48 */ 0x0000, 0x0130, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x017b, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0000, 0x0000, 0x010b, 0x0000, 0x0117, 0x0000, 0x0121,
    /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x017c, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc8 */
  {
    /* 0x40 */ 0x0000, 0x00c4, 0x0000, 0x0000, 0x0000, 0x00cb, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x00cf, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d6,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00dc, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0178, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x00e4, 0x0000, 0x0000, 0x0000, 0x00eb, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x00ef, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f6,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00fc, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xc9 */
  {
    0x0000,
  },
  /* 0xca */
  {
    /* 0x40 */ 0x0000, 0x00c5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016e, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x00e5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016f, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xcb */
  {
    /* 0x40 */ 0x0000, 0x0000, 0x0000, 0x00c7, 0x0000, 0x0000, 0x0000, 0x0122,
    /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0136, 0x013b, 0x0000, 0x0145, 0x0000,
    /* 0x50 */ 0x0000, 0x0000, 0x0156, 0x015e, 0x0162, 0x0000, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0000, 0x0000, 0x00e7, 0x0000, 0x0000, 0x0000, 0x0123,
    /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0137, 0x013c, 0x0000, 0x0146, 0x0000,
    /* 0x70 */ 0x0000, 0x0000, 0x0157, 0x015f, 0x0163, 0x0000, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xcc */
  {
    0x0000,
  },
  /* 0xcd */
  {
    /* 0x40 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0150,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0170, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0151,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0171, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xce */
  {
    /* 0x40 */ 0x0000, 0x0104, 0x0000, 0x0000, 0x0000, 0x0118, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x012e, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0172, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0105, 0x0000, 0x0000, 0x0000, 0x0119, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x012f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0173, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  },
  /* 0xcf */
  {
    /* 0x40 */ 0x0000, 0x0000, 0x0000, 0x010c, 0x010e, 0x011a, 0x0000, 0x0000,
    /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x013d, 0x0000, 0x0147, 0x0000,
    /* 0x50 */ 0x0000, 0x0000, 0x0158, 0x0160, 0x0164, 0x0000, 0x0000, 0x0000,
    /* 0x58 */ 0x0000, 0x0000, 0x017d, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 0x60 */ 0x0000, 0x0000, 0x0000, 0x010d, 0x010f, 0x011b, 0x0000, 0x0000,
    /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x013e, 0x0000, 0x0148, 0x0000,
    /* 0x70 */ 0x0000, 0x0000, 0x0159, 0x0161, 0x0165, 0x0000, 0x0000, 0x0000,
    /* 0x78 */ 0x0000, 0x0000, 0x017e, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  }
};

static size_t ISO6937toUTF8( const unsigned char **inbuf, size_t *inbytesleft,
                             unsigned char **outbuf, size_t *outbytesleft )


{
    if( !inbuf || !(*inbuf) )
        return (size_t)(0);    /* Reset state requested */

    const unsigned char *iptr = *inbuf;
    const unsigned char *iend = iptr + *inbytesleft;
    unsigned char *optr = *outbuf;
    unsigned char *oend = optr + *outbytesleft;
    uint16_t ch;
    int err = 0;

    while ( iptr < iend )
    {
        if( *iptr < 0x80 )
        {
            if( optr >= oend )
            {
                err = E2BIG;
                break;    /* No space in outbuf */
            }
            *optr++ = *iptr++;
            continue;
        }


        if ( optr + 2 >= oend )
        {
            err = E2BIG;
            break;        /* No space in outbuf for multibyte char */
        }

        ch = to_ucs4[*iptr - 0x80];

        if( ch == 0xffff )
        {
            /* Composed character */
            if ( iptr + 1 >= iend )
            {
                err = EINVAL;
                break;    /* No next character */
            }
            if ( iptr[1] < 0x40 || iptr[1] >= 0x80 ||
                 !(ch = to_ucs4_comb[iptr[0] - 0xc1][iptr[1] - 0x40]) )
            {
                err = EILSEQ;
                break;   /* Illegal combination */
            }
            iptr += 2;

        }
        else
        {
            if ( !ch )
            {
                err = EILSEQ;
                break;
            }
            iptr++;
        }

        if ( ch < 0x800 )
        {
            optr[1] = 0x80 | (ch & 0x3f);
            optr[0] = 0xc0 | (ch >> 6);
            optr +=2;
        }
        else
        {
            optr[2] = 0x80 | (ch & 0x3f);
            ch >>= 6;
            optr[1] = 0x80 | (ch & 0x3f);
            optr[0] = 0xe0 | (ch >> 6);
            optr += 3;
        }

    }
    *inbuf = iptr;
    *outbuf = optr;
    *inbytesleft = iend - iptr;
    *outbytesleft = oend - optr;

    if( err )
    {
        errno = err;
        return (size_t)(-1);
    }

    return (size_t)(0);

}
#endif

/*****************************************************************************
 * iconv wrapper
 *****************************************************************************/
vlc_iconv_t vlc_iconv_open( const char *tocode, const char *fromcode )
{
#ifndef __linux__
    if( !strcasecmp(tocode, "UTF-8") && !strcasecmp(fromcode, "ISO_6937") )
        return (vlc_iconv_t)(-2);
#endif
#if defined(HAVE_ICONV)
    return iconv_open( tocode, fromcode );
#else
    return (vlc_iconv_t)(-1);
#endif
}

size_t vlc_iconv( vlc_iconv_t cd, const char **inbuf, size_t *inbytesleft,
                  char **outbuf, size_t *outbytesleft )
{
#ifndef __linux__
    if ( cd == (vlc_iconv_t)(-2) )
        return ISO6937toUTF8( (const unsigned char **)inbuf, inbytesleft,
                              (unsigned char **)outbuf, outbytesleft );
#endif
#if defined(HAVE_ICONV)
    return iconv( cd, (ICONV_CONST char **)inbuf, inbytesleft,
                  outbuf, outbytesleft );
#else
    abort ();
#endif
}

int vlc_iconv_close( vlc_iconv_t cd )
{
#ifndef __linux__
    if ( cd == (vlc_iconv_t)(-2) )
        return 0;
#endif
#if defined(HAVE_ICONV)
    return iconv_close( cd );
#else
    abort ();
#endif
}

/*****************************************************************************
 * reduce a fraction
 *   (adapted from libavcodec, author Michael Niedermayer <michaelni@gmx.at>)
 *****************************************************************************/
bool vlc_ureduce( unsigned *pi_dst_nom, unsigned *pi_dst_den,
                        uint64_t i_nom, uint64_t i_den, uint64_t i_max )
{
    bool b_exact = 1;
    uint64_t i_gcd;

    if( i_den == 0 )
    {
        *pi_dst_nom = 0;
        *pi_dst_den = 1;
        return 1;
    }

    i_gcd = GCD( i_nom, i_den );
    i_nom /= i_gcd;
    i_den /= i_gcd;

    if( i_max == 0 ) i_max = INT64_C(0xFFFFFFFF);

    if( i_nom > i_max || i_den > i_max )
    {
        uint64_t i_a0_num = 0, i_a0_den = 1, i_a1_num = 1, i_a1_den = 0;
        b_exact = 0;

        for( ; ; )
        {
            uint64_t i_x = i_nom / i_den;
            uint64_t i_a2n = i_x * i_a1_num + i_a0_num;
            uint64_t i_a2d = i_x * i_a1_den + i_a0_den;

            if( i_a2n > i_max || i_a2d > i_max ) break;

            i_nom %= i_den;

            i_a0_num = i_a1_num; i_a0_den = i_a1_den;
            i_a1_num = i_a2n; i_a1_den = i_a2d;
            if( i_nom == 0 ) break;
            i_x = i_nom; i_nom = i_den; i_den = i_x;
        }
        i_nom = i_a1_num;
        i_den = i_a1_den;
    }

    *pi_dst_nom = i_nom;
    *pi_dst_den = i_den;

    return b_exact;
}

#undef vlc_execve
/*************************************************************************
 * vlc_execve: Execute an external program with a given environment,
 * wait until it finishes and return its standard output
 *************************************************************************/
int vlc_execve( vlc_object_t *p_object, int i_argc, char *const *ppsz_argv,
                char *const *ppsz_env, const char *psz_cwd,
                const char *p_in, size_t i_in,
                char **pp_data, size_t *pi_data )
{
    (void)i_argc; // <-- hmph
#ifdef HAVE_FORK
# define BUFSIZE 1024
    int fds[2], i_status;

    if (socketpair (PF_LOCAL, SOCK_STREAM, 0, fds))
        return -1;

    pid_t pid = -1;
    if ((fds[0] > 2) && (fds[1] > 2))
        pid = fork ();

    switch (pid)
    {
        case -1:
            msg_Err (p_object, "unable to fork (%m)");
            close (fds[0]);
            close (fds[1]);
            return -1;

        case 0:
        {
            sigset_t set;
            sigemptyset (&set);
            pthread_sigmask (SIG_SETMASK, &set, NULL);

            /* NOTE:
             * Like it or not, close can fail (and not only with EBADF)
             */
            if ((dup2 (fds[1], 0) == 0) && (dup2 (fds[1], 1) == 1)
             && ((psz_cwd == NULL) || (chdir (psz_cwd) == 0)))
                execve (ppsz_argv[0], ppsz_argv, ppsz_env);

            _exit (EXIT_FAILURE);
        }
    }

    close (fds[1]);

    *pi_data = 0;
    if (*pp_data)
        free (*pp_data);
    *pp_data = NULL;

    if (i_in == 0)
        shutdown (fds[0], SHUT_WR);

    while (!p_object->b_die)
    {
        struct pollfd ufd[1];
        memset (ufd, 0, sizeof (ufd));
        ufd[0].fd = fds[0];
        ufd[0].events = POLLIN;

        if (i_in > 0)
            ufd[0].events |= POLLOUT;

        if (poll (ufd, 1, 10) <= 0)
            continue;

        if (ufd[0].revents & ~POLLOUT)
        {
            char *ptr = realloc (*pp_data, *pi_data + BUFSIZE + 1);
            if (ptr == NULL)
                break; /* safely abort */

            *pp_data = ptr;

            ssize_t val = read (fds[0], ptr + *pi_data, BUFSIZE);
            switch (val)
            {
                case -1:
                case 0:
                    shutdown (fds[0], SHUT_RD);
                    break;

                default:
                    *pi_data += val;
            }
        }

        if (ufd[0].revents & POLLOUT)
        {
            ssize_t val = write (fds[0], p_in, i_in);
            switch (val)
            {
                case -1:
                case 0:
                    i_in = 0;
                    shutdown (fds[0], SHUT_WR);
                    break;

                default:
                    i_in -= val;
                    p_in += val;
            }
        }
    }

    close (fds[0]);

    while (waitpid (pid, &i_status, 0) == -1);

    if (WIFEXITED (i_status))
    {
        i_status = WEXITSTATUS (i_status);
        if (i_status)
            msg_Warn (p_object,  "child %s (PID %d) exited with error code %d",
                      ppsz_argv[0], (int)pid, i_status);
    }
    else
    if (WIFSIGNALED (i_status)) // <-- this should be redumdant a check
    {
        i_status = WTERMSIG (i_status);
        msg_Warn (p_object, "child %s (PID %d) exited on signal %d (%s)",
                  ppsz_argv[0], (int)pid, i_status, strsignal (i_status));
    }

#elif defined( WIN32 ) && !defined( UNDER_CE )
    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bFuncRetn = FALSE;
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    DWORD i_status;
    char *psz_cmd = NULL, *p_env = NULL, *p = NULL;
    char *const *ppsz_parser;
    int i_size;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    if ( !CreatePipe( &hChildStdoutRd, &hChildStdoutWr, &saAttr, 0 ) )
    {
        msg_Err( p_object, "stdout pipe creation failed" );
        return -1;
    }

    /* Ensure the read handle to the pipe for STDOUT is not inherited. */
    SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0 );

    /* Create a pipe for the child process's STDIN. */
    if ( !CreatePipe( &hChildStdinRd, &hChildStdinWr, &saAttr, 0 ) )
    {
        msg_Err( p_object, "stdin pipe creation failed" );
        return -1;
    }

    /* Ensure the write handle to the pipe for STDIN is not inherited. */
    SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0 );

    /* Set up members of the PROCESS_INFORMATION structure. */
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

    /* Set up members of the STARTUPINFO structure. */
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.wShowWindow = SW_HIDE;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    /* Set up the command line. */
    psz_cmd = malloc(32768);
    if( !psz_cmd )
        return -1;
    psz_cmd[0] = '\0';
    i_size = 32768;
    ppsz_parser = &ppsz_argv[0];
    while ( ppsz_parser[0] != NULL && i_size > 0 )
    {
        /* Protect the last argument with quotes ; the other arguments
         * are supposed to be already protected because they have been
         * passed as a command-line option. */
        if ( ppsz_parser[1] == NULL )
        {
            strncat( psz_cmd, "\"", i_size );
            i_size--;
        }
        strncat( psz_cmd, *ppsz_parser, i_size );
        i_size -= strlen( *ppsz_parser );
        if ( ppsz_parser[1] == NULL )
        {
            strncat( psz_cmd, "\"", i_size );
            i_size--;
        }
        strncat( psz_cmd, " ", i_size );
        i_size--;
        ppsz_parser++;
    }

    /* Set up the environment. */
    p = p_env = malloc(32768);
    if( !p )
    {
        free( psz_cmd );
        return -1;
    }

    i_size = 32768;
    ppsz_parser = &ppsz_env[0];
    while ( *ppsz_parser != NULL && i_size > 0 )
    {
        memcpy( p, *ppsz_parser,
                __MIN((int)(strlen(*ppsz_parser) + 1), i_size) );
        p += strlen(*ppsz_parser) + 1;
        i_size -= strlen(*ppsz_parser) + 1;
        ppsz_parser++;
    }
    *p = '\0';

    /* Create the child process. */
    bFuncRetn = CreateProcess( NULL,
          psz_cmd,       // command line
          NULL,          // process security attributes
          NULL,          // primary thread security attributes
          TRUE,          // handles are inherited
          0,             // creation flags
          p_env,
          psz_cwd,
          &siStartInfo,  // STARTUPINFO pointer
          &piProcInfo ); // receives PROCESS_INFORMATION

    free( psz_cmd );
    free( p_env );

    if ( bFuncRetn == 0 )
    {
        msg_Err( p_object, "child creation failed" );
        return -1;
    }

    /* Read from a file and write its contents to a pipe. */
    while ( i_in > 0 && !p_object->b_die )
    {
        DWORD i_written;
        if ( !WriteFile( hChildStdinWr, p_in, i_in, &i_written, NULL ) )
            break;
        i_in -= i_written;
        p_in += i_written;
    }

    /* Close the pipe handle so the child process stops reading. */
    CloseHandle(hChildStdinWr);

    /* Close the write end of the pipe before reading from the
     * read end of the pipe. */
    CloseHandle(hChildStdoutWr);

    /* Read output from the child process. */
    *pi_data = 0;
    if( *pp_data )
        free( pp_data );
    *pp_data = NULL;
    *pp_data = malloc( 1025 );  /* +1 for \0 */

    while ( !p_object->b_die )
    {
        DWORD i_read;
        if ( !ReadFile( hChildStdoutRd, &(*pp_data)[*pi_data], 1024, &i_read,
                        NULL )
              || i_read == 0 )
            break;
        *pi_data += i_read;
        *pp_data = xrealloc( *pp_data, *pi_data + 1025 );
    }

    while ( !p_object->b_die
             && !GetExitCodeProcess( piProcInfo.hProcess, &i_status )
             && i_status != STILL_ACTIVE )
        msleep( 10000 );

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    if ( i_status )
        msg_Warn( p_object,
                  "child %s returned with error code %ld",
                  ppsz_argv[0], i_status );

#else
    msg_Err( p_object, "vlc_execve called but no implementation is available" );
    return -1;

#endif

    if (*pp_data == NULL)
        return -1;

    (*pp_data)[*pi_data] = '\0';
    return 0;
}
