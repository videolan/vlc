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
int  aout_DummySysOpen          ( aout_sys_t *p_sys );
int  aout_DummySysReset         ( aout_sys_t *p_sys );
int  aout_DummySysSetFormat     ( aout_sys_t *p_sys );
int  aout_DummySysSetChannels   ( aout_sys_t *p_sys );
int  aout_DummySysSetRate       ( aout_sys_t *p_sys );
long aout_DummySysGetBufInfo    ( aout_sys_t *p_sys );
void aout_DummySysPlaySamples   ( aout_sys_t *p_sys, byte_t *buffer, int i_size );
void aout_DummySysClose         ( aout_sys_t *p_sys );
#ifdef AUDIO_DSP
int  aout_DspSysOpen            ( aout_sys_t *p_sys );
int  aout_DspSysReset           ( aout_sys_t *p_sys );
int  aout_DspSysSetFormat       ( aout_sys_t *p_sys );
int  aout_DspSysSetChannels     ( aout_sys_t *p_sys );
int  aout_DspSysSetRate         ( aout_sys_t *p_sys );
long aout_DspSysGetBufInfo      ( aout_sys_t *p_sys );
void aout_DspSysPlaySamples     ( aout_sys_t *p_dsp, byte_t *buffer, int i_size );
void aout_DspSysClose           ( aout_sys_t *p_sys );
#endif
