/*****************************************************************************
 * events.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "mkv.hpp"
#include "demux.hpp"
#include "events.hpp"

#include <vlc_actions.h>
#include <vlc_threads.h>

#include <algorithm>

namespace mkv {

event_thread_t::event_thread_t(demux_t *p_demux) : p_demux(p_demux)
{
    vlc_mutex_init( &lock );
    vlc_cond_init( &wait );
    is_running = false;
    memset(&pci_packet, 0, sizeof(pci_packet));
}
event_thread_t::~event_thread_t()
{
    ResetPci();
    vlc_cond_destroy( &wait );
    vlc_mutex_destroy( &lock );
}

void event_thread_t::SetPci(const pci_t *data)
{
    vlc_mutex_locker l(&lock);

    pci_packet = *data;

#ifndef WORDS_BIGENDIAN
    for( uint8_t button = 1; button <= pci_packet.hli.hl_gi.btn_ns; button++) {
        btni_t *button_ptr = &(pci_packet.hli.btnit[button-1]);
        binary *p_data = (binary*) button_ptr;

        uint16 i_x_start = ((p_data[0] & 0x3F) << 4 ) + ( p_data[1] >> 4 );
        uint16 i_x_end   = ((p_data[1] & 0x03) << 8 ) + p_data[2];
        uint16 i_y_start = ((p_data[3] & 0x3F) << 4 ) + ( p_data[4] >> 4 );
        uint16 i_y_end   = ((p_data[4] & 0x03) << 8 ) + p_data[5];
        button_ptr->x_start = i_x_start;
        button_ptr->x_end   = i_x_end;
        button_ptr->y_start = i_y_start;
        button_ptr->y_end   = i_y_end;

    }
    for ( uint8_t i = 0; i<3; i++ )
        for ( uint8_t j = 0; j<2; j++ )
            pci_packet.hli.btn_colit.btn_coli[i][j] = U32_AT( &pci_packet.hli.btn_colit.btn_coli[i][j] );
#endif
    if( !is_running )
    {
        b_abort = false;
        is_running = !vlc_clone( &thread, EventThread, this, VLC_THREAD_PRIORITY_LOW );
    }
}
void event_thread_t::ResetPci()
{
    if( !is_running )
        return;

    vlc_mutex_lock( &lock );
    b_abort = true;
    vlc_cond_signal( &wait );
    vlc_mutex_unlock( &lock );

    vlc_join( thread, NULL );
    is_running = false;
}
int event_thread_t::EventMouse( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t, vlc_value_t, void *p_data )
{
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    if( psz_var[6] == 'c' )
    {
        p_ev->b_clicked = true;
        msg_Dbg( p_this, "Event Mouse: clicked");
    }
    else if( psz_var[6] == 'm' )
        p_ev->b_moved = true;
    vlc_cond_signal( &p_ev->wait );
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

int event_thread_t::EventKey( vlc_object_t *p_this, char const *,
                              vlc_value_t, vlc_value_t newval, void *p_data )
{
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    p_ev->i_key_action = newval.i_int;
    vlc_cond_signal( &p_ev->wait );
    vlc_mutex_unlock( &p_ev->lock );
    msg_Dbg( p_this, "Event Key");

    return VLC_SUCCESS;
}

int event_thread_t::EventInput( vlc_object_t *p_this, char const *,
                                vlc_value_t, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( p_this );
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    if( newval.i_int == INPUT_EVENT_VOUT )
    {
        p_ev->b_vout |= true;
        vlc_cond_signal( &p_ev->wait );
    }
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

void event_thread_t::EventThread()
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    vlc_object_t   *p_vout = NULL;
    int canc = vlc_savecancel ();

    b_moved      = false;
    b_clicked    = false;
    i_key_action = 0;
    b_vout       = true;

    /* catch all key event */
    var_AddCallback( p_demux->obj.libvlc, "key-action", EventKey, this );
    /* catch input event */
    var_AddCallback( p_demux->p_input, "intf-event", EventInput, this );

    /* main loop */
    for( ;; )
    {
        vlc_mutex_lock( &lock );
        while( !b_abort && !i_key_action && !b_moved && !b_clicked && !b_vout)
            vlc_cond_wait( &wait, &lock );

        if( b_abort )
        {
            vlc_mutex_unlock( &lock );
            break;
        }

        /* KEY part */
        if( i_key_action )
            HandleKeyEvent();

        /* MOUSE part */
        if( p_vout && ( b_moved || b_clicked ) )
            HandleMouseEvent( p_vout );

        while( !pending_events.empty() )
        {
            /* TODO: handle events here */
            pending_events.pop_front();
        }


        b_vout = false;
        vlc_mutex_unlock( &lock );

        /* Always check vout */
        if( p_vout == NULL )
        {
            p_vout = (vlc_object_t*) input_GetVout(p_demux->p_input);
            if( p_vout)
            {
                var_AddCallback( p_vout, "mouse-moved", EventMouse, this );
                var_AddCallback( p_vout, "mouse-clicked", EventMouse, this );
            }
        }
    }

    /* Release callback */
    if( p_vout )
    {
        var_DelCallback( p_vout, "mouse-moved", EventMouse, this );
        var_DelCallback( p_vout, "mouse-clicked", EventMouse, this );
        vlc_object_release( p_vout );
    }
    var_DelCallback( p_demux->p_input, "intf-event", EventInput, this );
    var_DelCallback( p_demux->obj.libvlc, "key-action", EventKey, this );

    vlc_restorecancel (canc);
}

