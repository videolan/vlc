/*****************************************************************************
 * aout_arts.c : aRts functions library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Blindauer Emmanuel <manu@agat.net>
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

#define MODULE_NAME arts
#include "modules_inner.h"


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <artsc.h>

#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * aout_sys_t: arts audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some arts specific variables.
 *****************************************************************************/
typedef struct aout_sys_s
{
    arts_stream_t stream;

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
 * aout_Probe: probes the audio device and return a score
 *****************************************************************************
 * This function tries to open the dps and returns a score to the plugin
 * manager so that it can 
 *****************************************************************************/
static int aout_Probe( probedata_t *p_data )
{
    if( TestMethod( AOUT_METHOD_VAR, "arts" ) )
    {
        return( 999 );
    }

    /* We don't have to test anything -- if we managed to open this plugin,
     * it means we have the appropriate libs. */
    return( 50 );
}

/*****************************************************************************
 * aout_Open: initialize arts connection to server
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    int i_err = 0;
    p_aout->i_format = AOUT_FORMAT_DEFAULT;
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR, AOUT_STEREO_DEFAULT );
    p_aout->l_rate = AOUT_RATE_DEFAULT;


    i_err = arts_init();
    
    if (i_err < 0)
    {
        fprintf(stderr, "arts_init error: %s\n", arts_error_text(i_err));
        return(-1);
    }

    p_aout->p_sys->stream =
        arts_play_stream( p_aout->l_rate, 16, p_aout->i_channels, "vlc" );

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: set the output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
   /*Not ready*/ 
/*    p_aout->i_latency = esd_get_latency(i_fd);*/
    p_aout->i_latency = 0;
   
    intf_WarnMsg(2, "aout_arts_latency: %d",p_aout->i_latency);

    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    /* arbitrary value that should be changed */
    return( l_buffer_limit );
}

/*****************************************************************************
 * aout_Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{

    int i_err = arts_write( p_aout->p_sys->stream, buffer, i_size );

    if(i_err < 0)
    {
        fprintf(stderr, "arts_write error: %s\n", arts_error_text(i_err));
    }

}

/*****************************************************************************
 * aout_Close: close the Esound socket
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    arts_close_stream( p_aout->p_sys->stream );
}

