/******************************************************************************
 * spu_decoder.h : sub picture unit decoder thread interface
 * (c)1999 VideoLAN
 ******************************************************************************/

/******************************************************************************
 * spudec_thread_t : sub picture unit decoder thread descriptor
 ******************************************************************************/
typedef struct spudec_thread_s
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

} spudec_thread_t;

/******************************************************************************
 * Prototypes
 ******************************************************************************/
spudec_thread_t *       spudec_CreateThread( input_thread_t * p_input );
void                    spudec_DestroyThread( spudec_thread_t * p_spudec );

