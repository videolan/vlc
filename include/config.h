/*****************************************************************************
 * config.h: limits and configuration
 * Defines all compilation-time configuration constants and size limits
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
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
#define THREAD_SLEEP                    ((int)(0.010*CLOCK_FREQ))

/* When a thread waits on a condition in debug mode, delay to wait before
 * outputting an error message (in second) */
#define THREAD_COND_TIMEOUT             5

/*****************************************************************************
 * Interface configuration
 *****************************************************************************/

/* Base delay in micro second for interface sleeps */
#define INTF_IDLE_SLEEP                 ((int)(0.050*CLOCK_FREQ))

/* Step for changing gamma, and minimum and maximum values */
#define INTF_GAMMA_STEP                 .1
#define INTF_GAMMA_LIMIT                3

/*****************************************************************************
 * Input thread configuration
 *****************************************************************************/

/* XXX?? */
#define INPUT_IDLE_SLEEP                ((int)(0.100*CLOCK_FREQ))

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
#define INPUT_CHANNEL_CHANGE_DELAY         (mtime_t)(5*CLOCK_FREQ)

/* Duration between the time we receive the data packet, and the time we will
 * mark it to be presented */
#define DEFAULT_PTS_DELAY               (mtime_t)(.2*CLOCK_FREQ)

/*****************************************************************************
 * Audio configuration
 *****************************************************************************/

/* Maximum number of audio output threads */
#define AOUT_MAX_THREADS                10

/* Default audio output format (AOUT_FMT_S16_NE = Native Endianess) */
#define AOUT_FORMAT_DEFAULT             AOUT_FMT_S16_NE
/* #define AOUT_FORMAT_DEFAULT          AOUT_FMT_S8 */
/* #define AOUT_FORMAT_DEFAULT          AOUT_FMT_U8 */
/* #define AOUT_FORMAT_DEFAULT          AOUT_FMT_S16_BE */
/* #define AOUT_FORMAT_DEFAULT          AOUT_FMT_S16_LE */
/* #define AOUT_FORMAT_DEFAULT          AOUT_FMT_U16_BE */
/* #define AOUT_FORMAT_DEFAULT          AOUT_FMT_U16_LE */

/* Volume */
#define VOLUME_DEFAULT                  512
#define VOLUME_STEP                     128
#define VOLUME_MAX                      1024

/* Number of audio output frames contained in an audio output fifo.
 * (AOUT_FIFO_SIZE + 1) must be a power of 2, in order to optimise the
 * %(AOUT_FIFO_SIZE + 1) operation with an &AOUT_FIFO_SIZE.
 * With 511 we have at least 511*384/2/48000=2 seconds of sound */
#define AOUT_FIFO_SIZE                  511

/* Maximum number of audio fifos. The value of AOUT_MAX_FIFOS should be a power
 * of two, in order to optimize the '/AOUT_MAX_FIFOS' and '*AOUT_MAX_FIFOS'
 * operations with '>>' and '<<' (gcc changes this at compilation-time) */
#define AOUT_MAX_FIFOS                  2

/* Duration (in microseconds) of an audio output buffer should be :
 * - short, in order to be able to play a new song very quickly (especially a
 *   song from the interface)
 * - long, in order to perform the buffer calculations as few as possible */
#define AOUT_BUFFER_DURATION            100000

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

/* Number of simultaneous subpictures */
#define VOUT_MAX_SUBPICTURES            8

/* Maximum number of active areas in a rendering buffer. Active areas are areas
 * of the picture which need to be cleared before re-using the buffer. If a
 * picture, including its many additions such as subtitles, additionnal user
 * informations and interface, has too many active areas, some of them are
 * joined. */
#define VOUT_MAX_AREAS                  5

/* Default fonts */
#define VOUT_DEFAULT_FONT               "default8x9.psf"
#define VOUT_LARGE_FONT                 "default8x16.psf"

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
#define VOUT_DISPLAY_DELAY              ((int)(0.500*CLOCK_FREQ))

/* Delay (in microseconds) before an idle screen is displayed */
#define VOUT_IDLE_DELAY                 (5*CLOCK_FREQ)

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                20

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE            ((int)(0.020*CLOCK_FREQ))

/* Time to sleep when waiting for a buffer (from vout or the video fifo).
 * It should be approximately the time needed to perform a complete picture
 * loop. Since it only happens when the video heap is full, it does not need
 * to be too low, even if it blocks the decoder. */
#define VOUT_OUTMEM_SLEEP               ((int)(0.020*CLOCK_FREQ))

/* The default video output window title */
#define VOUT_TITLE                      "VideoLAN Client " VERSION

/*****************************************************************************
 * Video parser configuration
 *****************************************************************************/

#define VPAR_IDLE_SLEEP                 ((int)(0.010*CLOCK_FREQ))

/* Optimization level, from 0 to 2 - 1 is generally a good compromise. Remember
 * that raising this level dramatically lengthens the compilation time. */
#if defined( HAVE_RELEASE ) || defined( __pentiumpro__ )
#   define VPAR_OPTIM_LEVEL             2
#else
#   define VPAR_OPTIM_LEVEL             1
#endif

/* Maximum number of macroblocks in a picture. */
#define MAX_MB                          2048

