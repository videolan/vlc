/*****************************************************************************
 * demux.c: demux functions for dvdplay.
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_interface.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include "vcd.h"
#include "vcdplayer.h"
#include "intf.h"

/* how many packets vcdx_Demux will read in each loop */
/* #define vcdplay_READ_ONCE 64 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux         ( input_thread_t * );

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    vcd_data_t * p_vcd;

    module_t *   p_module;
    mpeg_demux_t mpeg;
};

/*****************************************************************************
 * VCDInit: initializes structures
 *****************************************************************************/
int VCDInit ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    vcd_data_t *    p_vcd = (vcd_data_t *)p_input->p_sys;
    demux_sys_t *   p_demux;

    printf("++++ VCDInit CALLED\n");
 

    if( p_input->stream.i_method != INPUT_METHOD_VCD )
    {
        return VLC_EGENERIC;
    }

    p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t ) );
    if( p_demux == NULL )
    {
        return VLC_ENOMOD;
    }

    p_input->p_private = (void*)&p_demux->mpeg;
    p_demux->p_module = module_Need( p_input, "mpeg-system", NULL, 0 );
    if( p_demux->p_module == NULL )
    {
        free( p_input->p_demux_data );
        return VLC_ENOMOD;
    }

    p_input->p_demux_data->p_vcd = p_vcd;

    p_input->pf_demux = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->pf_rewind = NULL;

    p_vcd->p_intf = NULL;
    p_vcd->i_still_time = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * VCDEnd: frees unused data
 *****************************************************************************/
void VCDEnd ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    vcd_data_t *    p_vcd = p_input->p_demux_data->p_vcd;
    intf_thread_t * p_intf = NULL;

    p_intf = vlc_object_find( p_input, VLC_OBJECT_INTF, FIND_CHILD );
    if( p_intf != NULL )
    {
        intf_StopThread( p_intf );
        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        vlc_object_release( p_intf );
    }

    p_vcd->p_intf = NULL;

    module_Unneed( p_input, p_input->p_demux_data->p_module );
    free( p_input->p_demux_data );
}

/*****************************************************************************
 * Demux
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    vcd_data_t *            p_vcd;
    data_packet_t *         p_data;
    ssize_t                 i_result;
    ptrdiff_t               i_remains;
    int                     i_data_nb = 0;

    p_vcd = p_input->p_demux_data->p_vcd;

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



//    if( p_vcd->b_still && p_vcd->b_end_of_cell && p_vcd->p_intf != NULL )
    if( p_vcd->i_still_time && p_vcd->b_end_of_cell && p_vcd->p_intf != NULL )
    {
        pgrm_descriptor_t * p_pgrm;

        /* when we receive still_time flag, we have to pause immediately */
        var_SetInteger( p_input, "state", PAUSE_S );

        vcdIntfStillTime( p_vcd->p_intf, p_vcd->i_still_time );
        p_vcd->i_still_time = 0;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        p_pgrm = p_input->stream.p_selected_program;
        p_pgrm->i_synchro_state = SYNCHRO_REINIT;

        vlc_mutex_unlock( &p_input->stream.stream_lock );

        input_ClockManageControl( p_input, p_pgrm, 0 );
    }

    return i_data_nb;
}
