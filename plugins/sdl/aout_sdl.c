/*****************************************************************************
 * aout_sdl.c : audio sdl functions library
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Pierre Baillet <oct@zoy.org>
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */


#include "SDL/SDL.h"                                         /* SDL base include */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "main.h"

#include "modules.h"


/*****************************************************************************
 * aout_sys_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/

/* the overflow limit is used to prevent the fifo from growing too big */
#define OVERFLOWLIMIT 100000

typedef struct aout_sys_s
{
    byte_t  * audio_buf;
    int i_audio_end;
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
static void    SDL_aout_callback(void *userdata, Uint8 *stream, int len);

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void aout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = aout_Probe;
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Probe: probes the audio device and return a score
 *****************************************************************************
 * This function tries to open the dps and returns a score to the plugin
 * manager so that it can select the best plugin.
 *****************************************************************************/
static int aout_Probe( probedata_t *p_data )
{
    SDL_AudioSpec *desired, *obtained;

    /* Start AudioSDL */
    if( SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        intf_DbgMsg( "aout_Probe: SDL init error: %s", SDL_GetError() );
        return( 0 );
    }
     
    /* asks for a minimum audio spec so that we are sure the dsp exists */
    desired = (SDL_AudioSpec *)malloc( sizeof(SDL_AudioSpec) );
    obtained = (SDL_AudioSpec *)malloc( sizeof(SDL_AudioSpec) );

    desired->freq       = 11025;                                /* frequency */
    desired->format     = AUDIO_U8;                       /* unsigned 8 bits */
    desired->channels   = 2;                                         /* mono */
    desired->callback   = SDL_aout_callback;     /* no callback function yet */
    desired->userdata   = NULL;                    /* null parm for callback */
    desired->samples    = 4096;
    
    
    /* If we were unable to open the device, there is no way we can use
     * the plugin. Return a score of 0. */
    if(SDL_OpenAudio( desired, obtained ) < 0)
    {
        free( desired );
        free( obtained );
        intf_DbgMsg( "aout_Probe: aout sdl error : %s", SDL_GetError() );
         return( 0 );
    }
    free( desired );
    free( obtained );

    /* Otherwise, there are good chances we can use this plugin, return 100. */
    SDL_CloseAudio();

    if( TestMethod( AOUT_METHOD_VAR, "sdl" ) )
    {
        return( 999 );
    }

    return( 50 );
}

/*****************************************************************************
 * aout_Open: opens the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->i_fd with the file's descriptor.
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    SDL_AudioSpec *desired;
    int i_channels = p_aout->b_stereo?2:1;

    /* asks for a minimum audio spec so that we are sure the dsp exists */
    desired = (SDL_AudioSpec *)malloc( sizeof(SDL_AudioSpec) );

       /* Allocate structure */
    
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("aout_Open error: %s", strerror(ENOMEM) );
        return( 1 );
    }
    
   
    p_aout->p_sys->i_audio_end = 0;
    p_aout->p_sys->audio_buf = malloc( OVERFLOWLIMIT );
    
    /* Initialize some variables */
    p_aout->psz_device = 0;
    p_aout->i_format   = AOUT_FORMAT_DEFAULT;
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR,
                                                  AOUT_STEREO_DEFAULT );
    p_aout->l_rate     = main_GetIntVariable( AOUT_RATE_VAR,
                                              AOUT_RATE_DEFAULT );
    
    
    desired->freq =     p_aout->l_rate;

    /* TODO: write conversion beetween AOUT_FORMAT_DEFAULT 
     * AND AUDIO* from SDL. */
    desired->format     = AUDIO_S16LSB;                    /* stereo 16 bits */
    desired->channels    = i_channels;
    desired->callback   = SDL_aout_callback;
    desired->userdata   = p_aout->p_sys; 
    desired->samples    = 2048;
     
    /* Open the sound device 
     * we just ask the SDL to wrap at the good frequency if the one we
     * ask for is unavailable. This is done by setting the second parar
     * to NULL
     */
    if( SDL_OpenAudio(desired,NULL) < 0 )
    {
        free( desired );
        intf_ErrMsg( "aout_Open error: can't open audio device: %s", 
                        SDL_GetError() );
        return( -1 );
    }
    p_aout->p_sys->b_active = 1;
    free( desired );
    SDL_PauseAudio(0);
    return( 0 );

}

