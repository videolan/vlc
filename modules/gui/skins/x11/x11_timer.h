/*****************************************************************************
 * x11_timer.h: helper class to implement timers
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_timer.h,v 1.3 2003/06/08 11:33:14 asmax Exp $
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


#ifndef VLC_SKIN_X11_TIMER
#define VLC_SKIN_X11_TIMER

#include <list.h>

typedef struct
{
    VLC_COMMON_MEMBERS
    int die;
} timer_thread_t;

class X11Timer;  // forward declaration

typedef bool(*callback_t)( void* );

//---------------------------------------------------------------------------
class X11Timer
{
    private:
        intf_thread_t *_p_intf;
        mtime_t _interval;       
        callback_t _callback;
        void *_data;
        vlc_mutex_t _lock;
        mtime_t _nextDate;

    public:
        X11Timer( intf_thread_t *p_intf, mtime_t interval, callback_t func, 
                  void *data );
        ~X11Timer();

        void SetDate( mtime_t date );
        mtime_t GetNextDate();
        bool Execute();
        
        inline void Lock() { vlc_mutex_lock( &_lock ); }
        inline void Unlock() { vlc_mutex_unlock( &_lock ); }
};
//---------------------------------------------------------------------------
class X11TimerManager
{
    private:
        static X11TimerManager *_instance;
        intf_thread_t *_p_intf;
        timer_thread_t *_p_timer;
        list<X11Timer*> _timers;
        vlc_mutex_t _lock;
        
        X11TimerManager( intf_thread_t *p_intf );
        ~X11TimerManager();

        static void *Thread( void *p_timer );
        void WaitNextTimer();

    public:
        static X11TimerManager *Instance( intf_thread_t *p_intf );
        void Destroy();

        void addTimer( X11Timer *timer );
        void removeTimer( X11Timer *timer );

        inline void Lock() { vlc_mutex_lock( &_lock ); }
        inline void Unlock() { vlc_mutex_unlock( &_lock ); }
};
//---------------------------------------------------------------------------
#endif
