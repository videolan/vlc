/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#ifndef VLC_QT_MESSAGE_HANDLER_HPP
#define VLC_QT_MESSAGE_HANDLER_HPP

extern "C" {
typedef struct vlc_object_t vlc_object_t ;
}

void setupVlcQtMessageHandler(vlc_object_t* p_intf);
void cleanupVlcQtMessageHandler();

//(un)register qt message handler using RAII
class VlcQtMessageHandlerRegisterer
{
public:
    inline VlcQtMessageHandlerRegisterer(vlc_object_t* p_intf) {
        setupVlcQtMessageHandler(p_intf);
    }

    inline ~VlcQtMessageHandlerRegisterer() {
        cleanupVlcQtMessageHandler();
    }
};

#endif /* VLC_QT_MESSAGE_HANDLER_HPP */
