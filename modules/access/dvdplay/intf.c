/*****************************************************************************
 * intf.c: interface for DVD video manager
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf.c,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <unistd.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "video.h"
#include "video_output.h"

#include "dvd.h"

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    input_thread_t *    p_input;
    dvd_data_t *        p_dvd;

    vlc_bool_t          b_still;
    vlc_bool_t          b_inf_still;
    mtime_t             m_still_time;

};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  InitThread     ( intf_thread_t *p_intf );

/* Exported functions */
static void RunIntf        ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize dummy interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    p_intf->p_sys->m_still_time = 0;
    p_intf->p_sys->b_inf_still = 0;
    p_intf->p_sys->b_still = 0;

    return( 0 );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    vout_thread_t *     p_vout;
    dvdplay_ctrl_t      control;
    mtime_t             mtime = 0;
    mtime_t             mlast = 0;
    
    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize intf" );
        return;
    }
    msg_Dbg( p_intf, "intf initialized" );

    p_vout = NULL;
    control.mouse.i_x = 0;
    control.mouse.i_y = 0;
    
    /* Main loop */
    while( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->change_lock );

        /*
         * still images
         */
#if 1
        if( p_intf->p_sys->b_still && !p_intf->p_sys->b_inf_still )
        {
            if( p_intf->p_sys->m_still_time > 0 )
            {
                /* update remaining still time */
                mtime = mdate();
                if( mlast )
                {
                    p_intf->p_sys->m_still_time -= mtime - mlast;
                }

                mlast = mtime;
            }
            else
            {
                /* still time elasped */
                input_SetStatus( p_intf->p_sys->p_input,
                                 INPUT_STATUS_PLAY );
                p_intf->p_sys->m_still_time = 0;
                p_intf->p_sys->b_still = 0;
                mlast = 0;
            }
        }
#else
        if( p_intf->p_sys->m_still_time != (mtime_t)(-1) )
        {
            if( p_intf->p_sys->m_still_time )
            {
                mtime = mdate();
                if( mlast )
                {
                    p_intf->p_sys->m_still_time -= mtime - mlast;
                }
                if( !p_intf->p_sys->m_still_time )
                {
                    input_SetStatus( p_intf->p_sys->p_input,
                                     INPUT_STATUS_PLAY );
                }
                mlast = mtime;
            }

        }
#endif

        /* 
         * mouse cursor
         */
        p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                  VLC_OBJECT_VOUT, FIND_CHILD );
        if( p_vout != NULL )
        {
            vlc_mutex_lock( &p_vout->change_lock );

            if( control.mouse.i_x != p_vout->i_mouse_x ||
                control.mouse.i_y != p_vout->i_mouse_y ||
                p_vout->i_mouse_button )
            {
                int i_activate = 0;
                
                control.mouse.i_x = p_vout->i_mouse_x;
                control.mouse.i_y = p_vout->i_mouse_y;

                if( p_vout->i_mouse_button )
                {
                    control.type = DVDCtrlMouseActivate;
                    
                    msg_Dbg( p_intf, "Activate coordinates: %dx%d",
                                 p_vout->i_mouse_x, p_vout->i_mouse_y );
                }
                else
                {
                    control.type = DVDCtrlMouseSelect;
                    
                    msg_Dbg( p_intf, "Select coordinates: %dx%d",
                                 p_vout->i_mouse_x, p_vout->i_mouse_y );
                }
                p_vout->i_mouse_button = 0;
                vlc_mutex_unlock( &p_vout->change_lock );
                
                msg_Dbg( p_intf, "send button" );
                
                /* we can safely interact with libdvdplay
                 * with the stream lock */
                vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
                
                i_activate =
                    dvdplay_button( p_intf->p_sys->p_dvd->vmg, &control );
                  
                vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
                

                if( i_activate && p_intf->p_sys->b_still )
                {
                    input_SetStatus( p_intf->p_sys->p_input,
                                     INPUT_STATUS_PLAY );
                    p_intf->p_sys->b_still = 0;
                    p_intf->p_sys->b_inf_still = 0;
                    p_intf->p_sys->m_still_time = 0;
                }
            }
            else
            {
                vlc_mutex_unlock( &p_vout->change_lock );
            }
            

            vlc_object_release( p_vout );
        }
            
        vlc_mutex_unlock( &p_intf->change_lock );
          
        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }
    
    vlc_object_release( p_intf->p_sys->p_input );

}

/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    /* we might need some locking here */
    if( !p_intf->b_die )
    {
        input_thread_t * p_input;
        dvd_data_t * p_dvd;
        
        p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_PARENT );
        p_dvd = (dvd_data_t*)p_input->p_access_data;

        p_dvd->p_intf = p_intf;
                    
        vlc_mutex_lock( &p_intf->change_lock );
    
        p_intf->p_sys->p_input = p_input;
        p_intf->p_sys->p_dvd = p_dvd;

        vlc_mutex_unlock( &p_intf->change_lock );

        return 0;
    }
    else
    {
        return -1;
    }
}

/*****************************************************************************
 * dvdIntfStillTime: function provided to demux plugin to request
 * still images
 *****************************************************************************/
int dvdIntfStillTime( intf_thread_t *p_intf, int i_sec )
{
    vlc_mutex_lock( &p_intf->change_lock );
#if 1
    
    if( i_sec == 0xff )
    {
        p_intf->p_sys->b_still = 1;
        p_intf->p_sys->b_inf_still = 1;
    }
    else if( i_sec > 0 )
    {
        p_intf->p_sys->b_still = 1;
        p_intf->p_sys->m_still_time = 1000000 * i_sec;
    }
#else
    if( i_sec > 0 )
    {
        if( i_sec == 0xff )
        {
            p_intf->p_sys->m_still_time = (mtime_t)(-1);
            msg_Warn( p_intf, "%lld", p_intf->p_sys->m_still_time );
        }
        else
        {
            p_intf->p_sys->m_still_time = 1000000 * i_sec;
        }
        
    }
#endif
    vlc_mutex_unlock( &p_intf->change_lock );

    return 0;
}


