/*****************************************************************************
 * aout_directx.c: Windows DirectX audio output method
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: aout_directx.c,v 1.7 2001/07/30 00:53:04 sam Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#define MODULE_NAME directx
#include "modules_inner.h"

/* The most important thing to do for now is to fix the audio bug we've got
 * on startup: the audio will start later than the video (sometimes) and
 * is trying to catching up with it.
 * At first sight it seems to be a scheduling problem
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                   /* write(), close() */
#endif

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#if defined( _MSC_VER )
#   include <dsound.h>
#else
#   include "directx.h"
#endif

#include "audio_output.h"                                   /* aout_thread_t */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "main.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * aout_sys_t: directx audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/

typedef struct aout_sys_s
{
    LPDIRECTSOUND       p_dsobject;              /* main Direct Sound object */

    LPDIRECTSOUNDBUFFER p_dsbuffer_primary;     /* the actual sound card buffer
                                                   (not used directly) */

    LPDIRECTSOUNDBUFFER p_dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    HINSTANCE           hdsound_dll;      /* handle of the opened dsound dll */

    long l_buffer_size;                       /* secondary sound buffer size */
    long l_write_position;             /* next write position for the buffer */

    boolean_t b_active;

} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Probe       ( probedata_t *p_data );
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static long    aout_GetBufInfo  ( aout_thread_t *p_aout, long l_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

/* local functions */
static int DirectxCreateSecondaryBuffer( aout_thread_t *p_aout );
static int DirectxInitDSound( aout_thread_t *p_aout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = aout_Probe;
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Probe: probe the audio device and return a score
 *****************************************************************************
 * This function tries to probe for a Direct Sound  device and returns a
 * score to the plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int aout_Probe( probedata_t *p_data )
{
    /* For now just assume the computer has a sound device */
    if( TestMethod( AOUT_METHOD_VAR, "directx" ) )
    {
        return( 999 );
    }
    return( 400 );
}

/*****************************************************************************
 * aout_Open: open the audio device
 *****************************************************************************
 * This function opens and setups Direct Sound.
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    HRESULT dsresult;
    DSBUFFERDESC dsbuffer_desc;
    WAVEFORMATEX waveformat;

    intf_WarnMsg( 3, "aout: DirectX aout_Open ");

   /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );

    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg( "aout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize some variables */
    p_aout->p_sys->p_dsobject = NULL;
    p_aout->p_sys->p_dsbuffer_primary = NULL;
    p_aout->p_sys->p_dsbuffer = NULL;

    p_aout->psz_device = 0;
    p_aout->i_format   = AOUT_FORMAT_DEFAULT;
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR,
                                                  AOUT_STEREO_DEFAULT );
    p_aout->l_rate     = main_GetIntVariable( AOUT_RATE_VAR,
                                              AOUT_RATE_DEFAULT );

    /* Initialise DirectSound */
    if( DirectxInitDSound( p_aout ) )
    {
        intf_WarnMsg( 3, "aout: can't initialise DirectSound ");
        return( 1 );
    }

    /* Obtain (not create) Direct Sound primary buffer */
    memset( &dsbuffer_desc, 0, sizeof(DSBUFFERDESC) );
    dsbuffer_desc.dwSize = sizeof(DSBUFFERDESC);
    dsbuffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    intf_WarnMsg( 3, "aout: Create direct sound primary buffer ");
    dsresult = IDirectSound_CreateSoundBuffer(p_aout->p_sys->p_dsobject,
                                            &dsbuffer_desc,
                                            &p_aout->p_sys->p_dsbuffer_primary,
                                            NULL);
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: can't create direct sound primary buffer ");
        IDirectSound_Release( p_aout->p_sys->p_dsobject );
        p_aout->p_sys->p_dsobject = NULL;
        p_aout->p_sys->p_dsbuffer_primary = NULL;
        return( 1 );
    }

    /* Set Direct Sound primary buffer format because the default value set by
     * Windows is usually not the high quality value */
    memset(&waveformat, 0, sizeof(WAVEFORMATEX)); 
    waveformat.wFormatTag = WAVE_FORMAT_PCM; 
    waveformat.nChannels = 2; 
    waveformat.nSamplesPerSec = 44100; 
    waveformat.wBitsPerSample = 16; 
    waveformat.nBlockAlign = waveformat.wBitsPerSample / 8 *
                                 waveformat.nChannels;
    waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec *
                                     waveformat.nBlockAlign;

    dsresult = IDirectSoundBuffer_SetFormat(p_aout->p_sys->p_dsbuffer_primary,
                                            &waveformat);
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: can't set primary buffer format");
    }

