/*****************************************************************************
 * x11_timer.cpp: helper class to implement timers
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_timer.cpp,v 1.3 2003/06/08 11:33:14 asmax Exp $
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
    _nextDate = 0;
    vlc_mutex_init( p_intf, &_lock );
}


X11Timer::~X11Timer()
{
    vlc_mutex_destroy( &_lock );
}


void X11Timer::SetDate( mtime_t date )
{
    _nextDate = date + _interval;
}


mtime_t X11Timer::GetNextDate()
{
    return _nextDate;
}


bool X11Timer::Execute()
{
    _nextDate += _interval;
    return (*_callback)( _data );
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

X11TimerManager *X11TimerManager::_instance = NULL;


X11TimerManager::X11TimerManager( intf_thread_t *p_intf )
{
    _p_intf = p_intf;
    
    vlc_mutex_init( p_intf, &_lock );
    
    // Create the timer thread
    _p_timer = (timer_thread_t*)vlc_object_create( _p_intf,
                                                   sizeof( timer_thread_t ) );
    _p_timer->die = 0;
}


X11TimerManager::~X11TimerManager()
{
    _p_timer->die = 1;
    vlc_thread_join( _p_timer );

    vlc_mutex_destroy( &_lock );
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
        _instance->WaitNextTimer();
    }
}


void X11TimerManager::WaitNextTimer()
{
    mtime_t curDate = mdate();
    mtime_t nextDate = LAST_MDATE;

    X11Timer *nextTimer = NULL;
 
    Lock();       
    // Find the next timer to execute
    list<X11Timer*>::iterator timer;
    for( timer = _timers.begin(); timer != _timers.end(); timer++ )
    {
        mtime_t timerDate = (*timer)->GetNextDate();
        if( timerDate < nextDate )
        {
            nextTimer = *timer;
            nextDate = timerDate;
        }
    }
    Unlock();
    
    if( nextTimer == NULL )
    {
        // FIXME: should wait on a cond instead
        msleep( 10000 );
    }
    else
    {
        if( nextDate > curDate )
        {
            mwait( nextDate );
        }
        bool ret = nextTimer->Execute();
        if( !ret ) 
        {   
            _timers.remove( nextTimer );
        }
    }
}


void X11TimerManager::addTimer( X11Timer *timer )
{ 
   timer->SetDate( mdate() );
    _timers.push_back( timer ); 
}


void X11TimerManager::removeTimer( X11Timer *timer ) 
{
    Lock();
    timer->Lock();
    _timers.remove( timer ); 
    Unlock();
    timer->Unlock();
}


#endif
