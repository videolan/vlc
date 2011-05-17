/*****************************************************************************
 * var_bool.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "var_bool.hpp"


const string VarBool::m_type = "bool";


VarBoolImpl::VarBoolImpl( intf_thread_t *pIntf ):
    VarBool( pIntf ), m_value( false )
{
}


void VarBoolImpl::set( bool value )
{
    if( value != m_value )
    {
        m_value = value;
        notify();
    }
}


VarBoolAndBool::VarBoolAndBool( intf_thread_t *pIntf, VarBool &rVar1,
                                VarBool &rVar2 ):
    VarBool( pIntf ), m_rVar1( rVar1 ), m_rVar2( rVar2 ),
    m_value( rVar1.get() && rVar2.get() )
{
    m_rVar1.addObserver( this );
    m_rVar2.addObserver( this );
}


VarBoolAndBool::~VarBoolAndBool()
{
    m_rVar1.delObserver( this );
    m_rVar2.delObserver( this );
}


void VarBoolAndBool::onUpdate( Subject<VarBool> &rVariable, void *arg )
{
    (void)rVariable; (void)arg;
    if( m_value != ( m_rVar1.get() && m_rVar2.get() ) )
    {
        m_value = ( m_rVar1.get() && m_rVar2.get() );
        notify();
    }
}


VarBoolOrBool::VarBoolOrBool( intf_thread_t *pIntf, VarBool &rVar1,
                              VarBool &rVar2 ):
    VarBool( pIntf ), m_rVar1( rVar1 ), m_rVar2( rVar2 ),
    m_value( rVar1.get() || rVar2.get() )
{
    m_rVar1.addObserver( this );
    m_rVar2.addObserver( this );
}


VarBoolOrBool::~VarBoolOrBool()
{
    m_rVar1.delObserver( this );
    m_rVar2.delObserver( this );
}


void VarBoolOrBool::onUpdate( Subject<VarBool> &rVariable, void *arg )
{
    (void)rVariable; (void)arg;
    if( m_value != ( m_rVar1.get() || m_rVar2.get() ) )
    {
        m_value = ( m_rVar1.get() || m_rVar2.get() );
        notify();
    }
}


VarNotBool::VarNotBool( intf_thread_t *pIntf, VarBool &rVar ):
    VarBool( pIntf ), m_rVar( rVar )
{
    m_rVar.addObserver( this );
}


VarNotBool::~VarNotBool()
{
    m_rVar.delObserver( this );
}


void VarNotBool::onUpdate( Subject<VarBool> &rVariable, void *arg )
{
    (void)rVariable; (void)arg;
    notify();
}