#if 0
    /* ensure the primary buffer is playing. We won't actually hear anything
     * until the secondary buffer is playing */
    dsresult = IDirectSoundBuffer_Play( p_aout->p_sys->p_dsbuffer_primary,
                                        0,
                                        0,
                                        DSBPLAY_LOOPING);
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: can't play direct sound primary buffer ");
        IDirectSound_Release( p_aout->p_sys->p_dsbuffer_primary );
        IDirectSound_Release( p_aout->p_sys->p_dsobject );
        p_aout->p_sys->p_dsobject = NULL;
        p_aout->p_sys->p_dsbuffer_primary = NULL;
        return( 1 );
    }
#endif

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: reset the audio device and sets its format
 *****************************************************************************
 * This functions set a new audio format.
 * For this we need to close the current secondary buffer and create another
 * one with the desired format.
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    HRESULT dsresult;

    intf_WarnMsg( 3, "aout: DirectX aout_SetFormat ");

    /* first release the current secondary buffer */
    if( p_aout->p_sys->p_dsbuffer != NULL )
    {
        IDirectSoundBuffer_Release( p_aout->p_sys->p_dsbuffer );
        p_aout->p_sys->p_dsbuffer = NULL;
    }

    /* then create a new secondary buffer */
    dsresult = DirectxCreateSecondaryBuffer( p_aout );    
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: DirectX aout_SetFormat cannot create buffer");
        return( 1 );
    }

    p_aout->i_latency = 0;
  
    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************
 * returns the number of bytes in the audio buffer compared to the size of
 * l_buffer_limit...
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    long l_play_position, l_notused, l_result;
    HRESULT dsresult;

    dsresult = IDirectSoundBuffer_GetCurrentPosition(p_aout->p_sys->p_dsbuffer,
                                                 &l_play_position, &l_notused);
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_GetCurrentPosition(
                                                 p_aout->p_sys->p_dsbuffer,
                                                 &l_play_position, &l_notused
                                                        );
    }
    if( dsresult != DS_OK )
    {
        intf_WarnMsg(3,"aout: DirectX aout_GetBufInfo cannot get current pos");
        return( l_buffer_limit );
    }

#if 0
    /* temporary hack. When you start playing a new file, the play position
     * doesn't start changing immediatly, even though sound is already
     * playing from the sound card */
    if( l_play_position == 0 )
    { 
       intf_WarnMsg( 5, "aout: DirectX aout_GetBufInfo: %li", l_buffer_limit);
       return( l_buffer_limit );
    }
#endif

    l_result = (p_aout->p_sys->l_write_position >= l_play_position) ?
      (p_aout->p_sys->l_write_position - l_play_position) /2
               : (p_aout->p_sys->l_buffer_size - l_play_position
                  + p_aout->p_sys->l_write_position) /2 ;

#if 0
    intf_WarnMsg( 5, "aout: DirectX aout_GetBufInfo: %li", l_result);
#endif
    return l_result;
}

/*****************************************************************************
 * aout_Play: play a sound buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    VOID            *p_write_position, *p_start_buffer;
    long            l_bytes1, l_bytes2;
    long            l_play_position, l_notused, l_buffer_free_length;
    HRESULT         dsresult;

    /* We want to copy data to the circular sound buffer, so we first need to
     * find out were in the buffer we can write our data */
    dsresult = IDirectSoundBuffer_GetCurrentPosition(p_aout->p_sys->p_dsbuffer,
                                                     &l_play_position,
                                                     &l_notused);
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_GetCurrentPosition(
                                                 p_aout->p_sys->p_dsbuffer,
                                                 &l_play_position, &l_notused
                                                        );
    }
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: DirectX aout_Play can'get buffer position");
    }

