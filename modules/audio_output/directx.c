/*****************************************************************************
 * directx.c: Windows DirectX audio output method
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: directx.c,v 1.4 2002/10/20 12:23:47 massiot Exp $
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

/*****************************************************************************
 * DirectSound GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
#include <initguid.h>
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

/*****************************************************************************
 * notification_thread_t: DirectX event thread
 *****************************************************************************/
typedef struct notification_thread_t
{
    VLC_COMMON_MEMBERS

    aout_instance_t * p_aout;
    DSBPOSITIONNOTIFY p_events[2];               /* play notification events */
    int i_buffer_size;                         /* Size in bytes of one frame */

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

    LPDIRECTSOUNDBUFFER p_dsbuffer_primary;     /* the actual sound card buffer
                                                   (not used directly) */

    LPDIRECTSOUNDBUFFER p_dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    LPDIRECTSOUNDNOTIFY p_dsnotify;         /* the position notify interface */

    HINSTANCE           hdsound_dll;      /* handle of the opened dsound dll */

    vlc_mutex_t buffer_lock;                            /* audio buffer lock */

    notification_thread_t * p_notif;                 /* DirectSoundThread id */

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
    HRESULT dsresult;
    DSBUFFERDESC dsbuffer_desc;

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
    p_aout->output.p_sys->p_dsbuffer_primary = NULL;
    p_aout->output.p_sys->p_dsbuffer = NULL;
    p_aout->output.p_sys->p_dsnotify = NULL;
    p_aout->output.p_sys->p_notif = NULL;
    vlc_mutex_init( p_aout, &p_aout->output.p_sys->buffer_lock );

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* Initialise DirectSound */
    if( DirectxInitDSound( p_aout ) )
    {
        msg_Err( p_aout, "cannot initialize DirectSound" );
        goto error;
    }

    /* Obtain (not create) Direct Sound primary buffer */
    memset( &dsbuffer_desc, 0, sizeof(DSBUFFERDESC) );
    dsbuffer_desc.dwSize = sizeof(DSBUFFERDESC);
    dsbuffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    msg_Warn( p_aout, "create direct sound primary buffer" );
    dsresult = IDirectSound_CreateSoundBuffer(p_aout->output.p_sys->p_dsobject,
                                     &dsbuffer_desc,
                                     &p_aout->output.p_sys->p_dsbuffer_primary,
                                     NULL);
    if( dsresult != DS_OK )
    {
        msg_Err( p_aout, "cannot create direct sound primary buffer" );
        goto error;
    }

    /* Now we need to setup DirectSound play notification */
    p_aout->output.p_sys->p_notif =
        vlc_object_create( p_aout, sizeof(notification_thread_t) );
    p_aout->output.p_sys->p_notif->p_aout = p_aout;

    /* first we need to create the notification events */
    p_aout->output.p_sys->p_notif->p_events[0].hEventNotify =
        CreateEvent( NULL, FALSE, FALSE, NULL );
    p_aout->output.p_sys->p_notif->p_events[1].hEventNotify =
        CreateEvent( NULL, FALSE, FALSE, NULL );

    vlc_mutex_lock( &p_aout->output.p_sys->buffer_lock );

    /* then create a new secondary buffer */
    if( DirectxCreateSecondaryBuffer( p_aout ) )
    {
        msg_Err( p_aout, "cannot create buffer" );
        vlc_mutex_unlock( &p_aout->output.p_sys->buffer_lock );
        return 1;
    }

    vlc_mutex_unlock( &p_aout->output.p_sys->buffer_lock );

    /* start playing the buffer */
    dsresult = IDirectSoundBuffer_Play( p_aout->output.p_sys->p_dsbuffer,
                                        0,                         /* Unused */
                                        0,                         /* Unused */
                                        DSBPLAY_LOOPING );          /* Flags */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->output.p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Play( p_aout->output.p_sys->p_dsbuffer,
                                            0,                     /* Unused */
                                            0,                     /* Unused */
                                            DSBPLAY_LOOPING );      /* Flags */
    }
    if( dsresult != DS_OK )
    {
        msg_Warn( p_aout, "cannot play buffer" );
    }

    /* then launch the notification thread */
    msg_Dbg( p_aout, "creating DirectSoundThread" );
    if( vlc_thread_create( p_aout->output.p_sys->p_notif,
                           "DirectSound Notification Thread",
                           DirectSoundThread, VLC_THREAD_PRIORITY_OUTPUT, 1 ) )
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
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
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
            vlc_thread_join( p_aout->output.p_sys->p_notif );
        }
        vlc_object_destroy( p_aout->output.p_sys->p_notif );
    }

    /* release the secondary buffer */
    DirectxDestroySecondaryBuffer( p_aout );

    /* then release the primary buffer */
    if( p_aout->output.p_sys->p_dsbuffer_primary )
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer_primary );

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
    int                  i_nb_channels;

    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels > 2 )
    {
        i_nb_channels = 2;
        p_aout->output.output.i_channels = AOUT_CHAN_STEREO;
    }

    /* First set the buffer format */
    memset(&waveformat, 0, sizeof(WAVEFORMATEX));
    waveformat.wFormatTag      = WAVE_FORMAT_PCM;
    waveformat.nChannels       = i_nb_channels;
    waveformat.nSamplesPerSec  = p_aout->output.output.i_rate;
    waveformat.wBitsPerSample  = 16;
    waveformat.nBlockAlign     = waveformat.wBitsPerSample / 8 *
                                 waveformat.nChannels;
    waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec *
                                     waveformat.nBlockAlign;

    /* Then fill in the descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_CTRLPOSITIONNOTIFY     /* We need notification */
                    | DSBCAPS_GLOBALFOCUS;      /* Allows background playing */
    dsbdesc.dwBufferBytes = FRAME_SIZE * 2 /* frames*/ *      /* buffer size */
                            sizeof(s16) * i_nb_channels;
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
    p_aout->output.p_sys->p_notif->i_buffer_size = FRAME_SIZE * sizeof(s16)
                                            * i_nb_channels;

    memset(&dsbcaps, 0, sizeof(DSBCAPS));
    dsbcaps.dwSize = sizeof(DSBCAPS);
    IDirectSoundBuffer_GetCaps( p_aout->output.p_sys->p_dsbuffer, &dsbcaps  );
    msg_Dbg( p_aout, "requested %li bytes buffer and got %li bytes.",
             2 * p_aout->output.p_sys->p_notif->i_buffer_size,
             dsbcaps.dwBufferBytes );

    /* Now the secondary buffer is created, we need to setup its position
     * notification */
    p_aout->output.p_sys->p_notif->p_events[0].dwOffset = 0;
    p_aout->output.p_sys->p_notif->p_events[1].dwOffset =
        p_aout->output.p_sys->p_notif->i_buffer_size;

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
                                    p_aout->output.p_sys->p_dsnotify, 2,
                                    p_aout->output.p_sys->p_notif->p_events ) )
    {
        msg_Err( p_aout, "cannot set position Notification" );
        goto error;
    }
    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
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
 * DirectSoundThread: this thread will capture play notification events. 
 *****************************************************************************
 * We use this thread to emulate a callback mechanism. The thread probes for
 * event notification and fills up the DS secondary buffer when needed.
 *****************************************************************************/
