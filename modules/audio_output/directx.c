/*****************************************************************************
 * directx.c: Windows DirectX audio output method
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: directx.c,v 1.9 2002/11/26 22:20:18 gbazin Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */

#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#define FRAME_SIZE 2048              /* The size is in samples, not in bytes */
#define FRAMES_NUM 4

/* frame buffer status */
#define FRAME_QUEUED 0
#define FRAME_EMPTY 1

/*****************************************************************************
 * DirectSound GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
#include <initguid.h>
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

/*****************************************************************************
 * Useful macros
 *****************************************************************************/
#ifndef WAVE_FORMAT_IEEE_FLOAT
#   define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

/*****************************************************************************
 * notification_thread_t: DirectX event thread
 *****************************************************************************/
typedef struct notification_thread_t
{
    VLC_COMMON_MEMBERS

    aout_instance_t * p_aout;
    int i_frame_status[FRAMES_NUM];           /* status of each frame buffer */
    DSBPOSITIONNOTIFY p_events[FRAMES_NUM];      /* play notification events */
    int i_frame_size;                         /* Size in bytes of one frame */

    mtime_t start_date;

} notification_thread_t;

/*****************************************************************************
 * aout_sys_t: directx audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    LPDIRECTSOUND       p_dsobject;              /* main Direct Sound object */

    LPDIRECTSOUNDBUFFER p_dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    LPDIRECTSOUNDNOTIFY p_dsnotify;         /* the position notify interface */

    HINSTANCE           hdsound_dll;      /* handle of the opened dsound dll */

    notification_thread_t * p_notif;                 /* DirectSoundThread id */

    int b_playing;                                         /* playing status */
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenAudio  ( vlc_object_t * );
static void CloseAudio ( vlc_object_t * );

static void Play       ( aout_instance_t * );