void *event_thread_t::EventThread(void *data)
{
    static_cast<event_thread_t*>(data)->EventThread();
    return NULL;
}

void event_thread_t::HandleKeyEvent()
{
    msg_Dbg( p_demux, "Handle Key Event");

    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;
    pci_t *pci = &pci_packet;

    uint16 i_curr_button = p_sys->dvd_interpretor.GetSPRM( 0x88 );

    switch( i_key_action )
    {
    case ACTIONID_NAV_LEFT:
        if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
        {
            btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
            if ( p_button_ptr->left > 0 && p_button_ptr->left <= pci->hli.hl_gi.btn_ns )
            {
                i_curr_button = p_button_ptr->left;
                p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                if ( button_ptr.auto_action_mode )
                {
                    vlc_mutex_unlock( &lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &lock );
                }
            }
        }
        break;
    case ACTIONID_NAV_RIGHT:
        if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
        {
            btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
            if ( p_button_ptr->right > 0 && p_button_ptr->right <= pci->hli.hl_gi.btn_ns )
            {
                i_curr_button = p_button_ptr->right;
                p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                if ( button_ptr.auto_action_mode )
                {
                    vlc_mutex_unlock( &lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &lock );
                }
            }
        }
        break;
    case ACTIONID_NAV_UP:
        if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
        {
            btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
            if ( p_button_ptr->up > 0 && p_button_ptr->up <= pci->hli.hl_gi.btn_ns )
            {
                i_curr_button = p_button_ptr->up;
                p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                if ( button_ptr.auto_action_mode )
                {
                    vlc_mutex_unlock( &lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &lock );
                }
            }
        }
        break;
    case ACTIONID_NAV_DOWN:
        if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
        {
            btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
            if ( p_button_ptr->down > 0 && p_button_ptr->down <= pci->hli.hl_gi.btn_ns )
            {
                i_curr_button = p_button_ptr->down;
                p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                if ( button_ptr.auto_action_mode )
                {
                    vlc_mutex_unlock( &lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &lock );
                }
            }
        }
        break;
    case ACTIONID_NAV_ACTIVATE:
        if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
        {
            btni_t button_ptr = pci->hli.btnit[i_curr_button-1];

            vlc_mutex_unlock( &lock );
            vlc_mutex_lock( &p_sys->lock_demuxer );

            // process the button action
            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

            vlc_mutex_unlock( &p_sys->lock_demuxer );
            vlc_mutex_lock( &lock );
        }
        break;
    default:
        break;
    }
    i_key_action = 0;
}

