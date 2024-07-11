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

#include <vlc_actions.h>
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
    ResetPci();
}

void event_thread_t::SetPci(const pci_t *data)
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;
    vlc_mutex_locker l(&lock);

    if(es_list.empty())
        return;

    auto interpretor = p_sys->GetDVDInterpretor();
    if (!interpretor)
        return;

    interpretor->SetPci( data );
    if( !is_running )
    {
        b_abort = false;
        is_running = !vlc_clone( &thread, EventThread, this );
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

int event_thread_t::SendEventNav( int nav_query )
{
    if( !is_running )
        return VLC_EGENERIC;

    vlc_mutex_locker lock_guard( &lock );

    pending_events.push_back( EventInfo( nav_query ) );

    vlc_cond_signal( &wait );

    return VLC_SUCCESS;
}

void event_thread_t::EventMouse( vlc_mouse_t const* new_state, void* userdata )
{
    ESInfo* info = static_cast<ESInfo*>( userdata );
    vlc_mutex_locker lock_guard( &info->owner.lock );

    if( !new_state )
        return vlc_mouse_Init( &info->mouse_state );

    info->owner.pending_events.push_back(
        EventInfo( info, info->mouse_state, *new_state ) );

    vlc_cond_signal( &info->owner.wait );
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
            EventInfo const& ev = pending_events.front();

            switch( ev.type )
            {
                case EventInfo::ESMouseEvent:
                    HandleMouseEvent( ev );
                    break;

                case EventInfo::ActionEvent:
                    HandleKeyEvent( ev );
                    break;
            }

            pending_events.pop_front();
        }
    }

    vlc_restorecancel (canc);
}

void *event_thread_t::EventThread(void *data)
{
    static_cast<event_thread_t*>(data)->EventThread();
    return NULL;
}

void event_thread_t::HandleKeyEvent( EventInfo const& ev )
{
    msg_Dbg( p_demux, "Handle Key Event");

    NavivationKey key;
    switch( ev.nav.query )
    {
    case DEMUX_NAV_LEFT:     key = NavivationKey::LEFT;
    case DEMUX_NAV_RIGHT:    key = NavivationKey::RIGHT;
    case DEMUX_NAV_UP:       key = NavivationKey::UP;
    case DEMUX_NAV_DOWN:     key = NavivationKey::DOWN;
    case DEMUX_NAV_ACTIVATE: key = NavivationKey::OK;
    case DEMUX_NAV_MENU:     key = NavivationKey::MENU;
    case DEMUX_NAV_POPUP:    key = NavivationKey::POPUP;
    default:                 return;
    }

    HandleKeyEvent( key );
}

void event_thread_t::HandleKeyEvent( NavivationKey key )
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;

    auto interpretor = p_sys->GetDVDInterpretor();
    if (!interpretor)
        return;

    vlc_mutex_unlock( &lock );
    vlc_mutex_lock( &p_sys->lock_demuxer );

    // process the button action
    interpretor->HandleKeyEvent( key );

    vlc_mutex_unlock( &p_sys->lock_demuxer );
    vlc_mutex_lock( &lock );
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

void event_thread_t::HandleMousePressed( unsigned x, unsigned y )
{
    demux_sys_t* p_sys = (demux_sys_t*)p_demux->p_sys;

    auto interpretor = p_sys->GetDVDInterpretor();
    if (!interpretor)
        return;

    msg_Dbg( p_demux, "Handle Mouse Event: Mouse clicked x(%d)*y(%d)", x, y);

    vlc_mutex_unlock( &lock );
    vlc_mutex_lock( &p_sys->lock_demuxer );
    interpretor->HandleMousePressed( x, y );
    vlc_mutex_unlock( &p_sys->lock_demuxer );
    vlc_mutex_lock( &lock );
}

void event_thread_t::SetHighlight( vlc_spu_highlight_t & spu_hl )
{
    /* TODO: only control relevant SPU_ES given who fired the event */
    for( auto it : es_list )
    {
        if( it.category != SPU_ES )
            continue;

        es_out_Control( p_demux->out, ES_OUT_SPU_SET_HIGHLIGHT, it.es, &spu_hl );
    }
}

bool event_thread_t::AddES( es_out_id_t* es, int category )
{
    vlc_mutex_locker lock_guard( &lock );

    es_list.push_front( ESInfo( es, category, *this ) );
    es_list_t::iterator info = es_list.begin();

    if( category == VIDEO_ES )
    {
        if( es_out_Control( p_demux->out, ES_OUT_VOUT_SET_MOUSE_EVENT,
                            es, EventMouse, static_cast<void*>( &*info ) ) )
        {
            msg_Warn( p_demux, "Unable to subscribe to mouse events" );
            es_list.erase( info );
            return false;
        }
    }
    return true;
}

void event_thread_t::DelES( es_out_id_t* es )
{
    vlc_mutex_locker lock_guard( &lock );
    es_list_t::iterator info = std::find( es_list.begin(), es_list.end(), es );
    if( info != es_list.end() )
        es_list.erase( info );
}

} // namespace