/* local functions */
static int  DirectxCreateSecondaryBuffer ( aout_instance_t * );
static void DirectxDestroySecondaryBuffer( aout_instance_t * );
static int  DirectxInitDSound            ( aout_instance_t * );
static void DirectSoundThread            ( notification_thread_t * );
static int  DirectxFillBuffer            ( aout_instance_t *, int,
                                           aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DirectX audio module") );
    set_capability( "audio output", 100 );
    add_shortcut( "directx" );
    set_callbacks( OpenAudio, CloseAudio );
vlc_module_end();

/*****************************************************************************
 * OpenAudio: open the audio device
 *****************************************************************************
 * This function opens and setups Direct Sound.
 *****************************************************************************/
static int OpenAudio( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    int i;

    msg_Dbg( p_aout, "Open" );

   /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Initialize some variables */
    p_aout->output.p_sys->p_dsobject = NULL;
    p_aout->output.p_sys->p_dsbuffer = NULL;
    p_aout->output.p_sys->p_dsnotify = NULL;
    p_aout->output.p_sys->p_notif = NULL;
    p_aout->output.p_sys->b_playing = 0;

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* Initialise DirectSound */
    if( DirectxInitDSound( p_aout ) )
    {
        msg_Err( p_aout, "cannot initialize DirectSound" );
        goto error;
    }

    /* Now we need to setup DirectSound play notification */
    p_aout->output.p_sys->p_notif =
        vlc_object_create( p_aout, sizeof(notification_thread_t) );
    p_aout->output.p_sys->p_notif->p_aout = p_aout;

    /* first we need to create the notification events */
    for( i = 0; i < FRAMES_NUM; i++ )
        p_aout->output.p_sys->p_notif->p_events[i].hEventNotify =
            CreateEvent( NULL, FALSE, FALSE, NULL );

    /* then create a new secondary buffer */
    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
    if( DirectxCreateSecondaryBuffer( p_aout ) )
    {
        msg_Err( p_aout, "cannot create WAVE_FORMAT_IEEE_FLOAT buffer" );

        p_aout->output.output.i_format = VLC_FOURCC('s','1','6','l');
        if( DirectxCreateSecondaryBuffer( p_aout ) )
        {
            msg_Err( p_aout, "cannot create WAVE_FORMAT_PCM buffer" );
            return 1;
        }
    }

    /* then launch the notification thread */
    msg_Dbg( p_aout, "creating DirectSoundThread" );
    if( vlc_thread_create( p_aout->output.p_sys->p_notif,
                           "DirectSound Notification Thread",
                           DirectSoundThread,
                           THREAD_PRIORITY_TIME_CRITICAL, 1 ) )
    {
        msg_Err( p_aout, "cannot create DirectSoundThread" );
        goto error;
    }

    vlc_object_attach( p_aout->output.p_sys->p_notif, p_aout );

    return 0;

 error:
    CloseAudio( VLC_OBJECT(p_aout) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Play: we'll start playing the directsound buffer here because at least here
 *       we know the first buffer has been put in the aout fifo and we also
 *       know its date.
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
    if( !p_aout->output.p_sys->b_playing )
    {
        aout_buffer_t *p_buffer;

        p_aout->output.p_sys->b_playing = 1;

        /* get the playing date of the first aout buffer */
        p_aout->output.p_sys->p_notif->start_date =
            aout_FifoFirstDate( p_aout, &p_aout->output.fifo );

        /* fill in the first samples */
        p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
        DirectxFillBuffer( p_aout, 0, p_buffer );

        /* wake up the audio output thread */
        SetEvent( p_aout->output.p_sys->p_notif->p_events[0].hEventNotify );
    }
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
static void CloseAudio( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    msg_Dbg( p_aout, "Close" );

    /* kill the position notification thread, if any */
    if( p_aout->output.p_sys->p_notif )
    {
        vlc_object_detach( p_aout->output.p_sys->p_notif );
        if( p_aout->output.p_sys->p_notif->b_thread )
        {
            p_aout->output.p_sys->p_notif->b_die = 1;

            if( !p_aout->output.p_sys->b_playing )
                /* wake up the audio thread */
                SetEvent(
                    p_aout->output.p_sys->p_notif->p_events[0].hEventNotify );

            vlc_thread_join( p_aout->output.p_sys->p_notif );
        }
        vlc_object_destroy( p_aout->output.p_sys->p_notif );
    }

    /* release the secondary buffer */
    DirectxDestroySecondaryBuffer( p_aout );

    /* finally release the DirectSound object */
    if( p_aout->output.p_sys->p_dsobject )
        IDirectSound_Release( p_aout->output.p_sys->p_dsobject );
    
    /* free DSOUND.DLL */
    if( p_aout->output.p_sys->hdsound_dll )
       FreeLibrary( p_aout->output.p_sys->hdsound_dll );

    free( p_aout->output.p_sys );
}

/*****************************************************************************
 * DirectxInitDSound: handle all the gory details of DirectSound initialisation
 *****************************************************************************/
static int DirectxInitDSound( aout_instance_t *p_aout )
{
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

    p_aout->output.p_sys->hdsound_dll = LoadLibrary("DSOUND.DLL");
    if( p_aout->output.p_sys->hdsound_dll == NULL )
    {
        msg_Warn( p_aout, "cannot open DSOUND.DLL" );
        goto error;
    }

    OurDirectSoundCreate = (void *)GetProcAddress(
                                           p_aout->output.p_sys->hdsound_dll,
                                           "DirectSoundCreate" );
    if( OurDirectSoundCreate == NULL )
    {
        msg_Warn( p_aout, "GetProcAddress FAILED" );
        goto error;
    }

    /* Create the direct sound object */
    if( OurDirectSoundCreate( NULL, &p_aout->output.p_sys->p_dsobject, NULL )
        != DS_OK )
    {
        msg_Warn( p_aout, "cannot create a direct sound device" );
        goto error;
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
    if( IDirectSound_SetCooperativeLevel( p_aout->output.p_sys->p_dsobject,
                                          GetDesktopWindow(),
                                          DSSCL_EXCLUSIVE) )
    {
        msg_Warn( p_aout, "cannot set direct sound cooperative level" );
    }

    return 0;

 error:
    p_aout->output.p_sys->p_dsobject = NULL;
    if( p_aout->output.p_sys->hdsound_dll )
    {
        FreeLibrary( p_aout->output.p_sys->hdsound_dll );
        p_aout->output.p_sys->hdsound_dll = NULL;
    }
    return 1;

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
static int DirectxCreateSecondaryBuffer( aout_instance_t *p_aout )
{
    WAVEFORMATEX         waveformat;
    DSBUFFERDESC         dsbdesc;
    DSBCAPS              dsbcaps;
    int                  i_nb_channels, i;

    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels >= 2 )
    {
        i_nb_channels = 2;
        p_aout->output.output.i_physical_channels =
            AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    else
    {
        i_nb_channels = 1;
        p_aout->output.output.i_physical_channels =
            AOUT_CHAN_CENTER;
    }

    /* First set the buffer format */
    memset( &waveformat, 0, sizeof(WAVEFORMATEX) );
    switch( p_aout->output.output.i_format )
    {
    case VLC_FOURCC('s','1','6','l'):
        waveformat.wFormatTag     = WAVE_FORMAT_PCM;
        waveformat.wBitsPerSample = 16;
        break;
    case VLC_FOURCC('f','l','3','2'):
        waveformat.wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.wBitsPerSample = sizeof(float) * 8;
        break;
    }
    waveformat.nChannels       = i_nb_channels;
    waveformat.nSamplesPerSec  = p_aout->output.output.i_rate;
    waveformat.nBlockAlign     = waveformat.wBitsPerSample / 8 *
                                 waveformat.nChannels;
    waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec *
                                     waveformat.nBlockAlign;

    aout_FormatPrepare( &p_aout->output.output );

    /* Then fill in the descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_CTRLPOSITIONNOTIFY     /* We need notification */
                    | DSBCAPS_GLOBALFOCUS;      /* Allows background playing */
    dsbdesc.dwBufferBytes = FRAME_SIZE * FRAMES_NUM           /* buffer size */
                            * p_aout->output.output.i_bytes_per_frame;
    dsbdesc.lpwfxFormat = &waveformat;
 
    if( IDirectSound_CreateSoundBuffer( p_aout->output.p_sys->p_dsobject,
                                        &dsbdesc,
                                        &p_aout->output.p_sys->p_dsbuffer,
                                        NULL) != DS_OK )
    {
        msg_Warn( p_aout, "cannot create direct sound secondary buffer" );
        goto error;
    }

    /* backup the size of a frame */
    p_aout->output.p_sys->p_notif->i_frame_size = FRAME_SIZE *
        p_aout->output.output.i_bytes_per_frame;

    memset(&dsbcaps, 0, sizeof(DSBCAPS));
    dsbcaps.dwSize = sizeof(DSBCAPS);
    IDirectSoundBuffer_GetCaps( p_aout->output.p_sys->p_dsbuffer, &dsbcaps  );
    msg_Dbg( p_aout, "requested %li bytes buffer and got %li bytes.",
             FRAMES_NUM * p_aout->output.p_sys->p_notif->i_frame_size,
             dsbcaps.dwBufferBytes );

    /* Now the secondary buffer is created, we need to setup its position
     * notification */
    for( i = 0; i < FRAMES_NUM; i++ )
    {
        p_aout->output.p_sys->p_notif->p_events[i].dwOffset = i *
            p_aout->output.p_sys->p_notif->i_frame_size;

        p_aout->output.p_sys->p_notif->i_frame_status[i] = FRAME_EMPTY;
    }

    /* Get the IDirectSoundNotify interface */
    if FAILED( IDirectSoundBuffer_QueryInterface(
                                p_aout->output.p_sys->p_dsbuffer,
                                &IID_IDirectSoundNotify,
                                (LPVOID *)&p_aout->output.p_sys->p_dsnotify ) )
    {
        msg_Err( p_aout, "cannot get Notify interface" );
        goto error;
    }
        
    if FAILED( IDirectSoundNotify_SetNotificationPositions(
                                    p_aout->output.p_sys->p_dsnotify,
                                    FRAMES_NUM,
                                    p_aout->output.p_sys->p_notif->p_events ) )
    {
        msg_Err( p_aout, "cannot set position Notification" );
        goto error;
    }

    p_aout->output.i_nb_samples = FRAME_SIZE;

    return 0;

 error:
    if( p_aout->output.p_sys->p_dsbuffer )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsbuffer = NULL;
    }
    if( p_aout->output.p_sys->p_dsnotify )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsnotify = NULL;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DirectxCreateSecondaryBuffer
 *****************************************************************************
 * This function destroys the secondary buffer.
 *****************************************************************************/
static void DirectxDestroySecondaryBuffer( aout_instance_t *p_aout )
{
    /* make sure the buffer isn't playing */
    if( p_aout->output.p_sys->p_dsbuffer )
        IDirectSoundBuffer_Stop( p_aout->output.p_sys->p_dsbuffer );

    if( p_aout->output.p_sys->p_dsnotify )
    {
        IDirectSoundNotify_Release( p_aout->output.p_sys->p_dsnotify );
        p_aout->output.p_sys->p_dsnotify = NULL;
    }

    if( p_aout->output.p_sys->p_dsbuffer )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsbuffer = NULL;
    }
}

/*****************************************************************************
 * DirectxFillBuffer: Fill in one of the direct sound frame buffers.
 *****************************************************************************
 * Returns VLC_SUCCESS on success.
 *****************************************************************************/
static int DirectxFillBuffer( aout_instance_t *p_aout, int i_frame,
                              aout_buffer_t *p_buffer )
{
    notification_thread_t *p_notif = p_aout->output.p_sys->p_notif;
    void *p_write_position, *p_wrap_around;
    long l_bytes1, l_bytes2;
    HRESULT dsresult;

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock(
                p_aout->output.p_sys->p_dsbuffer,               /* DS buffer */
                i_frame * p_notif->i_frame_size,             /* Start offset */
                p_notif->i_frame_size,                    /* Number of bytes */
                &p_write_position,                  /* Address of lock start */
                &l_bytes1,       /* Count of bytes locked before wrap around */
                &p_wrap_around,            /* Buffer adress (if wrap around) */
                &l_bytes2,               /* Count of bytes after wrap around */
                0 );                                                /* Flags */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->output.p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Lock(
                               p_aout->output.p_sys->p_dsbuffer,
                               i_frame * p_notif->i_frame_size,
                               p_notif->i_frame_size,
                               &p_write_position,
                               &l_bytes1,
                               &p_wrap_around,
                               &l_bytes2,
                               0 );
    }
    if( dsresult != DS_OK )
    {
        msg_Warn( p_notif, "cannot lock buffer" );
        if( p_buffer ) aout_BufferFree( p_buffer );
        return VLC_EGENERIC;
    }

    if ( p_buffer != NULL )
    {
        p_aout->p_vlc->pf_memcpy( p_write_position, p_buffer->p_buffer,
                                  l_bytes1 );
        aout_BufferFree( p_buffer );
    }
    else
        memset( p_write_position, 0, l_bytes1 );

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock( p_aout->output.p_sys->p_dsbuffer,
                               p_write_position, l_bytes1,
                               p_wrap_around, l_bytes2 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectSoundThread: this thread will capture play notification events. 
 *****************************************************************************
 * We use this thread to emulate a callback mechanism. The thread probes for
 * event notification and fills up the DS secondary buffer when needed.
 *****************************************************************************/
static void DirectSoundThread( notification_thread_t *p_notif )
{
    HANDLE  notification_events[FRAMES_NUM];
    HRESULT dsresult;
    aout_instance_t *p_aout = p_notif->p_aout;
    int i, i_which_frame, i_last_frame, i_next_frame;
    mtime_t mtime;

    for( i = 0; i < FRAMES_NUM; i++ )
        notification_events[i] = p_notif->p_events[i].hEventNotify;

    /* Tell the main thread that we are ready */
    vlc_thread_ready( p_notif );

    msg_Dbg( p_notif, "DirectSoundThread ready" );

    /* Wait here until Play() is called */
    WaitForSingleObject( notification_events[0], INFINITE );

    if( !p_notif->b_die )
    {
        mwait( p_notif->start_date - AOUT_PTS_TOLERANCE / 2 );

        /* start playing the buffer */
        dsresult = IDirectSoundBuffer_Play( p_aout->output.p_sys->p_dsbuffer,
                                        0,                         /* Unused */
                                        0,                         /* Unused */
                                        DSBPLAY_LOOPING );          /* Flags */
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_aout->output.p_sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Play(
                                            p_aout->output.p_sys->p_dsbuffer,
                                            0,                     /* Unused */
                                            0,                     /* Unused */
                                            DSBPLAY_LOOPING );      /* Flags */
        }
        if( dsresult != DS_OK )
        {
            msg_Err( p_aout, "cannot start playing buffer" );
        }
    }

    while( !p_notif->b_die )
    {
        aout_buffer_t *p_buffer;
        long l_latency;

        /* wait for the position notification */
        i_which_frame = WaitForMultipleObjects( FRAMES_NUM,
                                                notification_events, 0,
                                                INFINITE ) - WAIT_OBJECT_0;

        if( p_notif->b_die )
            break;

        mtime = mdate();

        /* We take into account the current latency */
        if( IDirectSoundBuffer_GetCurrentPosition(
                p_aout->output.p_sys->p_dsbuffer,
                &l_latency, NULL ) == DS_OK )
        {
            if( l_latency > (i_which_frame * FRAME_SIZE)
                  && l_latency < ((i_which_frame+1) * FRAME_SIZE) )
            {
                l_latency = - ( l_latency /
                                p_aout->output.output.i_bytes_per_frame %
                                FRAME_SIZE );
            }
            else
            {
                l_latency = FRAME_SIZE - ( l_latency /
                                      p_aout->output.output.i_bytes_per_frame %
                                      FRAME_SIZE );
            }
        }
        else
        {
            l_latency = 0;
        }

        /* Mark last frame as empty */
        i_last_frame = (i_which_frame + FRAMES_NUM -1) % FRAMES_NUM;
        i_next_frame = (i_which_frame + 1) % FRAMES_NUM;
        p_notif->i_frame_status[i_last_frame] = FRAME_EMPTY;

        /* Try to fill in as many frame buffers as possible */
        for( i = i_next_frame; (i % FRAMES_NUM) != i_which_frame; i++ )
        {

            /* Check if frame buf is already filled */
            if( p_notif->i_frame_status[i % FRAMES_NUM] == FRAME_QUEUED )
                continue;

            p_buffer = aout_OutputNextBuffer( p_aout,
                mtime + 1000000 / p_aout->output.output.i_rate *
                ((i - i_next_frame + 1) * FRAME_SIZE + l_latency), VLC_FALSE );

            /* If there is no audio data available and we have some buffered
             * already, then just wait for the next time */
            if( !p_buffer && (i != i_next_frame) )
            {
                //msg_Err( p_aout, "only %i frame buffers filled!",
                //         i - i_next_frame );
                break;
            }

            if( DirectxFillBuffer( p_aout, (i%FRAMES_NUM), p_buffer )
                != VLC_SUCCESS )
                break;

            /* Mark the frame buffer as QUEUED */
            p_notif->i_frame_status[i%FRAMES_NUM] = FRAME_QUEUED;
        }

    }

    /* free the events */
    for( i = 0; i < FRAMES_NUM; i++ )
        CloseHandle( notification_events[i] );

    msg_Dbg( p_notif, "DirectSoundThread exiting" );
}
