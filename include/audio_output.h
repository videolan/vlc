/******************************************************************************
 * audio_output.h : audio output thread interface
 * (c)1999 VideoLAN
 ******************************************************************************
 * Required headers:
 * - <pthread.h>                                                  ( pthread_t )
 * - <sys/soundcard.h>                                       ( audio_buf_info )
 * - "common.h"                                                   ( boolean_t )
 * - "mtime.h"                                                      ( mtime_t )
 ******************************************************************************/

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

/* Default audio output format (AFMT_S16_NE = Native Endianess) */
#define AOUT_DEFAULT_FORMAT     AFMT_S16_NE

/* Default stereo mode (0 stands for mono, 1 for stereo) */
#define AOUT_DEFAULT_STEREO     1

/* Audio output rate, in Hz */
#define AOUT_MIN_RATE           22050 /* ?? */
#define AOUT_DEFAULT_RATE       44100
#define AOUT_MAX_RATE           48000

/* Number of audio samples (s16 integers) contained in an audio output frame...
 * - Layer I        : a decoded frame contains 384 samples
 * - Layer II & III : a decoded frame contains 1152 = 3*384 samples */
#define AOUT_FRAME_SIZE         384

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

/******************************************************************************
 * aout_dsp_t
 ******************************************************************************/
typedef struct
{
    /* Path to the audio output device (default is set to "/dev/dsp") */
    char *              psz_device;
    int                 i_fd;

    /* Format of the audio output samples (see <sys/soundcard.h>) */
    int                 i_format;
    /* Following boolean is set to 0 if output sound is mono, 1 if stereo */
    boolean_t           b_stereo;
    /* Rate of the audio output sound (in Hz) */
    long                l_rate;

    /* Buffer information structure, used by aout_dspGetBufInfo() to store the
     * current state of the internal sound card buffer */
    audio_buf_info      buf_info;

} aout_dsp_t;

/******************************************************************************
 * aout_increment_t
 ******************************************************************************
 * This structure is used to keep the progression of an index up-to-date, in
 * order to avoid rounding problems and heavy computations, as the function
 * that handles this structure only uses additions.
 ******************************************************************************/
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

/******************************************************************************
 * aout_frame_t
 ******************************************************************************/
typedef s16 aout_frame_t[ AOUT_FRAME_SIZE ];

/******************************************************************************
 * aout_fifo_t
 ******************************************************************************/
typedef struct
{
    /* See the fifo types below */
    int                 i_type;
    boolean_t           b_die;

    boolean_t           b_stereo;
    long                l_rate;

    pthread_mutex_t     data_lock;
    pthread_cond_t      data_wait;

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

/******************************************************************************
 * aout_thread_t
 ******************************************************************************/
typedef struct aout_thread_s
{
    pthread_t           thread_id;
    boolean_t           b_die;

    aout_dsp_t          dsp;

    pthread_mutex_t     fifos_lock;
    aout_fifo_t         fifo[ AOUT_MAX_FIFOS ];

    void *              buffer;
    /* The s32 buffer is used to mix all the audio fifos together before
     * converting them and storing them in the audio output buffer */
    s32 *               s32_buffer;

    /* The size of the audio output buffer is kept in audio units, as this is
     * the only unit that is common with every audio decoder and audio fifo */
    long                l_units;

    mtime_t             date;
    /* date is the moment where the first audio unit of the output buffer
     * should be played and is kept up-to-date with the following incremental
     * structure */
    aout_increment_t    date_increment;

} aout_thread_t;

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int             aout_Open               ( aout_thread_t *p_aout );
int             aout_SpawnThread        ( aout_thread_t *p_aout );
void            aout_CancelThread       ( aout_thread_t *p_aout );
void            aout_Close              ( aout_thread_t *p_aout );

aout_fifo_t *   aout_CreateFifo         ( aout_thread_t *p_aout, aout_fifo_t *p_fifo );
void            aout_DestroyFifo        ( aout_fifo_t *p_fifo );
