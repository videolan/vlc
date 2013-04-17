/*****************************************************************************
 * vlc_config.h: limits and configuration
 * Defines all compilation-time configuration constants and size limits
 *****************************************************************************
 * Copyright (C) 1999-2003 VLC authors and VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
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

/**
 * \file
 * This file defines of values used in interface, vout, aout and vlc core functions.
 */

/* Conventions regarding names of symbols and variables
 * ----------------------------------------------------
 *
 * - Symbols should begin with a prefix indicating in which module they are
 *   used, such as INTF_, VOUT_ or AOUT_.
 */

/*****************************************************************************
 * General configuration
 *****************************************************************************/

/* All timestamp below or equal to this define are invalid/unset
 * XXX the numerical value is 0 because of historical reason and will change.*/
#define VLC_TS_INVALID (0)
#define VLC_TS_0 (1)

#define CLOCK_FREQ INT64_C(1000000)

/*****************************************************************************
 * Interface configuration
 *****************************************************************************/

/* Base delay in micro second for interface sleeps */
#define INTF_IDLE_SLEEP                 (CLOCK_FREQ/20)

/*****************************************************************************
 * Input thread configuration
 *****************************************************************************/

/* Used in ErrorThread */
#define INPUT_IDLE_SLEEP                (CLOCK_FREQ/10)

/*
 * General limitations
 */

/* Duration between the time we receive the data packet, and the time we will
 * mark it to be presented */
#define DEFAULT_PTS_DELAY               (3*CLOCK_FREQ/10)

/*****************************************************************************
 * SPU configuration
 *****************************************************************************/

/* Buffer must avoid arriving more than SPU_MAX_PREPARE_TIME in advanced to
 * the SPU */
#define SPU_MAX_PREPARE_TIME            (CLOCK_FREQ/2)

/*****************************************************************************
 * Video configuration
 *****************************************************************************/

/*
 * Default settings for video output threads
 */

/* Multiplier value for aspect ratio calculation (2^7 * 3^3 * 5^3) */
#define VOUT_ASPECT_FACTOR              432000

/* Maximum width of a scaled source picture - this should be relatively high,
 * since higher stream values will result in no display at all. */
#define VOUT_MAX_WIDTH                  4096

/* Number of planes in a picture */
#define VOUT_MAX_PLANES                 5

/*
 * Time settings
 */

/* Time to sleep when waiting for a buffer (from vout or the video fifo).
 * It should be approximately the time needed to perform a complete picture
 * loop. Since it only happens when the video heap is full, it does not need
 * to be too low, even if it blocks the decoder. */
#define VOUT_OUTMEM_SLEEP               (CLOCK_FREQ/50)

/* The default video output window title */
#define VOUT_TITLE                      "VLC"

/*****************************************************************************
 * Messages and console interfaces configuration
 *****************************************************************************/

/* Maximal depth of the object tree output by vlc_dumpstructure */
#define MAX_DUMPSTRUCTURE_DEPTH         100
