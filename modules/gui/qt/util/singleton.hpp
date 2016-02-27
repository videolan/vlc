/*****************************************************************************
 * singleton.hpp: Generic Singleton pattern implementation
 ****************************************************************************
 * Copyright (C) 2009 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_SINGLETON_HPP_
#define VLC_QT_SINGLETON_HPP_

#include <stdlib.h>
#include <vlc_threads.h>

#include "qt.hpp"

template <typename T>
class       Singleton
{
public:
    static T*      getInstance( intf_thread_t *p_intf = NULL )
    {
        vlc_mutex_locker lock( &m_mutex );
        if ( m_instance == NULL )
            m_instance = new T( p_intf );
        return m_instance;
    }

    static void    killInstance()
    {
        vlc_mutex_locker lock( &m_mutex );
        if ( m_instance != NULL )
        {
            delete m_instance;
            m_instance = NULL;
        }
    }
protected:
    Singleton(){}
    virtual ~Singleton(){}
    /* Not implemented since these methods should *NEVER* been called.
    If they do, it probably won't compile :) */
    Singleton(const Singleton<T>&);
    Singleton<T>&   operator=(const Singleton<T>&);

private:
    static T*      m_instance;
    static vlc_mutex_t m_mutex;
};

template <typename T>
T*  Singleton<T>::m_instance = NULL;

template <typename T>
vlc_mutex_t Singleton<T>::m_mutex = VLC_STATIC_MUTEX;

#endif // include-guard
