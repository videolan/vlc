/******************************************************************************
 * audio_decoder.h : audio decoder thread interface
 * (c)1999 VideoLAN
 ******************************************************************************
 * = Prototyped functions are implemented in audio_decoder/audio_decoder.c
 *
 * = Required headers :
 *   - <pthread.h>                                                ( pthread_t )
 *   - "common.h"                                    ( u32, byte_t, boolean_t )
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
 * bit_fifo_t : bit fifo descriptor
 ******************************************************************************
 * This type describes a bit fifo used to store bits while working with the
 * input stream at the bit level.
 ******************************************************************************/
typedef struct bit_fifo_s
{
    /* This unsigned integer allows us to work at the bit level. This buffer
     * can contain 32 bits, and the used space can be found on the MSb's side
     * and the available space on the LSb's side. */
    u32                 buffer;

    /* Number of bits available in the bit buffer */
    int                 i_available;

} bit_fifo_t;

/******************************************************************************
 * bit_stream_t : bit stream descriptor
 ******************************************************************************
 * This type, based on a PES stream, includes all the structures needed to
 * handle the input stream like a bit stream.
 ******************************************************************************/
typedef struct bit_stream_s
{
    /*
     * Input structures
     */
    /* The input thread feeds the stream with fresh PES packets */
    input_thread_t *    p_input;
    /* The decoder fifo contains the data of the PES stream */
    decoder_fifo_t *    p_decoder_fifo;

    /*
     * Byte structures
     */
    /* Current TS packet (in the current PES packet of the PES stream) */
    ts_packet_t *       p_ts;
    /* Index of the next byte that is to be read (in the current TS packet) */
    unsigned int        i_byte;

    /*
     * Bit structures
     */
    bit_fifo_t          fifo;

} bit_stream_t;

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
    pthread_t           thread_id;                /* id for pthread functions */
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

/******************************************************************************
 * Prototypes
 ******************************************************************************/
adec_thread_t * adec_CreateThread       ( input_thread_t * p_input /* !! , aout_thread_t * p_aout !! */ );
void            adec_DestroyThread      ( adec_thread_t * p_adec );
