/*******************************************************************************
 * file.h: file streams functions interface
 * (c)1999 VideoLAN
 *******************************************************************************/

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int     input_FileCreateMethod( input_thread_t *p_input ,
                                input_cfg_t *p_cfg );
int     input_FileRead( input_thread_t *p_input, const struct iovec *p_vector,
                        size_t i_count );
void    input_FileDestroyMethod( input_thread_t *p_input );
