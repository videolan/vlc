/******************************************************************************
 * audio_output.c : audio output thread
 * (c)1999 VideoLAN
 ******************************************************************************/

/* TODO:
 *
 * - Passer un certain nombre de "fonctions" (genre add_samples) en macro ou
 *   inline
 * - Faire les optimisations dans les fonctions threads :
 *   = Stocker les "petits calculs" dans des variables au lieu de les refaire
 *     à chaque boucle
 *   = Utiliser des tables pour les gros calculs
 * - Faire une structure différente pour intf/adec fifo
 * - Rajouter des pthread_cond_signal ?
 *
 */

/******************************************************************************
 * Preamble
 ******************************************************************************/
#include <unistd.h>

#include <pthread.h>
#include <sys/soundcard.h>
#include <stdio.h>                                            /* "intf_msg.h" */
#include <stdlib.h>                             /* calloc(), malloc(), free() */

#include "common.h"
#include "config.h"
#include "mtime.h"                              /* mtime_t, mdate(), msleep() */

#include "intf_msg.h"                         /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"
#include "audio_dsp.h"

/******************************************************************************
 * Local prototypes
 ******************************************************************************/

/* Creating as much aout_Thread functions as configurations is one solution,
 * examining the different cases in the Thread loop of an unique function is
 * another. I chose the first solution. */
void aout_Thread_S8_Mono        ( aout_thread_t * p_aout );
void aout_Thread_U8_Mono        ( aout_thread_t * p_aout );
void aout_Thread_S16_Mono       ( aout_thread_t * p_aout );
void aout_Thread_U16_Mono       ( aout_thread_t * p_aout );
void aout_Thread_S8_Stereo      ( aout_thread_t * p_aout );
void aout_Thread_U8_Stereo      ( aout_thread_t * p_aout );
void aout_Thread_S16_Stereo     ( aout_thread_t * p_aout );
void aout_Thread_U16_Stereo     ( aout_thread_t * p_aout );

static __inline__ void InitializeIncrement( aout_increment_t * p_increment, long l_numerator, long l_denominator );
static __inline__ int NextFrame( aout_thread_t * p_aout, aout_fifo_t * p_fifo );

/******************************************************************************
 * aout_Open
 ******************************************************************************/
int aout_Open( aout_thread_t * p_aout )
{
    if ( aout_dspOpen( &p_aout->dsp ) )
    {
        return( -1 );
    }

    if ( aout_dspReset( &p_aout->dsp ) )
    {
	aout_dspClose( &p_aout->dsp );
	return( -1 );
    }

    if ( aout_dspSetFormat( &p_aout->dsp ) )
    {
	aout_dspClose( &p_aout->dsp );
	return( -1 );
    }

    if ( aout_dspSetChannels( &p_aout->dsp ) )
    {
	aout_dspClose( &p_aout->dsp );
        return( -1 );
    }

    if ( aout_dspSetRate( &p_aout->dsp ) )
    {
	aout_dspClose( &p_aout->dsp );
	return( -1 );
    }

    intf_DbgMsg("aout debug: audio device (%s) opened (format=%i, stereo=%i, rate=%li)\n",
        p_aout->dsp.psz_device,
        p_aout->dsp.i_format,
        p_aout->dsp.b_stereo, p_aout->dsp.l_rate);

    return( 0 );
}

/******************************************************************************
 * aout_SpawnThread
 ******************************************************************************/
