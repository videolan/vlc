/*****************************************************************************
 * audio_output.h : audio output thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 * Michel Kaempf <maxx@via.ecp.fr>
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

/*****************************************************************************
 * Required headers:
 * - "common.h"                                                   ( boolean_t )
 * - "mtime.h"                                                      ( mtime_t )
 * - "threads.h"                                               ( vlc_thread_t )
 *****************************************************************************/

/* TODO :
 *
 * - Créer un flag destroy dans les fifos audio pour indiquer au thread audio
 *   qu'il peut libérer la mémoire occupée par le buffer de la fifo lorsqu'il
 *   le désire (fin du son ou fin du thread)
 * - Redéplacer les #define dans config.h
 *
 */

/*
 * Defines => "config.h"
 */

/* Default output device. You probably should not change this. */
#define AOUT_DEFAULT_DEVICE     "/dev/dsp"

/* Default audio output format (AOUT_FMT_S16_NE = Native Endianess) */
#define AOUT_DEFAULT_FORMAT     AOUT_FMT_S16_NE

/* #define AOUT_DEFAULT_FORMAT     AOUT_FMT_S8 */
/* #define AOUT_DEFAULT_FORMAT     AOUT_FMT_U8 */
/* #define AOUT_DEFAULT_FORMAT     AOUT_FMT_S16_BE */
/* #define AOUT_DEFAULT_FORMAT     AOUT_FMT_S16_LE */
/* #define AOUT_DEFAULT_FORMAT     AOUT_FMT_U16_BE */
/* #define AOUT_DEFAULT_FORMAT     AOUT_FMT_U16_LE */


/* Default stereo mode (0 stands for mono, 1 for stereo) */
#define AOUT_DEFAULT_STEREO     1
/* #define AOUT_DEFAULT_STEREO     0 */

/* Audio output rate, in Hz */
#define AOUT_MIN_RATE           22050 /* XXX?? */
#define AOUT_DEFAULT_RATE       44100
#define AOUT_MAX_RATE           48000


/* Volume (default 100) */
#define VOL 100
#define VOLSTEP 5
#define VOLMAX 300

/* Number of audio output frames contained in an audio output fifo.
 * (AOUT_FIFO_SIZE + 1) must be a power of 2, in order to optimise the
 * %(AOUT_FIFO_SIZE + 1) operation with an &AOUT_FIFO_SIZE.
 * With 511 we have at least 511*384/2/48000=2 seconds of sound */
#define AOUT_FIFO_SIZE          511

/* Maximum number of audio fifos. The value of AOUT_MAX_FIFOS should be a power
 * of two, in order to optimize the '/AOUT_MAX_FIFOS' and '*AOUT_MAX_FIFOS'
 * operations with '>>' and '<<' (gcc changes this at compilation-time) */
#define AOUT_MAX_FIFOS          2

/* Duration (in microseconds) of an audio output buffer should be :
 * - short, in order to be able to play a new song very quickly (especially a
 *   song from the interface)
 * - long, in order to perform the buffer calculations as few as possible */
#define AOUT_BUFFER_DURATION    100000

/*
 * Macros
 */
#define AOUT_FIFO_ISEMPTY( fifo )       ( (fifo).l_end_frame == (fifo).i_start_frame )
#define AOUT_FIFO_ISFULL( fifo )        ( ((((fifo).l_end_frame + 1) - (fifo).l_start_frame) & AOUT_FIFO_SIZE) == 0 )

/*****************************************************************************
 * aout_increment_t
 *****************************************************************************
 * This structure is used to keep the progression of an index up-to-date, in
 * order to avoid rounding problems and heavy computations, as the function
 * that handles this structure only uses additions.
 *****************************************************************************/
typedef struct
{
    /* The remainder is used to keep track of the fractional part of the
     * index. */
    long                l_remainder;

    /*
     * The increment structure is initialized with the result of an euclidean
     * division :
     *
     *  l_euclidean_numerator                           l_euclidean_remainder
     * ----------------------- = l_euclidean_integer + -----------------------
     * l_euclidean_denominator                         l_euclidean_denominator
     *
     */
    long                l_euclidean_integer;
    long                l_euclidean_remainder;
    long                l_euclidean_denominator;

} aout_increment_t;

/*****************************************************************************
 * aout_fifo_t
 *****************************************************************************/