/*****************************************************************************
 * Video decoder configuration
 *****************************************************************************/

#define VDEC_IDLE_SLEEP                 ((int)(0.100*CLOCK_FREQ))

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
 * it is needed when vasprintf is not avalaible */
#define INTF_MAX_MSG_SIZE               512

/* Maximal size of the message queue - in case of overflow, all messages in the
 * queue are printed, but not sent to the threads */
#define INTF_MSG_QSIZE                  256


/****************************************************************************
 * Macros for the names of the main options
 * Instead of directly manipulating the option names, we define macros for
 * them. This makes sense only for the main options (ie. only the ones defined
 * in main.c) because they are widely used.
 * We won't bother doing this for plugins as plugin specific options should
 * by definition be restricted in useage to the plugin that defines them.
 *
 ****************************************************************************/

/*
 * Interface option names
 */

/* Variable containing the display method */
#define INTF_METHOD_VAR                 "intf"
/* Variable used to store startup script */
#define INTF_INIT_SCRIPT_VAR            "vlcrc"
/* Default search path for interface file browser */
#define INTF_PATH_VAR                   "search_path"
/* Interface warnig message level */
#define INTF_WARNING_VAR                "warning"
/* Variable to enable stats mode */
#define INTF_STATS_VAR                  "stats"

/*
 * Audio output option names
 */

/* Variable to disable the audio output */
#define AOUT_NOAUDIO_VAR                "noaudio"
/* Variable containing the audio output method */
#define AOUT_METHOD_VAR                 "aout"
/* Variable for spdif mode */
#define AOUT_SPDIF_VAR                  "spdif"
/* Variable for volume */
#define AOUT_VOLUME_VAR                 "volume"
/* Variable for mono */
#define AOUT_MONO_VAR                   "audio_mono"
/* Variable for output rate */
#define AOUT_RATE_VAR                   "audio_rate"
/* Variable for output rate */
#define AOUT_DESYNC_VAR                 "audio_desync"

/*
 * Video output option names
 */

/* Variable to disable the video output */
#define VOUT_NOVIDEO_VAR                "novideo"
/* Variable containing the display method */
#define VOUT_METHOD_VAR                 "vout"
/* Variable used in place of DISPLAY if available */
#define VOUT_DISPLAY_VAR                "display"
/* Dimensions for display window */
#define VOUT_WIDTH_VAR                  "width"
#define VOUT_HEIGHT_VAR                 "height"
/* Variable for grayscale output mode */
#define VOUT_GRAYSCALE_VAR              "grayscale"
/* Variable for fullscreen mode */
#define VOUT_FULLSCREEN_VAR             "fullscreen"
/* Variable for overlay mode */
#define VOUT_NOOVERLAY_VAR              "nooverlay"
/* Variable containing the filter method */
#define VOUT_FILTER_VAR                 "filter"
/* Variable containing the SPU margin */
#define VOUT_SPUMARGIN_VAR              "spumargin"

/*
 * Input option names
 */

/* Variable containing the input method */
#define INPUT_METHOD_VAR                "input"
/* Input port */
#define INPUT_PORT_VAR                  "server_port"
/* Channels mode */
#define INPUT_NETWORK_CHANNEL_VAR       "network_channel"
/* Variable containing channel server and port */
#define INPUT_CHANNEL_SERVER_VAR        "channel_server"
#define INPUT_CHANNEL_PORT_VAR          "channel_port"
/* Variable containing network interface */
#define INPUT_IFACE_VAR                 "iface"

#define INPUT_TITLE_VAR                 "input_title"
#define INPUT_CHAPTER_VAR               "input_chapter"
#define INPUT_ANGLE_VAR                 "input_angle"
#define INPUT_AUDIO_VAR                 "input_audio"
#define INPUT_CHANNEL_VAR               "input_channel"
#define INPUT_SUBTITLE_VAR              "input_subtitle"
/* DVD defaults */
#define INPUT_DVD_DEVICE_VAR            "dvd_device"
/* VCD defaults */
#define INPUT_VCD_DEVICE_VAR            "vcd_device"

/*
 * Decoders option names
 */

/* Variables for audio decoders */
#define ADEC_MPEG_VAR                   "mpeg_adec"
#define ADEC_AC3_VAR                    "ac3_adec"
/* The synchro variable name */
#define VPAR_SYNCHRO_VAR                "vpar_synchro"
/* Variable containing the SMP value */
#define VDEC_SMP_VAR                    "vdec_smp"

/*
 * Playlist option names
 */

/* Launch on start-up */
#define PLAYLIST_STARTUP_VAR            "playlist_on_startup"
/* Enqueue drag'n dropped item */
#define PLAYLIST_ENQUEUE_VAR            "playlist_enqueue"
/* Loop on playlist end */
#define PLAYLIST_LOOP_VAR               "playlist_loop"

/*
 * CPU options
 */
#define NOMMX_VAR                       "nommx"
#define NO3DN_VAR                       "no3dn"
#define NOMMXEXT_VAR                    "nommxext"
#define NOSSE_VAR                       "nosse"
#define NOALTIVEC_VAR                   "noaltivec"

/*
 * Misc option names
 */

/* Variable containing the memcpy method */
#define MEMCPY_METHOD_VAR               "memcpy"
