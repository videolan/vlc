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
#ifdef AUDIO_DUMMY
int  aout_DummySysOpen          ( aout_thread_t *p_aout );
int  aout_DummySysReset         ( aout_thread_t *p_aout );
int  aout_DummySysSetFormat     ( aout_thread_t *p_aout );
int  aout_DummySysSetChannels   ( aout_thread_t *p_aout );
int  aout_DummySysSetRate       ( aout_thread_t *p_aout );
long aout_DummySysGetBufInfo    ( aout_thread_t *p_aout, long l_buffer_info );
void aout_DummySysPlaySamples   ( aout_thread_t *p_aout, byte_t *buffer, int i_size );
void aout_DummySysClose         ( aout_thread_t *p_aout );
#endif
#ifdef AUDIO_DSP
int  aout_DspSysOpen            ( aout_thread_t *p_aout );
int  aout_DspSysReset           ( aout_thread_t *p_aout );
int  aout_DspSysSetFormat       ( aout_thread_t *p_aout );
int  aout_DspSysSetChannels     ( aout_thread_t *p_aout );
int  aout_DspSysSetRate         ( aout_thread_t *p_aout );
long aout_DspSysGetBufInfo      ( aout_thread_t *p_aout, long l_buffer_info );
void aout_DspSysPlaySamples     ( aout_thread_t *p_aout, byte_t *buffer, int i_size );
void aout_DspSysClose           ( aout_thread_t *p_aout );
#endif
