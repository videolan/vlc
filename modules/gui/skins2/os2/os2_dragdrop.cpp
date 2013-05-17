/*****************************************************************************
 * os2_dragdrop.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef OS2_SKINS

#include "os2/os2_dragdrop.hpp"
#include "../commands/cmd_add_item.hpp"
#include "../events/evt_dragndrop.hpp"
#include <list>


OS2DragDrop::OS2DragDrop( intf_thread_t *pIntf,
                              bool playOnDrop, GenericWindow* pWin )
    : SkinObject( pIntf ), m_playOnDrop( playOnDrop ), m_pWin( pWin)
{
}


#endif
