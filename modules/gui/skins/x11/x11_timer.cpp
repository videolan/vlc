/*****************************************************************************
 * x11_timer.cpp: helper class to implement timers
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_timer.cpp,v 1.1 2003/06/05 22:16:15 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#ifdef X11_SKINS

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>
#include <mtime.h>

//--- SKIN ------------------------------------------------------------------
#include "x11_timer.h"


//---------------------------------------------------------------------------

X11Timer::X11Timer( intf_thread_t *p_intf, mtime_t interval, callback_t func,
                    void *data )
{
    _p_intf = p_intf;
    _interval = interval;
    _callback = func;
    _data = data;
}


X11Timer::~X11Timer()
{
}


mtime_t X11Timer::getNextDate( mtime_t current )
{
    return (current / _interval + 1) * _interval;
}


void X11Timer::Execute()
{
    (*_callback)( _data );
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

X11TimerManager *X11TimerManager::_instance = NULL;


X11TimerManager::X11TimerManager( intf_thread_t *p_intf )
{
    _p_intf = p_intf;
    
    // Create the timer thread
    _p_timer = (timer_thread_t*)vlc_object_create( _p_intf,
                                                   sizeof( timer_thread_t ) );
    _p_timer->die = 0;
}


X11TimerManager::~X11TimerManager()
{
    _p_timer->die = 1;
    vlc_thread_join( _p_timer );
}


// Return the instance of X11TimerManager (design pattern singleton)
X11TimerManager *X11TimerManager::Instance( intf_thread_t *p_intf )
{
    if( _instance == NULL )
    {
        _instance = new X11TimerManager( p_intf );
        // Run the timer thread
        vlc_thread_create( _instance->_p_timer, "Skins timer thread", 
                           &Thread, 0, VLC_TRUE );
    }
    return _instance;
}


// Destroy the instance, if any
void X11TimerManager::Destroy()
{
    if( _instance != NULL )
    {
        delete _instance;
    }
}

 
// Main timer loop
void *X11TimerManager::Thread( void *p_timer )
{
    vlc_thread_ready( (vlc_object_t*) p_timer );
    while( !((timer_thread_t*)p_timer)->die )
    {
        list<X11Timer*>::iterator timer;
        // FIXME temporary
        for( timer = _instance->_timers.begin(); 
             timer != _instance->_timers.end(); timer++ )
        {
            (*timer)->Execute();
        }
        msleep( 100000 );
    }
    
}

#endif
