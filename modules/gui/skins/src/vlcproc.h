/*****************************************************************************
 * vlcproc.h: VlcProc class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlcproc.h,v 1.8 2003/06/23 20:35:36 asmax Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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


#ifndef VLC_SKIN_PROC
#define VLC_SKIN_PROC

#include "skin_common.h"

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class VlcProc
{
    private:
        intf_thread_t *p_intf;

        // Vlc methods
        void LoadSkin();
        void DropFile( unsigned int param );
        void PauseStream();
        void PlayStream();
        void StopStream();
        void NextStream();
        void PrevStream();
        void MoveStream( long Pos );
        void FullScreen();
        void ChangeVolume( unsigned int msg, long param );
        void AddNetworkUDP( int port );

        static int RefreshCallback( vlc_object_t *p_this, 
            const char *psz_variable, vlc_value_t old_val, vlc_value_t new_val, 
            void *param );
        void InterfaceRefresh();
        void EnabledEvent( string type, bool state );

    public:
        // Constuctor
        VlcProc( intf_thread_t *_p_intf );

        // Destructor
        ~VlcProc();

        // Event procedures
        bool EventProc( Event *evt );
        bool EventProcEnd();
        bool IsClosing();

        // Getters
        intf_thread_t *GetpIntf() { return p_intf; };
};
//---------------------------------------------------------------------------

#endif
