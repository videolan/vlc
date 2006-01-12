/*****************************************************************************
 * wxwidgets.hpp: Common headers for the wxwidges interface
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

#ifndef _WXVLC_WIDGETS_H_
#define _WXVLC_WIDGETS_H_

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

#ifdef _MSC_VER
// turn off 'identifier was truncated to '255' characters in the debug info'
#   pragma warning( disable:4786 )
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <wx/wx.h>
#define SLIDER_MAX_POS 10000

/*
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/dnd.h>
#include <wx/treectrl.h>
#include <wx/gauge.h>
#include <wx/accel.h>
#include <wx/checkbox.h>
#include <wx/wizard.h>
#include <wx/taskbar.h>
#include "vlc_keys.h"
*/
#if (!wxCHECK_VERSION(2,5,0))
typedef long wxTreeItemIdValue;
#endif

DECLARE_LOCAL_EVENT_TYPE( wxEVT_DIALOG, 0 );
DECLARE_LOCAL_EVENT_TYPE( wxEVT_INTF, 1 );

/***************************************************************************
 * I18N macros
 ***************************************************************************/

/* wxU is used to convert ansi/utf8 strings to unicode strings (wchar_t) */
#if defined( ENABLE_NLS )
#if wxUSE_UNICODE
#   define wxU(utf8) wxString(utf8, wxConvUTF8)
#else
#   define wxU(utf8) wxString(wxConvUTF8.cMB2WC(utf8), *wxConvCurrent)
#endif
#else // ENABLE_NLS
#if wxUSE_UNICODE
#   define wxU(ansi) wxString(ansi, wxConvLocal)
#else
#   define wxU(ansi) (ansi)
#endif
#endif

/* wxL2U (locale to unicode) is used to convert ansi strings to unicode
 * strings (wchar_t) */
#define wxL2U(ansi) wxU(ansi)

#if wxUSE_UNICODE
#   define wxFromLocale(wxstring) FromUTF32(wxstring.wc_str())
#   define wxLocaleFree(string) free(string)
#else
#   define wxFromLocale(wxstring) FromLocale(wxstring.mb_str())
#   define wxLocaleFree(string) LocaleFree(string)
#endif


#define WRAPCOUNT 80

#define OPEN_NORMAL 0
#define OPEN_STREAM 1

enum
{
  ID_CONTROLS_TIMER,
  ID_SLIDER_TIMER,
};

namespace wxvlc {
    class WindowSettings;
    class VideoWindow;
};

using namespace wxvlc;

class DialogsProvider;
class PrefsTreeCtrl;
class AutoBuiltPanel;

/*****************************************************************************
 * intf_sys_t: description and status of wxwindows interface
 *****************************************************************************/
struct intf_sys_t
{
    /* the wx parent window */
    wxWindow            *p_wxwindow;
    wxIcon              *p_icon;

    /* window settings */
    WindowSettings      *p_window_settings;

    /* special actions */
    vlc_bool_t          b_playing;
    vlc_bool_t          b_intf_show;                /* interface to be shown */

    /* The input thread */
    input_thread_t *    p_input;

    /* The messages window */
    msg_subscription_t* p_sub;                  /* message bank subscription */

    /* Playlist management */
    int                 i_playing;                 /* playlist selected item */
    unsigned            i_playlist_usage;

    /* Send an event to show a dialog */
    void (*pf_show_dialog) ( intf_thread_t *p_intf, int i_dialog, int i_arg,
                             intf_dialog_args_t *p_arg );

    /* Popup menu */
    wxMenu              *p_popup_menu;

    /* Hotkeys */
    int                 i_first_hotkey_event;
    int                 i_hotkeys;

    /* Embedded vout */
    VideoWindow         *p_video_window;
    wxBoxSizer          *p_video_sizer;
    vlc_bool_t          b_video_autosize;

    /* Aout */
    aout_instance_t     *p_aout;
};



wxArrayString SeparateEntries( wxString );
wxWindow *CreateDialogsProvider( intf_thread_t *p_intf, wxWindow *p_parent );

/*
 * wxWindows keeps dead locking because the timer tries to lock the playlist
 * when it's already locked somewhere else in the very wxWindows interface
 * module. Unless someone implements a "vlc_mutex_trylock", we need that.
 */
inline void LockPlaylist( intf_sys_t *p_sys, playlist_t *p_pl )
{
    if( p_sys->i_playlist_usage++ == 0)
        vlc_mutex_lock( &p_pl->object_lock );
}

inline void UnlockPlaylist( intf_sys_t *p_sys, playlist_t *p_pl )
{
    if( --p_sys->i_playlist_usage == 0)
        vlc_mutex_unlock( &p_pl->object_lock );
}

#endif
