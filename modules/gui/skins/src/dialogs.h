/*****************************************************************************
 * dialogs.h: Dialogs class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialogs.h,v 1.2 2003/06/04 16:03:33 gbazin Exp $
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

#ifndef BASIC_SKINS

#ifdef WIN32                                               /* mingw32 hack */
#   undef Yield
#   undef CreateDialog
#endif
/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO
#include <wx/wx.h>

class OpenDialog;
class Messages;
class SoutDialog;
class PrefsDialog;
class FileInfo;
class wxIcon;

typedef struct dialogs_thread_t
{
    VLC_COMMON_MEMBERS
    intf_thread_t * p_intf;

} dialogs_thread_t;

#endif

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
        void ShowOpenSkin();
        void ShowMessages();
        void ShowPrefs();
        void ShowFileInfo();

        vlc_bool_t b_popup_change;

#ifndef BASIC_SKINS
        // Dialogs
        OpenDialog  *OpenDlg;
        Messages    *MessagesDlg;
        PrefsDialog *PrefsDlg;
        FileInfo    *FileInfoDlg;

        dialogs_thread_t *p_thread;

        void OnShowOpen( wxCommandEvent& event );
        void OnShowOpenSkin( wxCommandEvent& event );
        void OnShowMessages( wxCommandEvent& event );
        void OnShowPrefs( wxCommandEvent& event );
        void OnShowFileInfo( wxCommandEvent& event );
        void OnExitThread( wxCommandEvent& event );
#endif
};
//---------------------------------------------------------------------------

#endif
