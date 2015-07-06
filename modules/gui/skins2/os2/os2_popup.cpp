/*****************************************************************************
 * os2_popup.cpp
 *****************************************************************************
 * Copyright (C) 2003, 2013 the VideoLAN team
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          KO Myung-Hun      <komh@chollian.net>
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

#ifdef OS2_SKINS

#include "os2_popup.hpp"
#include "os2_factory.hpp"


OS2Popup::OS2Popup( intf_thread_t *pIntf, HWND hAssociatedWindow )
    : OSPopup( pIntf ), m_hWnd( hAssociatedWindow )
{
    // Create the popup menu
    m_hMenu = WinCreateWindow( m_hWnd,          // parent
                               WC_MENU,         // menu
                               "",              // title
                               0,               // style
                               0, 0,            // x, y
                               0, 0,            // cx, cy
                               m_hWnd,          // owner
                               HWND_TOP,        // z-order
                               1,               // id
                               NULL, NULL );    // ctrl data, pres params

    if( !m_hMenu )
    {
        msg_Err( getIntf(), "CreatePopupMenu failed" );
        return;
    }
}


OS2Popup::~OS2Popup()
{
    if( m_hMenu )
        WinDestroyWindow( m_hMenu );
}


void OS2Popup::show( int xPos, int yPos )
{
    POINTL ptl = { xPos, yPos };
    // Invert Y
    ptl.y = ( WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN ) - 1 ) - ptl.y;
    WinMapWindowPoints( HWND_DESKTOP, m_hWnd, &ptl, 1 );

    WinPopupMenu( m_hWnd, m_hWnd, m_hMenu, ptl.x, ptl.y, 0,
                  PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
                  PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
}


void OS2Popup::hide()
{
    WinSendMsg( m_hMenu, WM_CHAR,
                MPFROM2SHORT( KC_VIRTUALKEY, 0 ),
                MPFROM2SHORT( 0, VK_ESC ));
}


void OS2Popup::addItem( const std::string &rLabel, int pos )
{
    MENUITEM mi;

    mi.iPosition   = findInsertionPoint( pos );
    mi.afStyle     = MIS_TEXT;
    mi.afAttribute = 0;
    mi.id          = pos;
    mi.hwndSubMenu = NULLHANDLE;
    mi.hItem       = NULLHANDLE;

    WinSendMsg( m_hMenu, MM_INSERTITEM, MPFROMP( &mi ),
                MPFROMP( rLabel.c_str()));
}


void OS2Popup::addSeparator( int pos )
{
    MENUITEM mi;

    mi.iPosition   = findInsertionPoint( pos );
    mi.afStyle     = MIS_SEPARATOR;
    mi.afAttribute = 0;
    mi.id          = 0;
    mi.hwndSubMenu = NULLHANDLE;
    mi.hItem       = NULLHANDLE;

    WinSendMsg( m_hMenu, MM_INSERTITEM, MPFROMP( &mi ), 0 );
}


unsigned int OS2Popup::findInsertionPoint( unsigned int pos ) const
{
    int nCount = LONGFROMMR( WinSendMsg( m_hMenu, MM_QUERYITEMCOUNT, 0, 0 ));

    // For this simple algorithm, we rely on the fact that in the final state
    // of the menu, the ID of each item is equal to its position in the menu
    int i = 0;
    while( i < nCount &&
           SHORT1FROMMR( WinSendMsg( m_hMenu, MM_ITEMIDFROMPOSITION,
                                     MPFROMLONG( i ), 0 )) < pos )
    {
        i++;
    }
    return i;
}


#endif