static void DirectSoundThread( notification_thread_t *p_notif )
{
    HANDLE  notification_events[2];
    HRESULT dsresult;
    aout_instance_t *p_aout = p_notif->p_aout;

    notification_events[0] = p_notif->p_events[0].hEventNotify;
    notification_events[1] = p_notif->p_events[1].hEventNotify;

    /* Tell the main thread that we are ready */
    vlc_thread_ready( p_notif );

    msg_Dbg( p_notif, "DirectSoundThread ready" );

    while( !p_notif->b_die )
    {
        int i_which_event;
        void *p_write_position, *p_wrap_around;
        long l_bytes1, l_bytes2;
        aout_buffer_t * p_buffer;

        /* wait for the position notification */
        i_which_event = WaitForMultipleObjects( 2, notification_events, 0,
                                                INFINITE ) - WAIT_OBJECT_0;

        vlc_mutex_lock( &p_aout->output.p_sys->buffer_lock );

        if( p_notif->b_die )
        {
            vlc_mutex_unlock( &p_aout->output.p_sys->buffer_lock );
            break;
        }

        /* Before copying anything, we have to lock the buffer */
        dsresult = IDirectSoundBuffer_Lock(
                                                                /* DS buffer */
            p_aout->output.p_sys->p_dsbuffer,
                                                     /* Offset of lock start */
            i_which_event ? 0 : p_notif->i_buffer_size,
            p_notif->i_buffer_size,                 /* Number of bytes */
            &p_write_position,                      /* Address of lock start */
            &l_bytes1,           /* Count of bytes locked before wrap around */
            &p_wrap_around,                /* Buffer adress (if wrap around) */
            &l_bytes2,                   /* Count of bytes after wrap around */
            0 );                                                    /* Flags */
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_aout->output.p_sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Lock(
                           p_aout->output.p_sys->p_dsbuffer,
                           i_which_event ? 0 : p_notif->i_buffer_size,
                           p_notif->i_buffer_size,
                           &p_write_position,
                           &l_bytes1,
                           &p_wrap_around,
                           &l_bytes2,
                           0 );
        }
        if( dsresult != DS_OK )
        {
            msg_Warn( p_notif, "cannot lock buffer" );
            vlc_mutex_unlock( &p_aout->output.p_sys->buffer_lock );
            continue;
        }

        /* We also take into account the latency instead of just mdate() */
        p_buffer = aout_OutputNextBuffer( p_aout,
            mdate() + 1000000 / p_aout->output.output.i_rate * FRAME_SIZE,
            VLC_FALSE );

        /* Now do the actual memcpy into the circular buffer */
        if ( l_bytes1 != p_notif->i_buffer_size )
            msg_Err( p_aout, "Wrong buffer size: %d, %d", l_bytes1,
                     p_notif->i_buffer_size );

        if ( p_buffer != NULL )
        {
            p_aout->p_vlc->pf_memcpy( p_write_position, p_buffer->p_buffer,
                                      l_bytes1 );
            aout_BufferFree( p_buffer );
        }
        else
        {
            memset( p_write_position, 0, l_bytes1 );
        }

        /* Now the data has been copied, unlock the buffer */
        IDirectSoundBuffer_Unlock( p_aout->output.p_sys->p_dsbuffer,
                        p_write_position, l_bytes1, p_wrap_around, l_bytes2 );

        vlc_mutex_unlock( &p_aout->output.p_sys->buffer_lock );

    }

    /* free the events */
    CloseHandle( notification_events[0] );
    CloseHandle( notification_events[1] );

    msg_Dbg( p_notif, "DirectSoundThread exiting" );

}
