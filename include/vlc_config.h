/*****************************************************************************
 * vlc_config.h: limits and configuration
 * Defines all compilation-time configuration constants and size limits
 *****************************************************************************
 * Copyright (C) 1999-2003 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
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

#define CLOCK_FREQ 1000000


/* When creating or destroying threads in blocking mode, delay to poll thread
 * status */
#define THREAD_SLEEP                    ((mtime_t)(0.010*CLOCK_FREQ))

/*****************************************************************************
 * Interface configuration
 *****************************************************************************/

/* Base delay in micro second for interface sleeps */
#define INTF_IDLE_SLEEP                 ((mtime_t)(0.050*CLOCK_FREQ))

/* Step for changing gamma, and minimum and maximum values */
#define INTF_GAMMA_STEP                 .1
#define INTF_GAMMA_LIMIT                3

/*****************************************************************************
 * Input thread configuration
 *****************************************************************************/

#define DEFAULT_INPUT_ACTIVITY 1
#define TRANSCODE_ACTIVITY 10

/* Used in ErrorThread */
#define INPUT_IDLE_SLEEP                ((mtime_t)(0.100*CLOCK_FREQ))

/* Time to wait in case of read error */
#define INPUT_ERROR_SLEEP               ((mtime_t)(0.10*CLOCK_FREQ))

/* Number of read() calls needed until we check the file size through
 * fstat() */
#define INPUT_FSTAT_NB_READS            10

/*
 * General limitations
 */

/* Duration between the time we receive the data packet, and the time we will
 * mark it to be presented */
#define DEFAULT_PTS_DELAY               (mtime_t)(.3*CLOCK_FREQ)

/* DVD and VCD devices */
#if !defined( WIN32 ) && !defined( UNDER_CE )
#   define CD_DEVICE      "/dev/cdrom"
#   define DVD_DEVICE     "/dev/dvd"
#else
#   define CD_DEVICE      "D:"
#   define DVD_DEVICE     NULL
#endif
#define VCD_DEVICE        CD_DEVICE
#define CDAUDIO_DEVICE    CD_DEVICE

/*****************************************************************************
 * Audio configuration
 *****************************************************************************/

/* Volume */
/* If you are coding an interface, please see src/audio_output/intf.c */
#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_STEP                32
#define AOUT_VOLUME_MAX                 1024
#define AOUT_VOLUME_MIN                 0

/* Max number of pre-filters per input, and max number of post-filters */
#define AOUT_MAX_FILTERS                10

/* Max number of inputs */
#define AOUT_MAX_INPUTS                 5

/* Buffers which arrive in advance of more than AOUT_MAX_ADVANCE_TIME
 * will be considered as bogus and be trashed */
#define AOUT_MAX_ADVANCE_TIME           (mtime_t)(DEFAULT_PTS_DELAY * 5)

/* Buffers which arrive in advance of more than AOUT_MAX_PREPARE_TIME
 * will cause the calling thread to sleep */
#define AOUT_MAX_PREPARE_TIME           (mtime_t)(.5*CLOCK_FREQ)

/* Buffers which arrive after pts - AOUT_MIN_PREPARE_TIME will be trashed
 * to avoid too heavy resampling */
#define AOUT_MIN_PREPARE_TIME           (mtime_t)(.04*CLOCK_FREQ)

/* Max acceptable delay between the coded PTS and the actual presentation
 * time, without resampling */
#define AOUT_PTS_TOLERANCE              (mtime_t)(.04*CLOCK_FREQ)

/* Max acceptable resampling (in %) */
#define AOUT_MAX_RESAMPLING             10

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

/* Video heap size - remember that a decompressed picture is big
 * (~1 Mbyte) before using huge values */
#define VOUT_MAX_PICTURES               8

/* Minimum number of direct pictures the video output will accept without
 * creating additional pictures in system memory */
#define VOUT_MIN_DIRECT_PICTURES        6

/* Number of simultaneous subpictures */
#define VOUT_MAX_SUBPICTURES            8

/* Statistics are displayed every n loops (=~ pictures) */
#define VOUT_STATS_NB_LOOPS             100

/*
 * Time settings
 */

/* Time during which the thread will sleep if it has nothing to
 * display (in micro-seconds) */
#define VOUT_IDLE_SLEEP                 ((int)(0.020*CLOCK_FREQ))

/* Maximum lap of time allowed between the beginning of rendering and
 * display. If, compared to the current date, the next image is too
 * late, the thread will perform an idle loop. This time should be
 * at least VOUT_IDLE_SLEEP plus the time required to render a few
 * images, to avoid trashing of decoded images */
#define VOUT_DISPLAY_DELAY              ((int)(0.200*CLOCK_FREQ))

/* Pictures which are VOUT_BOGUS_DELAY or more in advance probably have
 * a bogus PTS and won't be displayed */
#define VOUT_BOGUS_DELAY                ((mtime_t)(DEFAULT_PTS_DELAY * 30))

/* Delay (in microseconds) before an idle screen is displayed */
#define VOUT_IDLE_DELAY                 (5*CLOCK_FREQ)

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                20

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE            ((mtime_t)(0.020*CLOCK_FREQ))

/* Time to sleep when waiting for a buffer (from vout or the video fifo).
 * It should be approximately the time needed to perform a complete picture
 * loop. Since it only happens when the video heap is full, it does not need
 * to be too low, even if it blocks the decoder. */
#define VOUT_OUTMEM_SLEEP               ((mtime_t)(0.020*CLOCK_FREQ))

/* The default video output window title */
#define VOUT_TITLE                      "VLC"

/*****************************************************************************
 * Messages and console interfaces configuration
 *****************************************************************************/

/* Maximal size of a message to be stored in the mesage queue,
 * it is needed when vasprintf is not available */
#define INTF_MAX_MSG_SIZE               512

/* Maximal size of the message queue - in case of overflow, all messages in the
 * queue are printed, but not sent to the threads */
#define VLC_MSG_QSIZE                   256

/* Maximal depth of the object tree output by vlc_dumpstructure */
#define MAX_DUMPSTRUCTURE_DEPTH         100