typedef struct
{
    /* See the fifo types below */
    int                 i_type;
    boolean_t           b_die;

    int                 i_channels;
    boolean_t           b_stereo;
    long                l_rate;

    vlc_mutex_t         data_lock;
    vlc_cond_t          data_wait;

    long                l_frame_size;
    void *              buffer;
    mtime_t *           date;
    /* The start frame is the first frame in the buffer that contains decoded
     * audio data. It it also the first frame in the current timestamped frame
     * area, ie the first dated frame in the decoded part of the buffer. :-p */
    long                l_start_frame;
    boolean_t           b_start_frame;
    /* The next frame is the end frame of the current timestamped frame area,
     * ie the first dated frame after the start frame. */
    long                l_next_frame;
    boolean_t           b_next_frame;
    /* The end frame is the first frame, after the start frame, that doesn't
     * contain decoded audio data. That's why the end frame is the first frame
     * where the audio decoder can store its decoded audio frames. */
    long                l_end_frame;

    long                l_unit;
    aout_increment_t    unit_increment;
    /* The following variable is used to store the number of remaining audio
     * units in the current timestamped frame area. */
    long                l_units;

} aout_fifo_t;

#define AOUT_EMPTY_FIFO         0
#define AOUT_INTF_MONO_FIFO     1
#define AOUT_INTF_STEREO_FIFO   2
#define AOUT_ADEC_MONO_FIFO     3
#define AOUT_ADEC_STEREO_FIFO   4

/*****************************************************************************
 * aout_thread_t : audio output thread descriptor
 *****************************************************************************/
typedef int  (aout_sys_open_t)           ( p_aout_thread_t p_aout );
typedef int  (aout_sys_reset_t)          ( p_aout_thread_t p_aout );
typedef int  (aout_sys_setformat_t)      ( p_aout_thread_t p_aout );
typedef int  (aout_sys_setchannels_t)    ( p_aout_thread_t p_aout );
typedef int  (aout_sys_setrate_t)        ( p_aout_thread_t p_aout );
typedef long (aout_sys_getbufinfo_t)     ( p_aout_thread_t p_aout,
                                           long l_buffer_limit );
typedef void (aout_sys_playsamples_t)    ( p_aout_thread_t p_aout,
                                           byte_t *buffer, int i_size );
typedef void (aout_sys_close_t)          ( p_aout_thread_t p_aout );

typedef struct aout_thread_s
{
    vlc_thread_t        thread_id;
    boolean_t           b_die;
    boolean_t           b_active;

    vlc_mutex_t         fifos_lock;
    aout_fifo_t         fifo[ AOUT_MAX_FIFOS ];

    /* Plugins */
    aout_sys_open_t *           p_sys_open;
    aout_sys_reset_t *          p_sys_reset;
    aout_sys_setformat_t *      p_sys_setformat;
    aout_sys_setchannels_t *    p_sys_setchannels;
    aout_sys_setrate_t *        p_sys_setrate;
    aout_sys_getbufinfo_t *     p_sys_getbufinfo;
    aout_sys_playsamples_t *    p_sys_playsamples;
    aout_sys_close_t *          p_sys_close;

    void *              buffer;
    /* The s32 buffer is used to mix all the audio fifos together before
     * converting them and storing them in the audio output buffer */
    s32 *               s32_buffer;

    /* The size of the audio output buffer is kept in audio units, as this is
     * the only unit that is common with every audio decoder and audio fifo */
    long                l_units;
    long                l_msleep;

    /* date is the moment where the first audio unit of the output buffer
     * will be played */
    mtime_t             date;

    /* Path to the audio output device (default is set to "/dev/dsp") */
    char *              psz_device;
    int                 i_fd;

    /* Format of the audio output samples */
    int                 i_format;
    /* Number of channels */
    int                 i_channels;
    /* Mono or Stereo sound */
    boolean_t           b_stereo;
    /* Rate and gain of the audio output sound (in Hz) */
    long                l_rate;
    long                l_gain;

    /* there might be some useful private structure, such as audio_buf_info
     * for the OSS output */
    p_aout_sys_t        p_sys;


    /* there is the current volume */
    int                 vol;

} aout_thread_t;

/* Those are from <linux/soundcard.h> but are needed because of formats
 * on other platforms */
#define AOUT_FMT_U8          0x00000008
#define AOUT_FMT_S16_LE      0x00000010           /* Little endian signed 16 */
#define AOUT_FMT_S16_BE      0x00000020              /* Big endian signed 16 */
#define AOUT_FMT_S8          0x00000040
#define AOUT_FMT_U16_LE      0x00000080                 /* Little endian U16 */
#define AOUT_FMT_U16_BE      0x00000100                    /* Big endian U16 */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define AOUT_FMT_S16_NE      AOUT_FMT_S16_LE
#elif __BYTE_ORDER == __BIG_ENDIAN
#define AOUT_FMT_S16_NE      AOUT_FMT_S16_BE
#endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
aout_thread_t * aout_CreateThread       ( int *pi_status );
void            aout_DestroyThread      ( aout_thread_t *p_aout, int *pi_status );


aout_fifo_t *   aout_CreateFifo         ( aout_thread_t *p_aout, aout_fifo_t *p_fifo );
void            aout_DestroyFifo        ( aout_fifo_t *p_fifo );
