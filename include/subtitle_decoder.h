/******************************************************************************
 * subtitle_decoder.h : subtitle decoder thread interface
 * (c)1999 VideoLAN
 ******************************************************************************/

/******************************************************************************
 * subtdec_thread_t : subtitle decoder thread descriptor
 ******************************************************************************/
typedef struct subtdec_thread_s
{
    /*
     * Thread properties and locks
     */
    boolean_t           b_die;                                  /* `die' flag */
    boolean_t           b_run;                                  /* `run' flag */
    boolean_t           b_active;                            /* `active' flag */
    boolean_t           b_error;                              /* `error' flag */
    vlc_thread_t        thread_id;                 /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t      fifo;                   /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Decoder properties
     */
    unsigned int        total_bits_read;
    /* ... */

} subtdec_thread_t;

/******************************************************************************
 * Prototypes
 ******************************************************************************/
subtdec_thread_t *      subtdec_CreateThread( input_thread_t * p_input );
void                    subtdec_DestroyThread( subtdec_thread_t * p_subtdec );
