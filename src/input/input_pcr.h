/*****************************************************************************
 * input_pcr.h: PCR management interface
 * (c)1999 VideoLAN
 *****************************************************************************/

/* Maximum number of samples used to compute the dynamic average value,
 * it is also the maximum of c_average in the pcr_descriptor_struct.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1) */
#define PCR_MAX_AVERAGE_COUNTER 40

/* Maximum gap allowed between two PCRs. */
#define PCR_MAX_GAP 1000000

/* synchro states */
#define SYNCHRO_NOT_STARTED 1
#define SYNCHRO_START       2
#define SYNCHRO_REINIT      3

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int            input_PcrInit        ( input_thread_t *p_input );
void           input_PcrDecode      ( input_thread_t *p_input, es_descriptor_t* p_es,
                                       u8* p_pcr_data );
void           input_PcrEnd         ( input_thread_t *p_input );
