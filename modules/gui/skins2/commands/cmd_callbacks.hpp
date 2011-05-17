/*****************************************************************************
 * cmd_callbacks.hpp
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Author: Erwan Tulou      <erwan10 aT videolan doT org >
 *         JP Dinger        <jpd (at) videolan (dot) org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CMD_CALLBACKS_HPP
#define CMD_CALLBACKS_HPP

#include "cmd_generic.hpp"
#include "../src/vlcproc.hpp"


class CmdCallback : public CmdGeneric
{
public:
    CmdCallback( intf_thread_t *pIntf, vlc_object_t *pObj, vlc_value_t newVal,
                 void (VlcProc::*func)(vlc_object_t *,vlc_value_t),
                 string label )
        : CmdGeneric( pIntf ), m_pObj( pObj ), m_newVal( newVal ),
          m_label( label ), m_pfExecute( func )
    {
        if( m_pObj )
            vlc_object_hold( m_pObj );
    }
    virtual ~CmdCallback()
    {
        if( m_pObj )
            vlc_object_release( m_pObj );
    }
    virtual void execute()
    {
        if( !m_pObj || !m_pfExecute )
            return;

        (VlcProc::instance( getIntf() )->*m_pfExecute)( m_pObj, m_newVal );

        vlc_object_release( m_pObj );
        m_pObj = NULL;
    }
    virtual string getType() const { return m_label; }

private:
    vlc_object_t* m_pObj;
    vlc_value_t   m_newVal;
    string        m_label;
    void (VlcProc::*m_pfExecute)(vlc_object_t *,vlc_value_t);
};


class CmdExecuteBlock : public CmdGeneric
{
public:
    CmdExecuteBlock( intf_thread_t* pIntf, vlc_object_t* obj,
                     void (*func) (intf_thread_t*, vlc_object_t* ) )
        : CmdGeneric( pIntf), m_pObj( obj ), m_pfFunc( func ),
          m_executing( false )
    {
        vlc_mutex_init( &m_lock );
        vlc_cond_init( &m_wait );
        if( m_pObj )
            vlc_object_hold( m_pObj );
    }

    virtual ~CmdExecuteBlock()
    {
        if( m_pObj )
            vlc_object_release( m_pObj );
        vlc_cond_destroy( &m_wait );
        vlc_mutex_destroy( &m_lock );
    }

    static void executeWait( const CmdGenericPtr& rcCommand  )
    {
        CmdExecuteBlock& rCmd = (CmdExecuteBlock&)*rcCommand.get();
        vlc_mutex_locker locker( &rCmd.m_lock );

        if( !rCmd.m_pObj || !rCmd.m_pfFunc || rCmd.m_executing )
        {
            msg_Err( rCmd.getIntf(), "unexpected command call" );
            return;
        }

        AsyncQueue *pQueue = AsyncQueue::instance( rCmd.getIntf() );
        pQueue->push( rcCommand, false );

        rCmd.m_executing = true;
        while( rCmd.m_executing )
            vlc_cond_wait( &rCmd.m_wait, &rCmd.m_lock );
    }

    virtual void execute()
    {
        vlc_mutex_locker locker( &m_lock );

        if( !m_pObj || !m_pfFunc || !m_executing )
        {
            msg_Err( getIntf(), "unexpected command call" );
            return;
        }

        (*m_pfFunc)( getIntf(), m_pObj );
        m_executing = false;
        vlc_cond_signal( &m_wait );
    }

    virtual string getType() const { return "CmdExecuteBlock"; }

private:
    vlc_object_t* m_pObj;
    void          (*m_pfFunc)(intf_thread_t*, vlc_object_t*);
    bool          m_executing;

    vlc_mutex_t   m_lock;
    vlc_cond_t    m_wait;
};


#endif
