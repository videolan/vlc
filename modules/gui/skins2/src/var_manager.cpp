/*****************************************************************************
 * var_manager.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include "var_manager.hpp"
#include <new>

VarManager::VarManager( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_pTooltipText( NULL ), m_pHelpText( NULL )
{
    m_pTooltipText = new VarText( pIntf );
    m_pHelpText = new VarText( pIntf, false );
}


VarManager::~VarManager()
{
    // Delete the variables in the reverse order they were added
    std::list<std::string>::const_iterator it1;
    for( it1 = m_varList.begin(); it1 != m_varList.end(); ++it1 )
    {
        m_varMap.erase(*it1);
    }

    // Delete the anonymous variables
    while( !m_anonVarList.empty() )
    {
        m_anonVarList.pop_back();
    }


    delete m_pTooltipText;

    // Warning! the help text must be the last variable to be deleted,
    // because VarText destructor references it (FIXME: find a cleaner way?)
    delete m_pHelpText;
}


VarManager *VarManager::instance( intf_thread_t *pIntf )
{
    if( ! pIntf->p_sys->p_varManager )
    {
        VarManager *pVarManager;
        pVarManager = new (std::nothrow) VarManager( pIntf );
        if( pVarManager )
        {
            pIntf->p_sys->p_varManager = pVarManager;
        }
    }
    return pIntf->p_sys->p_varManager;
}


void VarManager::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_varManager;
    pIntf->p_sys->p_varManager = NULL;
}


void VarManager::registerVar( const VariablePtr &rcVar, const std::string &rName )
{
    m_varMap[rName] = rcVar;
    m_varList.push_front( rName );

    m_anonVarList.push_back( rcVar );
}


void VarManager::registerVar( const VariablePtr &rcVar )
{
    m_anonVarList.push_back( rcVar );
}


Variable *VarManager::getVar( const std::string &rName )
{
    if( m_varMap.find( rName ) != m_varMap.end() )
    {
        return m_varMap[rName].get();
    }
    else
    {
        return NULL;
    }
}


Variable *VarManager::getVar( const std::string &rName, const std::string &rType )
{
    if( m_varMap.find( rName ) != m_varMap.end() )
    {
        Variable *pVar = m_varMap[rName].get();
        // Check the variable type
        if( pVar->getType() != rType )
        {
            msg_Warn( getIntf(), "variable %s has incorrect type (%s instead"
                      " of (%s)", rName.c_str(), pVar->getType().c_str(),
                      rType.c_str() );
            return NULL;
        }
        else
        {
            return pVar;
        }
    }
    else
    {
        return NULL;
    }
}


void VarManager::registerConst( const std::string &rName, const std::string &rValue)
{
    m_constMap[rName] = rValue;
}


std::string VarManager::getConst( const std::string &rName )
{
    return m_constMap[rName];
}

