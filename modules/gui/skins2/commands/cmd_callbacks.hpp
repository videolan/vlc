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
          m_pfExecute( func ), m_label( label )
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

#endif
