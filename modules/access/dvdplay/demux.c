/*****************************************************************************
 * demux.c: demux functions for dvdplay.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: demux.c,v 1.5 2003/02/07 00:26:23 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include "../../demux/mpeg/system.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include "interface.h"
#include "dvd.h"
#include "intf.h"
#include "es.h"

/* how many packets dvdplay_Demux will read in each loop */
#define dvdplay_READ_ONCE 64

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux         ( input_thread_t * );

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    dvd_data_t * p_dvd;

    module_t *   p_module;
    mpeg_demux_t mpeg;
};

/*****************************************************************************
 * InitDVD: initializes dvdplay structures
 *****************************************************************************/
int E_(InitDVD) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    dvd_data_t *    p_dvd = (dvd_data_t *)p_input->p_access_data;
    demux_sys_t *   p_demux;

    if( p_input->stream.i_method != INPUT_METHOD_DVD )
    {
        return VLC_EGENERIC;
    }

    p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t ) );
    if( p_demux == NULL )
    {
        return VLC_ENOMEM;
    }

    p_input->p_private = (void*)&p_demux->mpeg;
    p_demux->p_module = module_Need( p_input, "mpeg-system", NULL );
    if( p_demux->p_module == NULL )
    {
        free( p_input->p_demux_data );
        return VLC_ENOMOD;
    }

    p_input->p_demux_data->p_dvd = p_dvd;

    p_input->pf_demux = Demux;
    p_input->pf_rewind = NULL;

    p_dvd->p_intf = intf_Create( p_input, "dvdplay" );
    p_dvd->p_intf->b_block = VLC_FALSE;
    intf_RunThread( p_dvd->p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndDVD: frees unused data
 *****************************************************************************/
void E_(EndDVD) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    dvd_data_t *    p_dvd = p_input->p_demux_data->p_dvd;
    intf_thread_t * p_intf = NULL;

    p_intf = vlc_object_find( p_input, VLC_OBJECT_INTF, FIND_CHILD );
    if( p_intf != NULL )
    {
        intf_StopThread( p_intf );
        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        intf_Destroy( p_intf );
    }

    p_dvd->p_intf = NULL;

    module_Unneed( p_input, p_input->p_demux_data->p_module );
    free( p_input->p_demux_data );
}

/*****************************************************************************
 * Demux
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    dvd_data_t *            p_dvd;
    data_packet_t *         p_data;
    ssize_t                 i_result;
    ptrdiff_t               i_remains;
    int                     i_data_nb = 0;

    p_dvd = p_input->p_demux_data->p_dvd;

    /* Read headers to compute payload length */
    do
    {
        i_result = p_input->p_demux_data->mpeg.pf_read_ps( p_input, &p_data );

        if( i_result <= 0 )
        {
            return i_result;
        }

        i_remains = p_input->p_last_data - p_input->p_current_data;

        p_input->p_demux_data->mpeg.pf_demux_ps( p_input, p_data );


        ++i_data_nb;
    }
    while( i_remains );



//    if( p_dvd->b_still && p_dvd->b_end_of_cell && p_dvd->p_intf != NULL )
    if( p_dvd->i_still_time && p_dvd->b_end_of_cell && p_dvd->p_intf != NULL )
    {
        pgrm_descriptor_t * p_pgrm;

        /* when we receive still_time flag, we have to pause immediately */
        input_SetStatus( p_input, INPUT_STATUS_PAUSE );

        dvdIntfStillTime( p_dvd->p_intf, p_dvd->i_still_time );
        p_dvd->i_still_time = 0;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        p_pgrm = p_input->stream.p_selected_program;
        p_pgrm->i_synchro_state = SYNCHRO_REINIT;

        vlc_mutex_unlock( &p_input->stream.stream_lock );

        input_ClockManageControl( p_input, p_pgrm, 0 );
    }

    return i_data_nb;
}

