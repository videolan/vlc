/*****************************************************************************
 * audio_sys.h : header of the method-dependant functions library
 * (c)1999 VideoLAN
 *****************************************************************************
 * Required headers:
 * - "common.h" ( byte_t )
 * - "audio_output.h" ( aout_dsp_t )
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  aout_SysOpen            ( aout_thread_t *p_aout );
int  aout_SysReset           ( aout_thread_t *p_aout );
int  aout_SysSetFormat       ( aout_thread_t *p_aout );
int  aout_SysSetChannels     ( aout_thread_t *p_aout );
int  aout_SysSetRate         ( aout_thread_t *p_aout );
long aout_SysGetBufInfo      ( aout_thread_t *p_aout, long l_buffer_info );
void aout_SysPlaySamples     ( aout_thread_t *p_aout, byte_t *buffer, int i_size );
void aout_SysClose           ( aout_thread_t *p_aout );

