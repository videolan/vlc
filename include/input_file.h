/*****************************************************************************
 * input_file.h: file streams functions interface
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     input_FileOpen  ( input_thread_t *p_input );
int     input_FileRead  ( input_thread_t *p_input, const struct iovec *p_vector,
                          size_t i_count );
void    input_FileClose ( input_thread_t *p_input );