#if 1
    /* check that we are not overflowing the circular buffer (everything should
     * be alright but just in case) */
    l_buffer_free_length =  l_play_position - p_aout->p_sys->l_write_position;
    if( l_buffer_free_length <= 0 )
        l_buffer_free_length += p_aout->p_sys->l_buffer_size;

    if( i_size > l_buffer_free_length )
    {
        intf_WarnMsg( 3, "aout: DirectX aout_Play buffer overflow: size %i, free %i !!!", i_size, l_buffer_free_length);
        intf_WarnMsg( 3, "aout: DirectX aout_Play buffer overflow: writepos %i, readpos %i !!!", p_aout->p_sys->l_write_position, l_play_position);
        /*i_size = l_buffer_free_length;*/

        /* Update the write pointer */
        p_aout->p_sys->l_write_position = l_notused;

    }
    else
    {
#if 0
        intf_WarnMsg( 4, "aout: DirectX aout_Play buffer: size %i, free %i !!!"
                      , i_size, l_buffer_free_length);
        intf_WarnMsg( 4, "aout: DirectX aout_Play buffer: writepos %i, readpos %i !!!", p_aout->p_sys->l_write_position, l_play_position);
#endif
    }
#endif

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock( p_aout->p_sys->p_dsbuffer,
                   p_aout->p_sys->l_write_position,  /* Offset of lock start */
                   i_size,                        /* Number of bytes to lock */
                   &p_write_position,               /* Address of lock start */
                   &l_bytes1,    /* Count of bytes locked before wrap around */
                   &p_start_buffer,        /* Buffer adress (if wrap around) */
                   &l_bytes2,            /* Count of bytes after wrap around */
                   0);                                              /* Flags */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Lock( p_aout->p_sys->p_dsbuffer,
                                            p_aout->p_sys->l_write_position,
                                            i_size,
                                            &p_write_position,
                                            &l_bytes1,
                                            &p_start_buffer,
                                            &l_bytes2,
                                            0);

    }
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: DirectX aout_Play can't lock buffer");
        return;
    }

    /* Now do the actual memcopy (two memcpy because the buffer is circular) */
    memcpy( p_write_position, buffer, l_bytes1 );
    if( p_start_buffer != NULL )
    {
        memcpy( p_start_buffer, buffer + l_bytes1, l_bytes2 );
    }

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock( p_aout->p_sys->p_dsbuffer, 
            p_write_position, l_bytes1, p_start_buffer, l_bytes2 );

    /* Update the write position index of the buffer*/
    p_aout->p_sys->l_write_position += i_size;
    p_aout->p_sys->l_write_position %= p_aout->p_sys->l_buffer_size;

    /* The play function has no effect if the buffer is already playing */
    dsresult = IDirectSoundBuffer_Play( p_aout->p_sys->p_dsbuffer,
                                        0,                         /* Unused */
                                        0,                         /* Unused */
                                        DSBPLAY_LOOPING );          /* Flags */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Play( p_aout->p_sys->p_dsbuffer,
                                            0,                     /* Unused */
                                            0,                     /* Unused */
                                            DSBPLAY_LOOPING );      /* Flags */
    }
    if( dsresult != DS_OK )
    {
        intf_WarnMsg( 3, "aout: DirectX aout_Play can't play buffer");
        return;
    }

}

/*****************************************************************************
 * aout_Close: close the audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{

    intf_WarnMsg( 3, "aout: DirectX aout_Close ");

    /* make sure the buffer isn't playing */
    if( p_aout->p_sys->p_dsbuffer != NULL )
    {
        IDirectSoundBuffer_Stop( p_aout->p_sys->p_dsbuffer );
    }

    /* first release the secondary buffer */
    if( p_aout->p_sys->p_dsbuffer != NULL )
    {
        IDirectSoundBuffer_Release( p_aout->p_sys->p_dsbuffer );
        p_aout->p_sys->p_dsbuffer = NULL;
    }  

    /* then release the primary buffer */
    if( p_aout->p_sys->p_dsbuffer_primary != NULL )
    {
        IDirectSoundBuffer_Release( p_aout->p_sys->p_dsbuffer_primary );
        p_aout->p_sys->p_dsbuffer_primary = NULL;
    }  

    /* finally release the DirectSound object */
    if( p_aout->p_sys->p_dsobject != NULL )
    {
        IDirectSound_Release( p_aout->p_sys->p_dsobject );
        p_aout->p_sys->p_dsobject = NULL;
    }  
    
    /* free DSOUND.DLL */
    if( p_aout->p_sys->hdsound_dll != NULL )
    {
       FreeLibrary( p_aout->p_sys->hdsound_dll );
       p_aout->p_sys->hdsound_dll = NULL;
    }

    /* Close the Output. */
    if ( p_aout->p_sys != NULL )
    { 
        free( p_aout->p_sys );
        p_aout->p_sys = NULL;
    }
}

