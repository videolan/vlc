/*****************************************************************************
 * async_queue.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef ASYNC_QUEUE_HPP
#define ASYNC_QUEUE_HPP

#include "cmd_generic.hpp"

#include <list>
#include <string>

class OSTimer;


/// Asynchronous queue for commands
class AsyncQueue: public SkinObject
{
    public:
        /// Get the instance of AsyncQueue
        /// Returns NULL if initialization failed.
        static AsyncQueue *instance( intf_thread_t *pIntf );

        /// Destroy the instance of AsyncQueue
        static void destroy( intf_thread_t *pIntf );

        /// Add a command in the queue
        void push( const CmdGenericPtr &rcCommand );

        /// Remove the commands of the given type
        void remove( const string &rType );

        /// Flush the queue and execute the commands
        void flush();

    private:
        /// Command queue
        list<CmdGenericPtr> m_cmdList;
        /// Timer
        OSTimer *m_pTimer;
        /// Mutex
        vlc_mutex_t m_lock;

        // Private because it is a singleton
        AsyncQueue( intf_thread_t *pIntf );
        virtual ~AsyncQueue();

        // Callback to flush the queue
        DEFINE_CALLBACK( AsyncQueue, Flush );
};


#endif
