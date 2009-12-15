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

class CmdLabeled : public CmdGeneric
{
private:
    vlc_object_t *m_pObj;
    vlc_value_t   m_newVal;
protected:
    void execute_on( void (VlcProc::*on_label)(vlc_object_t *,vlc_value_t) )
    {
        if( !m_pObj )
            return;

        (VlcProc::instance( getIntf() )->*on_label)( m_pObj, m_newVal );

        vlc_object_release( m_pObj );
        m_pObj =  NULL;
    }
    CmdLabeled( intf_thread_t *pIntf, vlc_object_t *pObj, vlc_value_t newVal )
              : CmdGeneric( pIntf ), m_pObj( pObj ), m_newVal( newVal )
    {
        if( m_pObj )
            vlc_object_hold( m_pObj );
    }
public:
    virtual ~CmdLabeled() {
        if( m_pObj )
            vlc_object_release( m_pObj );
    }
};

#define ADD_COMMAND( label )                                            \
    class Cmd_##label : public CmdLabeled                               \
    {   public:                                                         \
        Cmd_##label( intf_thread_t *I, vlc_object_t *O, vlc_value_t V ) \
                   : CmdLabeled (I, O, V) { }                           \
        virtual string getType() const { return #label; }               \
        virtual void execute() { execute_on( &VlcProc::on_##label ); }  \
    };

ADD_COMMAND( item_current_changed )
ADD_COMMAND( intf_event_changed )
ADD_COMMAND( bit_rate_changed )
ADD_COMMAND( sample_rate_changed )
ADD_COMMAND( can_record_changed )

ADD_COMMAND( random_changed )
ADD_COMMAND( loop_changed )
ADD_COMMAND( repeat_changed )

ADD_COMMAND( volume_changed )

ADD_COMMAND( audio_filter_changed )

ADD_COMMAND( intf_show_changed )

#undef ADD_COMMAND

#endif