/*****************************************************************************
 * DirectxInitDSound
 *****************************************************************************
 *****************************************************************************/
static int DirectxInitDSound( aout_thread_t *p_aout )
{
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

    p_aout->p_sys->hdsound_dll = LoadLibrary("DSOUND.DLL");
    if( p_aout->p_sys->hdsound_dll == NULL )
    {
      intf_WarnMsg( 3, "aout: can't open DSOUND.DLL ");
      return( 1 );
    }

    OurDirectSoundCreate = (void *)GetProcAddress( p_aout->p_sys->hdsound_dll,
                                                   "DirectSoundCreate" );

    if( OurDirectSoundCreate == NULL )
    {
      intf_WarnMsg( 3, "aout: GetProcAddress FAILED ");
      FreeLibrary( p_aout->p_sys->hdsound_dll );
      p_aout->p_sys->hdsound_dll = NULL;
      return( 1 );
    }

    /* Create the direct sound object */
    if( OurDirectSoundCreate(NULL, &p_aout->p_sys->p_dsobject, NULL) != DS_OK )
    {
        intf_WarnMsg( 3, "aout: can't create a direct sound device ");
        p_aout->p_sys->p_dsobject = NULL;
        FreeLibrary( p_aout->p_sys->hdsound_dll );
        p_aout->p_sys->hdsound_dll = NULL;
        return( 1 );
    }

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
    if( IDirectSound_SetCooperativeLevel(p_aout->p_sys->p_dsobject,
                                         GetDesktopWindow(),
                                         DSSCL_EXCLUSIVE) )
    {
        intf_WarnMsg( 3, "aout: can't set direct sound cooperative level ");
    }

    return( 0 );
}

/*****************************************************************************
 * DirectxCreateSecondaryBuffer
 *****************************************************************************
 * This function creates the buffer we'll use to play audio.
 * In DirectSound there are two kinds of buffers:
 * - the primary buffer: which is the actual buffer that the soundcard plays
 * - the secondary buffer(s): these buffers are the one actually used by
 *    applications and DirectSound takes care of mixing them into the primary.
 *
 * Once you create a secondary buffer, you cannot change its format anymore so
 * you have to release the current and create another one.
 *****************************************************************************/
static int DirectxCreateSecondaryBuffer( aout_thread_t *p_aout )
{
    WAVEFORMATEX waveformat;
    DSBUFFERDESC dsbdesc;
    DSBCAPS      dsbcaps;

    /* First set the buffer format */
    memset(&waveformat, 0, sizeof(WAVEFORMATEX)); 
    waveformat.wFormatTag = WAVE_FORMAT_PCM; 
    waveformat.nChannels = p_aout->i_channels; 
    waveformat.nSamplesPerSec = p_aout->l_rate; 
    waveformat.wBitsPerSample = 16; 
    waveformat.nBlockAlign = waveformat.wBitsPerSample / 8 *
                                 waveformat.nChannels;
    waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec *
                                     waveformat.nBlockAlign;

    /* Then fill in the descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); 
    dsbdesc.dwSize = sizeof(DSBUFFERDESC); 
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_GLOBALFOCUS;      /* Allows background playing */
    dsbdesc.dwBufferBytes = waveformat.nAvgBytesPerSec * 2;  /* 2 sec buffer */
    dsbdesc.lpwfxFormat = &waveformat; 
 
    if( IDirectSound_CreateSoundBuffer( p_aout->p_sys->p_dsobject,
                                        &dsbdesc,
                                        &p_aout->p_sys->p_dsbuffer,
                                        NULL) != DS_OK )
    {
        intf_WarnMsg( 3, "aout: can't create direct sound secondary buffer ");
        p_aout->p_sys->p_dsbuffer = NULL;
        return( 1 );
    }

    /* backup the size of the secondary sound buffer */
    memset(&dsbcaps, 0, sizeof(DSBCAPS)); 
    dsbcaps.dwSize = sizeof(DSBCAPS);
    IDirectSoundBuffer_GetCaps( p_aout->p_sys->p_dsbuffer, &dsbcaps  );
    p_aout->p_sys->l_buffer_size = dsbcaps.dwBufferBytes;
    p_aout->p_sys->l_write_position = 0;
    intf_WarnMsg( 3, "aout: DirectX DirectxCreateSecondaryBuffer: %li",
                  p_aout->p_sys->l_buffer_size);

    return( 0 );
}
