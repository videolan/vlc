/*****************************************************************************
 * ac3_decoder.h : ac3 decoder thread interface
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * ac3dec_thread_t : ac3 decoder thread descriptor
 *****************************************************************************/
typedef struct ac3dec_thread_s
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_error;                             /* `error' flag */

    /*
     * Input properties
     */
    decoder_fifo_t      fifo;                  /* stores the PES stream data */
    input_thread_t *    p_input;
    ts_packet_t *       p_ts;

    /*
     * Decoder properties
     */

    ac3dec_t            ac3_decoder;

    /*
     * Output properties
     */
    aout_fifo_t *       p_aout_fifo; /* stores the decompressed audio frames */
    aout_thread_t *     p_aout;           /* needed to create the audio fifo */

} ac3dec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
ac3dec_thread_t *       ac3dec_CreateThread( input_thread_t * p_input );
void                    ac3dec_DestroyThread( ac3dec_thread_t * p_ac3dec );
