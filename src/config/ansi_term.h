/*****************************************************************************
 * ansi_term.h: Common declarations and helpers for ANSI terminal handling
 *****************************************************************************
 * Copyright (C) 1998-2011 VLC authors and VideoLAN
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

#ifndef VLC_ANSI_TERM_H
#define VLC_ANSI_TERM_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if !defined( _WIN32 )
# include <termios.h>
# include <sys/ioctl.h>
#endif

/* ANSI terminal control ("escape") sequences */

/* Terminal control sequence construction */
#define term_seq(x) "\033[" #x "m"

/**
 * Codes:
 *
 * Effects:
 *   - Normal:    0 (reset)
 *   - Bold:      1
 *   - Dim:       2
 *   - Italic:    3
 *   - Underline: 4
 *   - Reverse:   7
 *   - Invisible: 8
 *   - Strike:    9 (Strike-through)
 *
 * Color set 1:
 *   - Black:   30
 *   - Red      31
 *   - Green    32
 *   - Yellow:  33
 *   - Blue:    34
 *   - Magenta: 35
 *   - Cyan:    36
 *   - White:   37
 *
 * Color set 2:
 *   - Black:   90
 *   - Red:     91
 *   - Green:   92
 *   - Yellow:  93
 *   - Blue:    94
 *   - Magenta: 95
 *   - Cyan:    96
 *   - White:   97
 *
 * Text background color highlighting, set 1:
 *   - Black:   40
 *   - Red:     41
 *   - Green:   42
 *   - Yellow:  43
 *   - Blue:    44
 *   - Magenta: 45
 *   - Cyan:    46
 *   - White:   47
 *
 * Text background color highlighting, set 2:
 *   - Black:   100
 *   - Red:     101
 *   - Green:   102
 *   - Yellow:  103
 *   - Blue:    104
 *   - Magenta: 105
 *   - Cyan:    106
 *   - White:   107
 */

#define TS_RESET term_seq(0)

#define TS_RESET_BOLD term_seq(0;1)

#define TS_BOLD       term_seq(1)
#define TS_DIM        term_seq(2)
#define TS_ITALIC     term_seq(3)
#define TS_UNDERSCORE term_seq(4)
#define TS_REVERSE    term_seq(7)
#define TS_INVISIBLE  term_seq(8)
#define TS_STRIKE     term_seq(9)

#define TS_BLACK   term_seq(30)
#define TS_RED     term_seq(31)
#define TS_GREEN   term_seq(32)
#define TS_YELLOW  term_seq(33)
#define TS_BLUE    term_seq(34)
#define TS_MAGENTA term_seq(35)
#define TS_CYAN    term_seq(36)
#define TS_WHITE   term_seq(37)

#define TS_BLACK_BOLD   term_seq(30;1)
#define TS_RED_BOLD     term_seq(31;1)
#define TS_GREEN_BOLD   term_seq(32;1)
#define TS_YELLOW_BOLD  term_seq(33;1)
#define TS_BLUE_BOLD    term_seq(34;1)
#define TS_MAGENTA_BOLD term_seq(35;1)
#define TS_CYAN_BOLD    term_seq(36;1)
#define TS_WHITE_BOLD   term_seq(37;1)

#endif