/*****************************************************************************
 * aout_SetFormat: resets the dsp and sets its format
 *****************************************************************************
 * This functions resets the DSP device, tries to initialize the output
 * format with the value contained in the dsp structure, and if this value
 * could not be set, the default value returned by ioctl is set. It then
 * does the same for the stereo mode, and for the output rate.
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    /* TODO: finish and clean this */
    SDL_AudioSpec *desired;
    int i_stereo = p_aout->b_stereo?2:1;
    desired = (SDL_AudioSpec *)malloc( sizeof(SDL_AudioSpec) );

/*    i_format = p_aout->i_format;
*/
    desired->freq =     p_aout->l_rate;               /* Set the output rate */
    desired->format     = AUDIO_S16LSB;                    /* stereo 16 bits */
    desired->channels    = i_stereo;
    desired->callback   = SDL_aout_callback;
    desired->userdata   = p_aout->p_sys; 
    desired->samples    = 2048;
     
    /* Open the sound device */
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    if( SDL_OpenAudio(desired,NULL) < 0 )
    {
        free( desired );
        p_aout->p_sys->b_active = 0;
        return( -1 );
    }
    p_aout->p_sys->b_active = 1;
    free( desired );
    SDL_PauseAudio(0);
    return(0);    
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************
 * returns the number of bytes in the audio buffer compared to the size of 
 * l_buffer_limit...
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    if(l_buffer_limit > p_aout->p_sys->i_audio_end)
    {
        /* returning 0 here juste gives awful sound in the speakers :/ */
        return( l_buffer_limit );
    }
    return( p_aout->p_sys->i_audio_end-l_buffer_limit);
}


static void SDL_aout_callback(void *userdata, byte_t *stream, int len)
{
    struct aout_sys_s * p_sys = userdata;
    int end = p_sys->i_audio_end;
    
    if(end > OVERFLOWLIMIT)
    {
        intf_ErrMsg("aout SDL_aout_callback: Overflow.");
        free(p_sys->audio_buf);
        p_sys->audio_buf = NULL;
        p_sys->i_audio_end = 0;
        end = 0;
        // we've gone to slow, increase output freq
    }
       
    /* if we are not in underrun */
    if(end>len)                     
    {
        memcpy(stream, p_sys->audio_buf, len);
        memmove(p_sys->audio_buf, &(p_sys->audio_buf[len]), end-len);
        p_sys->i_audio_end -= len;
    }
}

/*****************************************************************************
 * aout_Play: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    byte_t * audio_buf = p_aout->p_sys->audio_buf;
    
    SDL_LockAudio();                                     /* Stop callbacking */

    audio_buf = realloc(audio_buf, p_aout->p_sys->i_audio_end + i_size);
    memcpy(&(audio_buf[p_aout->p_sys->i_audio_end]), buffer, i_size);
    p_aout->p_sys->i_audio_end += i_size; 
    p_aout->p_sys->audio_buf = audio_buf;
    
    SDL_UnlockAudio();                                  /* go on callbacking */
}

/*****************************************************************************
 * aout_Close: closes the dsp audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    if( p_aout->p_sys->b_active )
    {
        SDL_PauseAudio(1);                                    /* pause audio */
    
        if(p_aout->p_sys->audio_buf != NULL)    /* do we have a buffer now ? */
        {
            free(p_aout->p_sys->audio_buf);
        }
        p_aout->p_sys->b_active = 0;                         /* just for sam */
    }
    free(p_aout->p_sys);                                /* Close the Output. */
    SDL_CloseAudio();
}