int aout_SpawnThread( aout_thread_t * p_aout )
{
    int         i_fifo;
    long        l_bytes;
    s64         s64_numerator, s64_denominator;
    void *      aout_thread = NULL;

    intf_DbgMsg("aout debug: spawning audio output thread (%p)\n", p_aout);

    /* We want the audio output thread to live */
    p_aout->b_die = 0;

    /* Initialize the fifos lock */
    pthread_mutex_init( &p_aout->fifos_lock, NULL );
    /* Initialize audio fifos : set all fifos as empty and initialize locks */
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
        pthread_mutex_init( &p_aout->fifo[i_fifo].data_lock, NULL );
        pthread_cond_init( &p_aout->fifo[i_fifo].data_wait, NULL );
    }

    /* Compute the size (in audio units) of the audio output buffer. Although
     * AOUT_BUFFER_DURATION is given in microseconds, the output rate is given
     * in Hz, that's why we need to divide by 10^6 microseconds (1 second) */
    p_aout->l_units = (long)( ((s64)p_aout->dsp.l_rate * AOUT_BUFFER_DURATION) / 1000000 );

    /* Make aout_thread point to the right thread function, and compute the
     * byte size of the audio output buffer */
    switch ( p_aout->dsp.b_stereo )
    {
        /* Audio output is mono */
        case 0:
            switch ( p_aout->dsp.i_format )
            {
                case AFMT_U8:
                    l_bytes = 1 * sizeof(u8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U8_Mono;
                    break;

                case AFMT_S8:
                    l_bytes = 1 * sizeof(s8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S8_Mono;
                    break;

                case AFMT_U16_LE:
                case AFMT_U16_BE:
                    l_bytes = 1 * sizeof(u16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U16_Mono;
                    break;

                case AFMT_S16_LE:
                case AFMT_S16_BE:
                    l_bytes = 1 * sizeof(s16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S16_Mono;
                    break;

                default:
                    intf_ErrMsg("aout error: unknown audio output format (%i)\n",
                        p_aout->dsp.i_format);
		    return( -1 );
            }
            break;

        /* Audio output is stereo */
        case 1:
            switch ( p_aout->dsp.i_format )
            {
                case AFMT_U8:
                    l_bytes = 2 * sizeof(u8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U8_Stereo;
                    break;

                case AFMT_S8:
                    l_bytes = 2 * sizeof(s8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S8_Stereo;
                    break;

                case AFMT_U16_LE:
                case AFMT_U16_BE:
                    l_bytes = 2 * sizeof(u16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U16_Stereo;
                    break;

                case AFMT_S16_LE:
                case AFMT_S16_BE:
                    l_bytes = 2 * sizeof(s16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S16_Stereo;
                    break;

                default:
                    intf_ErrMsg("aout error: unknown audio output format (%i)\n",
                        p_aout->dsp.i_format);
                    return( -1 );
            }
            break;

        default:
            intf_ErrMsg("aout error: unknown number of audio channels (%i)\n",
                p_aout->dsp.b_stereo + 1);
            return( -1 );
    }

    /* Allocate the memory needed by the audio output buffers, and set to zero
     * the s32 buffer's memory */
    if ( (p_aout->buffer = malloc(l_bytes)) == NULL )
    {
        intf_ErrMsg("aout error: not enough memory to create the output buffer\n");
        return( -1 );
    }
    if ( (p_aout->s32_buffer = (s32 *)calloc(p_aout->l_units, sizeof(s32) << p_aout->dsp.b_stereo)) == NULL )
    {
        intf_ErrMsg("aout error: not enough memory to create the s32 output buffer\n");
        free( p_aout->buffer );
        return( -1 );
    }

    /* Initialize the incremental structure that is used to work out the date
     * of the first audio unit in the output buffer */
    s64_numerator = (s64)p_aout->l_units * 1000000;
    s64_denominator = (s64)p_aout->dsp.l_rate;

    p_aout->date_increment.l_remainder = -(long)s64_denominator;

    p_aout->date_increment.l_euclidean_integer = 0;
    while ( s64_numerator >= s64_denominator )
    {
        p_aout->date_increment.l_euclidean_integer++;
        s64_numerator -= s64_denominator;
    }

    p_aout->date_increment.l_euclidean_remainder = (long)s64_numerator;

    p_aout->date_increment.l_euclidean_denominator = (long)s64_denominator;

    /* Before launching the thread, we try to predict the date of the first
     * audio unit in the first output buffer */
    p_aout->date = mdate();

    /* Launch the thread */
    if ( pthread_create( &p_aout->thread_id, NULL, aout_thread, p_aout ) )
    {
        intf_ErrMsg("aout error: can't spawn audio output thread (%p)\n", p_aout);
        free( p_aout->buffer );
        free( p_aout->s32_buffer );
        return( -1 );
    }

    intf_DbgMsg("aout debug: audio output thread (%p) spawned\n", p_aout);
    return( 0 );
}

/******************************************************************************
 * aout_CancelThread
 ******************************************************************************/
void aout_CancelThread( aout_thread_t * p_aout )
{
    intf_DbgMsg("aout debug: requesting termination of audio output thread (%p)\n", p_aout);

    /* Ask thread to kill itself and wait until it's done */
    p_aout->b_die = 1;
    pthread_join( p_aout->thread_id, NULL );

    /* Free the allocated memory */
    free( p_aout->buffer );
    free( p_aout->s32_buffer );
}

/******************************************************************************
 * aout_Close
 ******************************************************************************/
void aout_Close( aout_thread_t * p_aout )
{
    aout_dspClose( &p_aout->dsp );
    intf_DbgMsg("aout debug: audio device (%s) closed\n", p_aout->dsp.psz_device);
}

/******************************************************************************
 * aout_CreateFifo
 ******************************************************************************/
aout_fifo_t * aout_CreateFifo( aout_thread_t * p_aout, aout_fifo_t * p_fifo )
{
    int i_fifo;

    /* Take the fifos lock */
    pthread_mutex_lock( &p_aout->fifos_lock );

    /* Looking for a free fifo structure */
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        if ( p_aout->fifo[i_fifo].i_type == AOUT_EMPTY_FIFO)
	{
            break;
        }
    }
    if ( i_fifo == AOUT_MAX_FIFOS )
    {
        intf_ErrMsg("aout error: no empty fifo available\n");
        pthread_mutex_unlock( &p_aout->fifos_lock );
        return( NULL );
    }

    /* Initialize the new fifo structure */
    switch ( p_aout->fifo[i_fifo].i_type = p_fifo->i_type )
    {
        case AOUT_INTF_MONO_FIFO:
        case AOUT_INTF_STEREO_FIFO:
	    p_aout->fifo[i_fifo].b_die = 0;

	    p_aout->fifo[i_fifo].b_stereo = p_fifo->b_stereo;
	    p_aout->fifo[i_fifo].l_rate = p_fifo->l_rate;

            p_aout->fifo[i_fifo].buffer = p_fifo->buffer;

            p_aout->fifo[i_fifo].l_unit = 0;
            InitializeIncrement( &p_aout->fifo[i_fifo].unit_increment, p_fifo->l_rate, p_aout->dsp.l_rate );
            p_aout->fifo[i_fifo].l_units = p_fifo->l_units;
            break;

        case AOUT_ADEC_MONO_FIFO:
        case AOUT_ADEC_STEREO_FIFO:
            p_aout->b_die = 0;

            p_aout->fifo[i_fifo].b_stereo = p_fifo->b_stereo;
            p_aout->fifo[i_fifo].l_rate = p_fifo->l_rate;

            /* Allocate the memory needed to store the audio frames. As the
             * fifo is a rotative fifo, we must be able to find out whether the
             * fifo is full or empty, that's why we must in fact allocate memory
             * for (AOUT_FIFO_SIZE+1) audio frames. */
	    if ( (p_aout->fifo[i_fifo].buffer = malloc( sizeof(s16)*(AOUT_FIFO_SIZE+1)*AOUT_FRAME_SIZE )) == NULL )
	    {
                intf_ErrMsg("aout error: not enough memory to create the frames buffer\n");
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
	        pthread_mutex_unlock( &p_aout->fifos_lock );
	        return( NULL );
	    }

            /* Allocate the memory needed to store the dates of the frames */
            if ( (p_aout->fifo[i_fifo].date = (mtime_t *)malloc( sizeof(mtime_t)*(AOUT_FIFO_SIZE+1) )) == NULL )
            {
                intf_ErrMsg("aout error: not enough memory to create the dates buffer\n");
                free( p_aout->fifo[i_fifo].buffer );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                pthread_mutex_unlock( &p_aout->fifos_lock );
                return( NULL );
            }

            /* Set the fifo's buffer as empty (the first frame that is to be
             * played is also the first frame that is not to be played) */
            p_aout->fifo[i_fifo].l_start_frame = 0;
            /* p_aout->fifo[i_fifo].l_next_frame = 0; */
            p_aout->fifo[i_fifo].l_end_frame = 0;

            /* Waiting for the audio decoder to compute enough frames to work
             * out the fifo's current rate (as soon as the decoder has decoded
             * enough frames, the members of the fifo structure that are not
             * initialized now will be calculated) */
            p_aout->fifo[i_fifo].l_unit = 0; /* !! */
            p_aout->fifo[i_fifo].b_start_frame = 0;
            p_aout->fifo[i_fifo].b_next_frame = 0;
            break;

        default:
            intf_ErrMsg("aout error: unknown fifo type (%i)\n", p_aout->fifo[i_fifo].i_type);
            p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
            pthread_mutex_unlock( &p_aout->fifos_lock );
            return( NULL );
    }

    /* Release the fifos lock */
    pthread_mutex_unlock( &p_aout->fifos_lock );

    /* Return the pointer to the fifo structure */
    intf_DbgMsg("aout debug: audio output fifo (%p) allocated\n", &p_aout->fifo[i_fifo]);
    return( &p_aout->fifo[i_fifo] );
}

/******************************************************************************
 * aout_DestroyFifo
 ******************************************************************************/
void aout_DestroyFifo( aout_fifo_t * p_fifo )
{
    intf_DbgMsg("aout debug: requesting destruction of audio output fifo (%p)\n", p_fifo);
    p_fifo->b_die = 1;
}

/* Here are the local macros */

#define S32_TO_S16( sample ) \
    (s16)( (sample) )

#define UPDATE_INCREMENT( increment, integer ) \
    if ( ((increment).l_remainder += (increment).l_euclidean_remainder) >= 0 ) \
    { \
        (integer) += (increment).l_euclidean_integer + 1; \
        (increment).l_remainder -= (increment).l_euclidean_denominator; \
    } \
    else \
    { \
        (integer) += (increment).l_euclidean_integer; \
    }

/* Following functions are local */

/******************************************************************************
 * InitializeIncrement
 ******************************************************************************/
static __inline__ void InitializeIncrement( aout_increment_t * p_increment, long l_numerator, long l_denominator )
{
    p_increment->l_remainder = -l_denominator;

    p_increment->l_euclidean_integer = 0;
    while ( l_numerator >= l_denominator )
    {
        p_increment->l_euclidean_integer++;
        l_numerator -= l_denominator;
    }

    p_increment->l_euclidean_remainder = l_numerator;

    p_increment->l_euclidean_denominator = l_denominator;
}

/******************************************************************************
 * NextFrame
 ******************************************************************************/
static __inline__ int NextFrame( aout_thread_t * p_aout, aout_fifo_t * p_fifo )
{
    long l_units, l_rate;

    /* We take the lock */
    pthread_mutex_lock( &p_fifo->data_lock );

    /* Are we looking for a dated start frame ? */
    if ( !p_fifo->b_start_frame )
    {
        while ( p_fifo->l_start_frame != p_fifo->l_end_frame )
        {
            if ( p_fifo->date[p_fifo->l_start_frame] != LAST_MDATE )
            {
                p_fifo->b_start_frame = 1;
                p_fifo->l_next_frame = (p_fifo->l_start_frame + 1) & AOUT_FIFO_SIZE;
                break;
            }
            p_fifo->l_start_frame = (p_fifo->l_start_frame + 1) & AOUT_FIFO_SIZE;
        }
        if ( p_fifo->l_start_frame == p_fifo->l_end_frame )
        {
            pthread_mutex_unlock( &p_fifo->data_lock );
            return( -1 );
        }
    }

    /* We are looking for the next dated frame */
    while ( p_fifo->l_next_frame != p_fifo->l_end_frame )
    {
        if ( p_fifo->date[p_fifo->l_next_frame] != LAST_MDATE )
        {
            p_fifo->b_next_frame = 1;
            break;
        }
        p_fifo->l_next_frame = (p_fifo->l_next_frame + 1) & AOUT_FIFO_SIZE;        
    }
    if ( p_fifo->l_next_frame == p_fifo->l_end_frame )
    {
        if ( (((p_fifo->l_end_frame + 1) - p_fifo->l_start_frame) & AOUT_FIFO_SIZE) == 0 )
        {
            p_fifo->l_start_frame = 0;
            p_fifo->b_start_frame = 0;
            /* p_fifo->l_next_frame = 0; */
            /* p_fifo->b_next_frame = 0; */
            p_fifo->l_end_frame = 0;
        }
        pthread_mutex_unlock( &p_fifo->data_lock );
        return( -1 );
    }

    l_units = ((p_fifo->l_next_frame - p_fifo->l_start_frame) & AOUT_FIFO_SIZE)
        * (AOUT_FRAME_SIZE >> p_fifo->b_stereo);

    l_rate = (long)( ((mtime_t)l_units * 1000000)
        / (p_fifo->date[p_fifo->l_next_frame] - p_fifo->date[p_fifo->l_start_frame]) );

    InitializeIncrement( &p_fifo->unit_increment, l_rate, p_aout->dsp.l_rate );

    p_fifo->l_units = (((l_units - (p_fifo->l_unit -
        (p_fifo->l_start_frame * (AOUT_FRAME_SIZE >> p_fifo->b_stereo))))
        * p_aout->dsp.l_rate) / l_rate) + 1;

    /* We release the lock before leaving */
    pthread_mutex_unlock( &p_fifo->data_lock );
    return( 0 );
}

void aout_Thread_S8_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_S8_Stereo( aout_thread_t * p_aout )
{
}

void aout_Thread_U8_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_U8_Stereo( aout_thread_t * p_aout )
{
}

void aout_Thread_S16_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_S16_Stereo( aout_thread_t * p_aout )
{
    int i_fifo;
    long l_units;
    long l_buffer, l_buffer_limit;

    intf_DbgMsg("adec debug: running audio output thread (%p) (pid == %i)\n", p_aout, getpid());

    /* As the s32_buffer was created with calloc(), we don't have to set this
     * memory to zero and we can immediately jump into the thread's loop */
    while ( !p_aout->b_die )
    {
        pthread_mutex_lock( &p_aout->fifos_lock );
        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
	{
            switch ( p_aout->fifo[i_fifo].i_type )
	    {
	        case AOUT_EMPTY_FIFO:
                    break;

	        case AOUT_INTF_MONO_FIFO:
                    if ( p_aout->fifo[i_fifo].l_units > p_aout->l_units )
		    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->l_units << 1) ) /* p_aout->dsp.b_stereo == 1 */
			{
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        p_aout->fifo[i_fifo].l_units -= p_aout->l_units;
                    }
                    else
		    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->fifo[i_fifo].l_units << 1) ) /* p_aout->dsp.b_stereo == 1 */
			{
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        free( p_aout->fifo[i_fifo].buffer ); /* !! */
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]); /* !! */
                    }
                    break;

                case AOUT_INTF_STEREO_FIFO:
                    if ( p_aout->fifo[i_fifo].l_units > p_aout->l_units )
		    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->l_units << 1) ) /* p_aout->dsp.b_stereo == 1 */
			{
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        p_aout->fifo[i_fifo].l_units -= p_aout->l_units;
                    }
                    else
		    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->fifo[i_fifo].l_units << 1) ) /* p_aout->dsp.b_stereo */
			{
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        free( p_aout->fifo[i_fifo].buffer ); /* !! */
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]); /* !! */
                    }
                    break;

                case AOUT_ADEC_MONO_FIFO:
                    l_units = p_aout->l_units;
                    l_buffer = 0;
                    while ( l_units > 0 )
                    {
                        if ( !p_aout->fifo[i_fifo].b_next_frame )
                        {
                            if ( NextFrame(p_aout, &p_aout->fifo[i_fifo]) )
                            {
                                break;
                            }
                        }

                        if ( p_aout->fifo[i_fifo].l_units > l_units )
                        {
                            l_buffer_limit = p_aout->l_units << 1; /* p_aout->dsp.b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                     ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 0)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                        ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 0));
                                }
                            }
                            p_aout->fifo[i_fifo].l_units -= l_units;
                            break;
                        }
                        else
                        {
                            l_buffer_limit = l_buffer + (p_aout->fifo[i_fifo].l_units << 1);
                            /* p_aout->dsp.b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                     ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 0)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                        ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 0));
                                }
                            }
                            l_units -= p_aout->fifo[i_fifo].l_units;

                            pthread_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                            p_aout->fifo[i_fifo].l_start_frame = p_aout->fifo[i_fifo].l_next_frame;
                            pthread_cond_signal( &p_aout->fifo[i_fifo].data_wait );
                            pthread_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                            /* p_aout->fifo[i_fifo].b_start_frame = 1; */
                            p_aout->fifo[i_fifo].l_next_frame += 1;
                            p_aout->fifo[i_fifo].l_next_frame &= AOUT_FIFO_SIZE;
                            p_aout->fifo[i_fifo].b_next_frame = 0;
                        }
                    }
                    break;

                case AOUT_ADEC_STEREO_FIFO:
                    l_units = p_aout->l_units;
                    l_buffer = 0;
                    while ( l_units > 0 )
                    {
                        if ( !p_aout->fifo[i_fifo].b_next_frame )
                        {
                            if ( NextFrame(p_aout, &p_aout->fifo[i_fifo]) )
                            {
                                break;
                            }
                        }

                        if ( p_aout->fifo[i_fifo].l_units > l_units )
                        {
                            l_buffer_limit = p_aout->l_units << 1; /* p_aout->dsp.b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                     ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 1)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                        ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 1));
                                }
                            }
                            p_aout->fifo[i_fifo].l_units -= l_units;
                            break;
                        }
                        else
                        {
                            l_buffer_limit = l_buffer + (p_aout->fifo[i_fifo].l_units << 1);
                            /* p_aout->dsp.b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                     ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 1)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                        ((AOUT_FIFO_SIZE + 1) * (AOUT_FRAME_SIZE >> 1));
                                }
                            }
                            l_units -= p_aout->fifo[i_fifo].l_units;

                            pthread_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                            p_aout->fifo[i_fifo].l_start_frame = p_aout->fifo[i_fifo].l_next_frame;
                            pthread_cond_signal( &p_aout->fifo[i_fifo].data_wait );
                            pthread_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                            /* p_aout->fifo[i_fifo].b_start_frame = 1; */
                            p_aout->fifo[i_fifo].l_next_frame += 1;
                            p_aout->fifo[i_fifo].l_next_frame &= AOUT_FIFO_SIZE;
                            p_aout->fifo[i_fifo].b_next_frame = 0;
                        }
                    }
                    break;

	        default:
                    intf_DbgMsg("aout debug: unknown fifo type (%i)\n", p_aout->fifo[i_fifo].i_type);
                    break;
            }
        }
        pthread_mutex_unlock( &p_aout->fifos_lock );

        l_buffer_limit = p_aout->l_units << 1; /* p_aout->dsp.b_stereo == 1 */

        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )
	{
            ((s16 *)p_aout->buffer)[l_buffer] = S32_TO_S16( p_aout->s32_buffer[l_buffer] / AOUT_MAX_FIFOS );
            p_aout->s32_buffer[l_buffer] = 0;
        }

        aout_dspGetBufInfo( &p_aout->dsp );
        if ( p_aout->dsp.buf_info.fragments == p_aout->dsp.buf_info.fragstotal ) /* ?? */
	{
            aout_dspPlaySamples( &p_aout->dsp, (byte_t *)p_aout->buffer, sizeof(s16) * l_buffer_limit );
            p_aout->date = mdate();
        }
        else if ( p_aout->dsp.buf_info.bytes >=
            ((p_aout->dsp.buf_info.fragsize * p_aout->dsp.buf_info.fragstotal) -
            (sizeof(s16) * l_buffer_limit)) )
	{
            aout_dspPlaySamples( &p_aout->dsp, (byte_t *)p_aout->buffer, sizeof(s16) * l_buffer_limit );
        }
        else
	{
            aout_dspPlaySamples( &p_aout->dsp, (byte_t *)p_aout->buffer, sizeof(s16) * l_buffer_limit );
            msleep( p_aout->date_increment.l_euclidean_integer );
        }
        UPDATE_INCREMENT( p_aout->date_increment, p_aout->date )
    }

    pthread_mutex_lock( &p_aout->fifos_lock );
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        switch ( p_aout->fifo[i_fifo].i_type )
        {
            case AOUT_EMPTY_FIFO:
                break;

            case AOUT_INTF_MONO_FIFO:
            case AOUT_INTF_STEREO_FIFO:
                free( p_aout->fifo[i_fifo].buffer ); /* !! */
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                break;

            case AOUT_ADEC_MONO_FIFO:
            case AOUT_ADEC_STEREO_FIFO:
                free( p_aout->fifo[i_fifo].buffer );
                free( p_aout->fifo[i_fifo].date );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                break;

            default:
                break;
        }
    }
    pthread_mutex_unlock( &p_aout->fifos_lock );
}

void aout_Thread_U16_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_U16_Stereo( aout_thread_t * p_aout )
{
}
