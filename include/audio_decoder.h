/******************************************************************************
 * audio_decoder.h : audio decoder thread interface
 * (c)1999 VideoLAN
 ******************************************************************************
 * = Prototyped functions are implemented in audio_decoder/audio_decoder.c
 *
 * = Required headers :
 *   - "common.h"                                    ( u32, byte_t, boolean_t )
 *   - "vlc_thread.h"                                          ( vlc_thread_t )
 *   - "input.h"                                ( ts_packet_t, input_thread_t )
 *   - "decoder_fifo.h"                                      ( decoder_fifo_t )
 *   - "audio_output.h"                          ( aout_fifo_t, aout_thread_t )
 *
 * = - LSb = Least Significant bit
 *   - LSB = Least Significant Byte
 *
 * = - MSb = Most Significant bit
 *   - MSB = Most Significant Byte
 ******************************************************************************/

/*
 * TODO :
 * - Etudier /usr/include/asm/bitops.h d'un peu plus près, bien qu'il ne me
 *   semble pas être possible d'utiliser ces fonctions ici
 * - N'y aurait-t-il pas moyen de se passer d'un buffer de bits, en travaillant
 *   directement sur le flux PES ?
 */

/******************************************************************************
 * adec_bank_t
 ******************************************************************************/
typedef struct adec_bank_s
{
    float               v1[512];
    float               v2[512];
    float *             actual;
    int                 pos;

} adec_bank_t;

/******************************************************************************
 * adec_thread_t : audio decoder thread descriptor
 ******************************************************************************
 * This type describes an audio decoder thread
 ******************************************************************************/
typedef struct adec_thread_s
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                 /* id for thread functions */
    boolean_t           b_die;                                  /* `die' flag */
    boolean_t           b_error;                              /* `error' flag */

    /*
     * Input properties
     */
    decoder_fifo_t      fifo;                   /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Decoder properties
     */
    adec_bank_t         bank_0;
    adec_bank_t         bank_1;

    /*
     * Output properties
     */
    aout_fifo_t *       p_aout_fifo;  /* stores the decompressed audio frames */
    aout_thread_t *     p_aout;            /* needed to create the audio fifo */

} adec_thread_t;

#define AOUT_FRAME_SIZE 384

/******************************************************************************
 * aout_frame_t
 ******************************************************************************/
typedef s16 aout_frame_t[ AOUT_FRAME_SIZE ];

/******************************************************************************
 * Prototypes
 ******************************************************************************/
adec_thread_t * adec_CreateThread       ( input_thread_t * p_input /* !! , aout_thread_t * p_aout !! */ );
void            adec_DestroyThread      ( adec_thread_t * p_adec );
