/*****************************************************************************
 * singleton.hpp: Generic Singleton pattern implementation
 ****************************************************************************
 * Copyright (C) 2009 VideoLAN
 *
 * Authors: Hugo Beauzee-Luyssen <beauze.h # gmail - com>
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

#ifndef _SINGLETON_HPP_
#define _SINGLETON_HPP_

#include <stdlib.h>
#include "qt4.hpp"

template <typename T>
class       Singleton
{
public:
    static T*      getInstance( intf_thread_t *p_intf = NULL )
    {
        if ( m_instance == NULL )
            m_instance = new T( p_intf );
        return m_instance;
    }

    static void    killInstance()
    {
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
};

template <typename T>
T*  Singleton<T>::m_instance = NULL;

#endif // _SINGLETON_HPP_
