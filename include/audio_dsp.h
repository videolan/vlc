/******************************************************************************
 * audio_dsp.h : header of the dsp functions library
 * (c)1999 VideoLAN
 ******************************************************************************
 * Required headers:
 * - "common.h" ( byte_t )
 * - "audio_output.h" ( aout_dsp_t )
 ******************************************************************************/

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int  aout_dspOpen       ( aout_dsp_t *p_dsp );
int  aout_dspReset      ( aout_dsp_t *p_dsp );
int  aout_dspSetFormat  ( aout_dsp_t *p_dsp );
int  aout_dspSetChannels( aout_dsp_t *p_dsp );
int  aout_dspSetRate    ( aout_dsp_t *p_dsp );
void aout_dspGetBufInfo ( aout_dsp_t *p_dsp );
void aout_dspPlaySamples( aout_dsp_t *p_dsp, byte_t *buffer, int i_size );
void aout_dspClose      ( aout_dsp_t *p_dsp );
