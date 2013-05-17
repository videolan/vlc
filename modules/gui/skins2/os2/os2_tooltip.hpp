/*****************************************************************************
 * os2_tooltip.hpp
 *****************************************************************************
 * Copyright (C) 2003, 2013 the VideoLAN team
 *
 * Authors: Cyril Deguet      <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef OS2_TOOLTIP_HPP
#define OS2_TOOLTIP_HPP

#include "../src/os_tooltip.hpp"


/// OS2 implementation of OSTooltip
class OS2Tooltip: public OSTooltip
{
public:
    OS2Tooltip( intf_thread_t *pIntf, HMODULE hInst, HWND hParentWindow );

    virtual ~OS2Tooltip();

    /// Show the tooltip
    virtual void show( int left, int top, OSGraphics &rText );

    /// Hide the tooltip
    virtual void hide();

private:
    /// Window ID
    HWND m_hWnd;

    /// Client window ID
    HWND m_hWndClient;
};


#endif
