/*******************************************************************************
 * input_network.h: network functions interface
 * (c)1999 VideoLAN
 *******************************************************************************/

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int input_NetworkOpen   ( input_thread_t *p_input );
int input_NetworkRead   ( input_thread_t *p_input, const struct iovec *p_vector,
                          size_t i_count );
void input_NetworkClose ( input_thread_t *p_input );
