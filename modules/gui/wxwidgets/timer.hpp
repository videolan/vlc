/*****************************************************************************
 * timer.hpp: Timer headers
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include "wxwidgets.hpp"

namespace wxvlc
{
    class InputManager;
    class Interface;

    class Timer: public wxTimer
    {
    public:
        /* Constructor */
        Timer( intf_thread_t *p_intf, Interface *p_main_interface );
        virtual ~Timer();

        virtual void Notify();

    private:
        intf_thread_t *p_intf;
        Interface *p_main_interface;
        vlc_bool_t b_init;
        int i_old_playing_status;
        int i_old_rate;

        InputManager *msm;
    };

}
