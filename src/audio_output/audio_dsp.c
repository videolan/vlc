/******************************************************************************
 * audio_dsp.c : dsp functions library
 * (c)1999 VideoLAN
 ******************************************************************************/

/* TODO:
 *
 * - an aout_dspGetFormats() function
 * - dsp inline/static
 * - make this library portable (see mpg123)
 * - macroify aout_dspPlaySamples &/| aout_dspGetBufInfo ?
 *
 */

/******************************************************************************
 * Preamble
 ******************************************************************************/
#include <pthread.h>
#include <fcntl.h>                                        /* open(), O_WRONLY */
#include <sys/ioctl.h>                                             /* ioctl() */
#include <unistd.h>                                       /* write(), close() */
/* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT, SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED, SNDCTL_DSP_GETOSPACE */
#include <sys/soundcard.h>

#include "common.h"                                      /* boolean_t, byte_t */
#include "mtime.h"

#include "audio_output.h"                                       /* aout_dsp_t */
#include "audio_dsp.h"

#include "intf_msg.h"                         /* intf_DbgMsg(), intf_ErrMsg() */

/******************************************************************************
 * aout_dspOpen: opens the audio device (the digital sound processor)
 ******************************************************************************
 * - This function opens the dsp as an usual non-blocking write-only file, and
 *   modifies the p_dsp->i_fd with the file's descriptor.
 * - p_dsp->psz_device must be set before calling this function !
 ******************************************************************************/
int aout_dspOpen( aout_dsp_t *p_dsp )
{
    if ( (p_dsp->i_fd = open( p_dsp->psz_device, O_WRONLY )) < 0 )
    {
        intf_ErrMsg( "aout error: can't open audio device (%s)\n", p_dsp->psz_device );
	return( -1 );
    }

    return( 0 );
}

/******************************************************************************
 * aout_dspReset: resets the dsp
 ******************************************************************************/
int aout_dspReset( aout_dsp_t *p_dsp )
{
    if ( ioctl( p_dsp->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        intf_ErrMsg( "aout error: can't reset audio device (%s)\n", p_dsp->psz_device );
	return( -1 );
    }

    return( 0 );
}

/******************************************************************************
 * aout_dspSetFormat: sets the dsp output format
 ******************************************************************************
 * This functions tries to initialize the dsp output format with the value
 * contained in the dsp structure, and if this value could not be set, the
 * default value returned by ioctl is set.
 ******************************************************************************/
int aout_dspSetFormat( aout_dsp_t *p_dsp )
{
    int i_format;

    i_format = p_dsp->i_format;
    if ( ioctl( p_dsp->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output format (%i)\n", p_dsp->i_format );
	return( -1 );
    }

    if ( i_format != p_dsp->i_format )
    {
        intf_DbgMsg( "aout debug: audio output format not supported (%i)\n", p_dsp->i_format );
	p_dsp->i_format = i_format;
    }

    return( 0 );
}

/******************************************************************************
 * aout_dspSetChannels: sets the dsp's stereo or mono mode
 ******************************************************************************
 * This function acts just like the previous one...
 ******************************************************************************/
int aout_dspSetChannels( aout_dsp_t *p_dsp )
{
    boolean_t b_stereo;

    b_stereo = p_dsp->b_stereo;
    if ( ioctl( p_dsp->i_fd, SNDCTL_DSP_STEREO, &b_stereo ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set number of audio channels (%i)\n", p_dsp->b_stereo );
	return( -1 );
    }

    if ( b_stereo != p_dsp->b_stereo )
    {
        intf_DbgMsg( "aout debug: number of audio channels not supported (%i)\n", p_dsp->b_stereo );
	p_dsp->b_stereo = b_stereo;
    }

    return( 0 );
}

/******************************************************************************
 * aout_dspSetRate: sets the dsp's audio output rate
 ******************************************************************************
 * This function tries to initialize the dsp with the rate contained in the
 * dsp structure, but if the dsp doesn't support this value, the function uses
 * the value returned by ioctl...
 ******************************************************************************/
int aout_dspSetRate( aout_dsp_t *p_dsp )
{
    long l_rate;

    l_rate = p_dsp->l_rate;
    if ( ioctl( p_dsp->i_fd, SNDCTL_DSP_SPEED, &l_rate ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output rate (%li)\n", p_dsp->l_rate );
	return( -1 );
    }

    if ( l_rate != p_dsp->l_rate )
    {
        intf_DbgMsg( "aout debug: audio output rate not supported (%li)\n", p_dsp->l_rate );
	p_dsp->l_rate = l_rate;
    }

    return( 0 );
}

/******************************************************************************
 * aout_dspGetBufInfo: buffer status query
 ******************************************************************************
 * This function fills in the audio_buf_info structure :
 * - int fragments : number of available fragments (partially usend ones not
 *   counted)
 * - int fragstotal : total number of fragments allocated
 * - int fragsize : size of a fragment in bytes
 * - int bytes : available space in bytes (includes partially used fragments)
 * Note! 'bytes' could be more than fragments*fragsize
 ******************************************************************************/
void aout_dspGetBufInfo( aout_dsp_t *p_dsp )
{
    ioctl( p_dsp->i_fd, SNDCTL_DSP_GETOSPACE, &p_dsp->buf_info );
}

/******************************************************************************
 * aout_dspPlaySamples: plays a sound samples buffer
 ******************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 ******************************************************************************/
void aout_dspPlaySamples( aout_dsp_t *p_dsp, byte_t *buffer, int i_size )
{
    write( p_dsp->i_fd, buffer, i_size );
}

/******************************************************************************
 * aout_dspClose: closes the dsp audio device
 ******************************************************************************/
void aout_dspClose( aout_dsp_t *p_dsp )
{
    close( p_dsp->i_fd );
}
