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
#include <vlc_cxx_helpers.hpp>

#include "qt.hpp"


template <typename T>
class Singleton
{
public:
    template <bool create, class = typename std::enable_if<!create>::type>
    static T* getInstance( void )
    {
        vlc::threads::mutex_locker lock( m_mutex );
        return m_instance;
    }

    template <class T2 = T, typename... Args>
    static T* getInstance( Args&&... args )
    {
        vlc::threads::mutex_locker lock( m_mutex );
        if ( !m_instance )
          m_instance = new T2( std::forward<Args>( args )... );
        return m_instance;
    }

    static void killInstance()
    {
        vlc::threads::mutex_locker lock( m_mutex );
        delete m_instance;
        m_instance = nullptr;
    }

protected:
    Singleton(){}
    virtual ~Singleton(){}
    /* Not implemented since these methods should *NEVER* been called.
    If they do, it probably won't compile :) */
    Singleton(const Singleton<T>&);
    Singleton<T>&   operator=(const Singleton<T>&);

private:
    static T* m_instance;
    static vlc::threads::mutex m_mutex;
};
template <typename T>
T* Singleton<T>::m_instance = nullptr;
template <typename T>
vlc::threads::mutex Singleton<T>::m_mutex;

#endif // include-guard
