/*****************************************************************************
 * events.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
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
#include "chapter_command_dvd.hpp"

#include <vlc_threads.h>

#include <algorithm>

namespace mkv {

event_thread_t::event_thread_t(demux_t *p_demux) : p_demux(p_demux)
{
    vlc_mutex_init( &lock );
    vlc_cond_init( &wait );
    is_running = false;
}
event_thread_t::~event_thread_t()
{
    AbortThread();
}

void event_thread_t::SendData( mkv_track_t &track, block_t * p_block )
{
    if ( track.codec == "B_VOBBTN")
    {
        if( p_block->i_size > 0)
        {
            QueueEvent( EventInfo{ p_block } );
            return; // the block will be released later
        }
    }

    block_Release( p_block );
}

void event_thread_t::EnsureThreadLocked()
{
    if (is_running || b_abort)
        return;

    is_running = !vlc_clone( &thread, EventThread, this );
}

void event_thread_t::AbortThread()
{
    if( !is_running )
        return;

    {
        vlc_mutex_locker lock_guard(&lock);
        b_abort = true;
        vlc_cond_signal( &wait );
    }

    vlc_join( thread, NULL );
    is_running = false;
}

int event_thread_t::SendEventNav( demux_query_e nav_query )
{
    NavivationKey key;
    switch( nav_query )
    {
    case DEMUX_NAV_LEFT:     key = NavivationKey::LEFT;  break;
    case DEMUX_NAV_RIGHT:    key = NavivationKey::RIGHT; break;
    case DEMUX_NAV_UP:       key = NavivationKey::UP;    break;
    case DEMUX_NAV_DOWN:     key = NavivationKey::DOWN;  break;
    case DEMUX_NAV_ACTIVATE: key = NavivationKey::OK;    break;
    case DEMUX_NAV_MENU:     key = NavivationKey::MENU;  break;
    case DEMUX_NAV_POPUP:    key = NavivationKey::POPUP; break;
    default:
        assert(false); // invalid navigation query received
        return VLC_ENOTSUP;
    }

    if( !is_running )
        return VLC_EGENERIC;

    if ( !HandleKeyEvent( key ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

void event_thread_t::EventMouse( vlc_mouse_t const* new_state, void* userdata )
{
    ESInfo* info = static_cast<ESInfo*>( userdata );

    if( !new_state )
        return vlc_mouse_Init( &info->mouse_state );

    info->owner.QueueEvent( EventInfo{ info->mouse_state, *new_state } );
    info->mouse_state = *new_state;
}

void event_thread_t::EventThread()
{
    vlc_thread_set_name("vlc-mkv-events");

    int canc = vlc_savecancel ();

    for( vlc_mutex_locker guard( &lock );; )
    {
        while( !b_abort && pending_events.empty() )
            vlc_cond_wait( &wait, &lock );

        if( b_abort )
            break;

        while( !pending_events.empty() )
        {
            const EventInfo ev = pending_events.front();
            pending_events.pop_front();

            if(es_list.empty())
                break;

            vlc_mutex_unlock( &lock );
            switch( ev.type )
            {
                case EventInfo::ESMouseEvent:
                    HandleMouseEvent( ev );
                    break;

                case EventInfo::ButtonDataEvent:
                    HandleButtonData( ev );
                    break;
            }
            vlc_mutex_lock( &lock );
        }
    }

    vlc_restorecancel (canc);
}

void *event_thread_t::EventThread(void *data)
{
    static_cast<event_thread_t*>(data)->EventThread();
    return NULL;
}

bool event_thread_t::HandleKeyEvent( NavivationKey key )
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;

    vlc_mutex_locker demux_lock ( &p_sys->lock_demuxer );

    auto interpretor = p_sys->GetDVDInterpretor();
    if (!interpretor)
        return false;

    // process the button action
    return interpretor->HandleKeyEvent( key );
}

void event_thread_t::HandleMouseEvent( EventInfo const& event )
{
    int x = event.mouse.state_new.i_x;
    int y = event.mouse.state_new.i_y;

    if( vlc_mouse_HasPressed( &event.mouse.state_old, &event.mouse.state_new,
                              MOUSE_BUTTON_LEFT ) )
    {
        HandleMousePressed( (unsigned)x, (unsigned)y );
    }
    else if( vlc_mouse_HasMoved( &event.mouse.state_old, &event.mouse.state_new ) )
    {
//                dvdnav_mouse_select( NULL, pci, x, y );
    }
}

void event_thread_t::HandleButtonData( EventInfo const& event )
{
    HandleButtonData( event.button_data.get() );
}

void event_thread_t::HandleButtonData( block_t *p_block )
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;

    vlc_mutex_locker demux_lock ( &p_sys->lock_demuxer );

    auto interpretor = p_sys->GetDVDInterpretor();
    if (!interpretor)
        return;

    interpretor->SetPci( &p_block->p_buffer[1], p_block->i_size - 1 );
}

void event_thread_t::HandleMousePressed( unsigned x, unsigned y )
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;

    msg_Dbg( p_demux, "Handle Mouse Event: Mouse clicked x(%d)*y(%d)", x, y);

    vlc_mutex_locker demux_lock ( &p_sys->lock_demuxer );

    auto interpretor = p_sys->GetDVDInterpretor();
    if (!interpretor)
        return;

    interpretor->HandleMousePressed( x, y );
}

void event_thread_t::SetHighlight( vlc_spu_highlight_t & spu_hl )
{
    /* TODO: only control relevant SPU_ES given who fired the event */
    for( auto it : es_list )
    {
        if( it.track.fmt.i_cat != SPU_ES )
            continue;

        es_out_Control( p_demux->out, ES_OUT_SPU_SET_HIGHLIGHT, it.track.p_es, &spu_hl );
    }
}

bool event_thread_t::AddTrack( mkv_track_t & track )
{
    es_out_id_t* es = track.p_es;
    int category = track.fmt.i_cat;

    vlc_mutex_locker lock_guard( &lock );

    es_list.push_front( ESInfo( track, *this ) );
    es_list_t::iterator info = es_list.begin();

    if( category == VIDEO_ES )
    {
        if( es_out_Control( p_demux->out, ES_OUT_VOUT_SET_MOUSE_EVENT,
                            es, static_cast<vlc_mouse_event>(EventMouse),
                            static_cast<void*>( &*info ) ) )
        {
            msg_Warn( p_demux, "Unable to subscribe to mouse events" );
            es_list.erase( info );
            return false;
        }
    }
    return true;
}

void event_thread_t::DelTrack( mkv_track_t &track )
{
    vlc_mutex_locker lock_guard( &lock );
    es_list_t::iterator info = std::find( es_list.begin(), es_list.end(), track );
    if( info != es_list.end() )
        es_list.erase( info );
}

} // namespace

