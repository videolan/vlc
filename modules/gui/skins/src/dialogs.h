/*****************************************************************************
 * dialogs.h: Dialogs class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialogs.h,v 1.8 2003/07/20 10:38:49 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#ifndef VLC_SKIN_DIALOGS
#define VLC_SKIN_DIALOGS

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;

//---------------------------------------------------------------------------
class Dialogs
{
    protected:
        intf_thread_t *p_intf;

    public:
        // Constructor
        Dialogs( intf_thread_t *_p_intf );

        // Destructor
        virtual ~Dialogs();

        void ShowOpen( bool b_play );
        void ShowOpenSkin( bool b_block );
        void ShowMessages();
        void ShowPrefs();
        void ShowFileInfo();
        void ShowPopup();

        vlc_bool_t b_popup_change;

    private:
        /* Dialogs provider module */
        intf_thread_t *p_provider;
        module_t *p_module;
};

//---------------------------------------------------------------------------

#endif
