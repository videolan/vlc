/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "qml_main_context.hpp"
#include "maininterface/main_interface.hpp"

QmlMainContext::QmlMainContext(intf_thread_t* intf, MainInterface* mainInterface, QObject* parent)
    : QObject(parent)
    , m_intf( intf )
    , m_playlist(intf->p_sys->p_playlist)
    , m_mainInterface(mainInterface)
{
}

MainInterface*QmlMainContext::getMainInterface() const
{
    return m_mainInterface;
}

intf_thread_t*QmlMainContext::getIntf() const
{
    return m_intf;
}

PlaylistPtr QmlMainContext::getPlaylist() const
{
    return m_playlist;
}
