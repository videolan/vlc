/*****************************************************************************
 * vlc_config.h: limits and configuration
 * Defines all compilation-time configuration constants and size limits
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* Conventions regarding names of symbols and variables
 * ----------------------------------------------------
 *
 * - Symbols should begin with a prefix indicating in which module they are
 *   used, such as INTF_, VOUT_ or ADEC_.
 */

/*****************************************************************************
 * General configuration
 *****************************************************************************/

#define CLOCK_FREQ 1000000


/* Automagically spawn audio and video decoder threads */
#define AUTO_SPAWN

/* When creating or destroying threads in blocking mode, delay to poll thread
 * status */
#define THREAD_SLEEP                    ((mtime_t)(0.010*CLOCK_FREQ))

/* When a thread waits on a condition in debug mode, delay to wait before
 * outputting an error message (in second) */
#define THREAD_COND_TIMEOUT             1

/* The configuration file and directory */
#ifdef SYS_BEOS
#  define CONFIG_DIR                    "config/settings/VideoLAN Client"
#elif SYS_DARWIN
#  define CONFIG_DIR                    "Library/Preferences/VLC"
#elif defined( WIN32 ) || defined( UNDER_CE )
#  define CONFIG_DIR                    "vlc"
#else
#  define CONFIG_DIR                    ".vlc"
#endif
#define CONFIG_FILE                     "vlcrc"
#define PLUGINSCACHE_DIR                "cache"

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

/* Maximum number of input threads - this value is used exclusively by
 * interface, and is in fact an interface limitation */
#define INPUT_MAX_THREADS               10

/* Maximum size of a data packet (128 kB) */
#define INPUT_MAX_PACKET_SIZE           131072

/* Maximum length of a pre-parsed chunk (4 MB) */
#define INPUT_PREPARSE_LENGTH           4194304

/* Maximum length of a hostname or source name */
#define INPUT_MAX_SOURCE_LENGTH         100

/* Maximum memory the input is allowed to use (20 MB) */
#define INPUT_MAX_ALLOCATION            20971520

/*
 * Channel method
 */

/* Delay between channel changes - this is required to avoid flooding the
 * channel server */
#define INPUT_CHANNEL_CHANGE_DELAY      (mtime_t)(5*CLOCK_FREQ)

/* Duration between the time we receive the data packet, and the time we will
 * mark it to be presented */
#define DEFAULT_PTS_DELAY               (mtime_t)(.3*CLOCK_FREQ)

/* DVD and VCD devices */
#if !defined( WIN32 ) && !defined( UNDER_CE )
#   define DVD_DEVICE "/dev/dvd"
#   define VCD_DEVICE "/dev/cdrom"
#   define CDAUDIO_DEVICE "/dev/cdrom"
#else
#   define DVD_DEVICE "D:"
#   define VCD_DEVICE "D:"
#   define CDAUDIO_DEVICE "D:"
#endif

/*****************************************************************************
 * Audio configuration
 *****************************************************************************/

/* Maximum number of audio output threads */
#define AOUT_MAX_THREADS                10

/* Volume */
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
#define AOUT_MAX_ADVANCE_TIME           (mtime_t)(DEFAULT_PTS_DELAY * 3)

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

/* Maximum number of video output threads */
#define VOUT_MAX_THREADS                256

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

/* Maximum number of active areas in a rendering buffer. Active areas are areas
 * of the picture which need to be cleared before re-using the buffer. If a
 * picture, including its many additions such as subtitles, additionnal user
 * information and interface, has too many active areas, some of them are
 * joined. */
#define VOUT_MAX_AREAS                  5

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
#define VOUT_BOGUS_DELAY                ((mtime_t)(DEFAULT_PTS_DELAY * 3))

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
 * Video parser configuration
 *****************************************************************************/

#define VPAR_IDLE_SLEEP                 ((mtime_t)(0.010*CLOCK_FREQ))

/* Optimization level, from 0 to 2 - 1 is generally a good compromise. Remember
 * that raising this level dramatically lengthens the compilation time. */
#if defined( HAVE_RELEASE )
#   define VPAR_OPTIM_LEVEL             2
#else
#   define VPAR_OPTIM_LEVEL             1
#endif

/* Maximum number of macroblocks in a picture. */
#define MAX_MB                          2048

/*****************************************************************************
 * Video decoder configuration
 *****************************************************************************/

#define VDEC_IDLE_SLEEP                 ((mtime_t)(0.100*CLOCK_FREQ))

/* Maximum range of values out of the IDCT + motion compensation. */
#define VDEC_CROPRANGE                  2048

/* No SMP by default, since it slows down things on non-smp machines. */
#define VDEC_SMP_DEFAULT                0

/* Nice increments for decoders -- necessary for x11 scheduling */
#define VDEC_NICE                       3

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
