/*****************************************************************************
 * var_text.cpp
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

#include "var_text.hpp"
#include "../src/vlcproc.hpp"
#include "../src/var_manager.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"


const std::string VarText::m_type = "text";


VarText::VarText( intf_thread_t *pIntf, bool substVars ): Variable( pIntf ),
    m_text( pIntf, "" ), m_lastText( pIntf, "" ), m_substVars( substVars )
{
}


VarText::~VarText()
{
    if( m_substVars )
    {
        // Remove the observers
        delObservers();
    }
}


const UString VarText::get() const
{
    if( !m_substVars )
    {
        // Do not substitute "$X" variables
        return m_text;
    }

    uint32_t pos;
    VlcProc *pVlcProc = VlcProc::instance( getIntf() );

    // Fill a temporary UString object, and replace the escape characters
    // ($H for help, $T for current time, $L for time left, $D for duration,
    // $V for volume)
    UString temp( m_text );

    // $H is processed first, in case the help string contains other variables
    // to replace. And it is replaced only once, in case one of these other
    // variables is $H...
    if( (pos = temp.find( "$H" )) != UString::npos )
    {
        VarManager *pVarManager = VarManager::instance( getIntf() );
        temp.replace( pos, 2, pVarManager->getHelpText().get() );
    }
    while( (pos = temp.find( "$T" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getTimeVar().getAsStringCurrTime().c_str() );
    }
    while( (pos = temp.find( "$t" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getTimeVar().getAsStringCurrTime(true).c_str() );
    }
    while( (pos = temp.find( "$L" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getTimeVar().getAsStringTimeLeft().c_str() );
    }
    while( (pos = temp.find( "$l" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getTimeVar().getAsStringTimeLeft(true).c_str() );
    }
    while( (pos = temp.find( "$D" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getTimeVar().getAsStringDuration().c_str() );
    }
    while( (pos = temp.find( "$d" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getTimeVar().getAsStringDuration(true).c_str() );
    }
    while( (pos = temp.find( "$V" )) != UString::npos )
    {
        temp.replace( pos, 2,
                      pVlcProc->getVolumeVar().getAsStringPercent().c_str() );
    }
    while( (pos = temp.find( "$N" )) != UString::npos )
    {
        temp.replace( pos, 2, pVlcProc->getStreamNameVar().get() );
    }
    while( (pos = temp.find( "$F" )) != UString::npos )
    {
        temp.replace( pos, 2, pVlcProc->getStreamURIVar().get() );
    }
    while( (pos = temp.find( "$B" )) != UString::npos )
    {
        temp.replace( pos, 2, pVlcProc->getStreamBitRateVar().get() );
    }
    while( (pos = temp.find( "$S" )) != UString::npos )
    {
        temp.replace( pos, 2, pVlcProc->getStreamSampleRateVar().get() );
    }
    while( (pos = temp.find( "$R" )) != UString::npos )
    {
        temp.replace( pos, 2, pVlcProc->getSpeedVar().get() );
    }

    return temp;
}


void VarText::set( const UString &rText )
{
    // Avoid an infinite loop
    if( rText == m_text )
    {
        return;
    }

    m_text = rText;

    if( m_substVars )
    {
        // Stop observing other variables
        delObservers();

        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        VarManager *pVarManager = VarManager::instance( getIntf() );

        // Observe needed variables
        if( m_text.find( "$H" ) != UString::npos )
        {
            pVarManager->getHelpText().addObserver( this );
        }
        if( m_text.find( "$T" ) != UString::npos ||
            m_text.find( "$t" ) != UString::npos ||
            m_text.find( "$L" ) != UString::npos ||
            m_text.find( "$l" ) != UString::npos ||
            m_text.find( "$D" ) != UString::npos ||
            m_text.find( "$d" ) != UString::npos )
        {
            pVlcProc->getTimeVar().addObserver( this );
        }
        if( m_text.find( "$V" ) != UString::npos )
        {
            pVlcProc->getVolumeVar().addObserver( this );
        }
        if( m_text.find( "$N" ) != UString::npos )
        {
            pVlcProc->getStreamNameVar().addObserver( this );
        }
        if( m_text.find( "$F" ) != UString::npos )
        {
            pVlcProc->getStreamURIVar().addObserver( this );
        }
        if( m_text.find( "$B" ) != UString::npos )
        {
            pVlcProc->getStreamBitRateVar().addObserver( this );
        }
        if( m_text.find( "$S" ) != UString::npos )
        {
            pVlcProc->getStreamSampleRateVar().addObserver( this );
        }
        if( m_text.find( "$R" ) != UString::npos )
        {
            pVlcProc->getSpeedVar().addObserver( this );
        }
    }

    notify();
}


void VarText::onUpdate( Subject<VarPercent> &rVariable, void *arg )
{
    (void)rVariable; (void)arg;
    UString newText = get();
    // If the text has changed, notify the observers
    if( newText != m_lastText )
    {
        m_lastText = newText;
        notify();
    }
}


void VarText::onUpdate( Subject<VarText> &rVariable, void *arg )
{
    (void)rVariable; (void)arg;
    UString newText = get();
    // If the text has changed, notify the observers
    if( newText != m_lastText )
    {
        m_lastText = newText;
        notify();
    }
}


void VarText::delObservers()
{
    // Stop observing other variables

    VlcProc *pVlcProc = getIntf()->p_sys->p_vlcProc;
    VarManager *pVarManager = getIntf()->p_sys->p_varManager;

    if( pVlcProc )
    {
        pVlcProc->getTimeVar().delObserver( this );
        pVlcProc->getVolumeVar().delObserver( this );
        pVlcProc->getSpeedVar().delObserver( this );
        pVlcProc->getStreamNameVar().delObserver( this );
        pVlcProc->getStreamURIVar().delObserver( this );
        pVlcProc->getStreamBitRateVar().delObserver( this );
        pVlcProc->getStreamSampleRateVar().delObserver( this );
    }

    if( pVarManager )
        pVarManager->getHelpText().delObserver( this );
}

