/*****************************************************************************
 * win32_popup.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifdef WIN32_SKINS

#include "win32_popup.hpp"
#include "win32_factory.hpp"

#ifndef TPM_NOANIMATION
const UINT TPM_NOANIMATION = 0x4000L;
#endif


Win32Popup::Win32Popup( intf_thread_t *pIntf, HWND hAssociatedWindow )
    : OSPopup( pIntf ), m_hWnd( hAssociatedWindow )
{
    // Create the popup menu
    m_hMenu = CreatePopupMenu();

    if( !m_hMenu )
    {
        msg_Err( getIntf(), "CreatePopupMenu failed" );
        return;
    }
}


Win32Popup::~Win32Popup()
{
    if( m_hMenu )
        DestroyMenu( m_hMenu );
}


void Win32Popup::show( int xPos, int yPos )
{
    TrackPopupMenuEx( m_hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON
                               | TPM_HORIZONTAL | TPM_NOANIMATION,
                      xPos, yPos, m_hWnd, NULL );
}


void Win32Popup::hide()
{
    SendMessage( m_hWnd, WM_CANCELMODE, 0, 0 );
}


void Win32Popup::addItem( const string &rLabel, int pos )
{
    MENUITEMINFO menuItem;
    menuItem.cbSize = sizeof( MENUITEMINFO );
//     menuItem.fMask = MIIM_FTYPE | MIIM_ID | MIIM_TYPE | MIIM_STRING;
//     menuItem.fType = MFT_STRING;
    menuItem.fMask = MIIM_ID | MIIM_STRING;
    menuItem.wID = pos;
    menuItem.dwTypeData = ToT(rLabel.c_str());
    menuItem.cch = rLabel.size();

    InsertMenuItem( m_hMenu, findInsertionPoint( pos ), TRUE, &menuItem );
}


void Win32Popup::addSeparator( int pos )
{
    MENUITEMINFO sepItem;
    sepItem.cbSize = sizeof( MENUITEMINFO );
    sepItem.fMask = MIIM_FTYPE;
    sepItem.fType = MFT_SEPARATOR;

    InsertMenuItem( m_hMenu, findInsertionPoint( pos ), TRUE, &sepItem );
}


unsigned int Win32Popup::findInsertionPoint( unsigned int pos ) const
{
    // For this simple algorithm, we rely on the fact that in the final state
    // of the menu, the ID of each item is equal to its position in the menu
    int i = 0;
    while( i < GetMenuItemCount( m_hMenu ) &&
           GetMenuItemID( m_hMenu, i ) < pos )
    {
        i++;
    }
    return i;
}


#endif