void event_thread_t::HandleMouseEvent( vlc_object_t* p_vout )
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;
    int x, y;

    var_GetCoords( p_vout, "mouse-moved", &x, &y );
    pci_t *pci = &pci_packet;

    if( b_clicked )
    {
        int32_t button;
        int32_t best,dist,d;
        int32_t mx,my,dx,dy;

        msg_Dbg( p_demux, "Handle Mouse Event: Mouse clicked x(%d)*y(%d)", x, y);

        // get current button
        best = 0;
        dist = 0x08000000; /* >> than  (720*720)+(567*567); */
        for(button = 1; button <= pci->hli.hl_gi.btn_ns; button++)
        {
            btni_t *button_ptr = &(pci->hli.btnit[button-1]);

            if(((unsigned)x >= button_ptr->x_start)
             && ((unsigned)x <= button_ptr->x_end)
             && ((unsigned)y >= button_ptr->y_start)
             && ((unsigned)y <= button_ptr->y_end))
            {
                mx = (button_ptr->x_start + button_ptr->x_end)/2;
                my = (button_ptr->y_start + button_ptr->y_end)/2;
                dx = mx - x;
                dy = my - y;
                d = (dx*dx) + (dy*dy);
                /* If the mouse is within the button and the mouse is closer
                * to the center of this button then it is the best choice. */
                if(d < dist) {
                    dist = d;
                    best = button;
                }
            }
        }

        if ( best != 0)
        {
            btni_t button_ptr = pci->hli.btnit[best-1];
            uint16 i_curr_button = p_sys->dvd_interpretor.GetSPRM( 0x88 );

            msg_Dbg( &p_sys->demuxer, "Clicked button %d", best );
            vlc_mutex_unlock( &lock );
            vlc_mutex_lock( &p_sys->lock_demuxer );

            // process the button action
            p_sys->dvd_interpretor.SetSPRM( 0x88, best );
            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

            msg_Dbg( &p_sys->demuxer, "Processed button %d", best );

            // select new button
            if ( best != i_curr_button )
            {
                uint32_t i_palette;

                if(button_ptr.btn_coln != 0) {
                    i_palette = pci->hli.btn_colit.btn_coli[button_ptr.btn_coln-1][1];
                } else {
                    i_palette = 0;
                }

                for( int i = 0; i < 4; i++ )
                {
                    uint32_t i_yuv = 0xFF;//p_sys->clut[(hl.palette>>(16+i*4))&0x0f];
                    uint8_t i_alpha = (i_palette>>(i*4))&0x0f;
                    i_alpha = i_alpha == 0xf ? 0xff : i_alpha << 4;

                    p_sys->palette[i][0] = (i_yuv >> 16) & 0xff;
                    p_sys->palette[i][1] = (i_yuv >> 0) & 0xff;
                    p_sys->palette[i][2] = (i_yuv >> 8) & 0xff;
                    p_sys->palette[i][3] = i_alpha;
                }

                vlc_global_lock( VLC_HIGHLIGHT_MUTEX );
                var_SetInteger( p_demux->p_input, "x-start",
                                button_ptr.x_start );
                var_SetInteger( p_demux->p_input, "x-end",
                                button_ptr.x_end );
                var_SetInteger( p_demux->p_input, "y-start",
                                button_ptr.y_start );
                var_SetInteger( p_demux->p_input, "y-end",
                                button_ptr.y_end );
                var_SetAddress( p_demux->p_input, "menu-palette",
                                p_sys->palette );
                var_SetBool( p_demux->p_input, "highlight", true );
                vlc_global_unlock( VLC_HIGHLIGHT_MUTEX );
            }
            vlc_mutex_unlock( &p_sys->lock_demuxer );
            vlc_mutex_lock( &lock );
        }
    }
    else if( b_moved )
    {
//                dvdnav_mouse_select( NULL, pci, x, y );
    }

    b_moved = false;
    b_clicked = false;
}

void event_thread_t::AddES( es_out_id_t* es, int category )
{
    vlc_mutex_locker lock_guard( &lock );

    es_list.push_back( ESInfo( es, category, *this ) );
    es_list_t::reverse_iterator info = es_list.rbegin();

    /* TODO:
     *  - subscribe to events if required,
     *  - use &*info as callback data if necessary
     **/

    VLC_UNUSED( info );
}

void event_thread_t::DelES( es_out_id_t* es )
{
    vlc_mutex_locker lock_guard( &lock );
    es_list.erase( std::find( es_list.begin(), es_list.end(), es ) );
}

} // namespace

