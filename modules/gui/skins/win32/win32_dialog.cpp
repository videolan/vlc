/*****************************************************************************
 * win32_dialog.cpp: Win32 implementation of some dialog boxes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_dialog.cpp,v 1.5 2003/04/20 20:28:39 ipkiss Exp $
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

#ifdef WIN32

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>
extern intf_thread_t *g_pIntf;

//--- WIN32 -----------------------------------------------------------------
#define _WIN32_IE 0x0400    // Yes, i think it's a fucking kludge !
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <richedit.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/banks.h"
#include "../src/dialog.h"
#include "../os_dialog.h"
#include "../src/skin_common.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "../src/event.h"
#include "../os_api.h"


//---------------------------------------------------------------------------
// Open file dialog box
//---------------------------------------------------------------------------
Win32OpenFileDialog::Win32OpenFileDialog( intf_thread_t *_p_intf, string title,
    bool multiselect ) : OpenFileDialog( _p_intf, title, multiselect )
{
}
//---------------------------------------------------------------------------
Win32OpenFileDialog::~Win32OpenFileDialog()
{
}
//---------------------------------------------------------------------------
void Win32OpenFileDialog::AddFilter( string name, string type )
{
    unsigned int i;

    for( i = 0; i < name.length(); i++ )
        Filter[FilterLength++] = name[i];

    Filter[FilterLength++] = ' ';
    Filter[FilterLength++] = '(';

    for( i = 0; i < type.length(); i++ )
        Filter[FilterLength++] = type[i];

    Filter[FilterLength++] = ')';
    Filter[FilterLength++] = '\0';

    for( i = 0; i < type.length(); i++ )
        Filter[FilterLength++] = type[i];

    Filter[FilterLength++] = '\0';

    // Ending null character if this filter is the last
    Filter[FilterLength] = '\0';
}
//---------------------------------------------------------------------------
bool Win32OpenFileDialog::Open()
{
    // Initailize dialog box
    OPENFILENAME OpenFile;
    memset( &OpenFile, 0, sizeof( OpenFile ) );
    OpenFile.lStructSize  = sizeof( OPENFILENAME );
    OpenFile.hwndOwner = NULL;
    OpenFile.lpstrFile = new char[MAX_PATH];
    OpenFile.lpstrFile[0] = '\0';
    OpenFile.nMaxFile = MAX_PATH;
    if( MultiSelect )
    {
        OpenFile.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }
    else
    {
        OpenFile.Flags = OFN_EXPLORER;
    }
    OpenFile.lpstrTitle  = Title.c_str();
    OpenFile.lpstrFilter = Filter;

    // Remove mouse tracking event to avoid non process due to modal open box
    if( p_intf != NULL && p_intf->p_sys->p_theme != NULL )
    {
        TRACKMOUSEEVENT TrackEvent;
        TrackEvent.cbSize      = sizeof( TRACKMOUSEEVENT );
        TrackEvent.dwFlags     = TME_LEAVE|TME_CANCEL;
        TrackEvent.dwHoverTime = 1;

        list<Window *>::const_iterator win;
        for( win = g_pIntf->p_sys->p_theme->WindowList.begin();
            win != g_pIntf->p_sys->p_theme->WindowList.end(); win++ )
        {
            TrackEvent.hwndTrack   = ( (Win32Window *)(*win) )->GetHandle();
            TrackMouseEvent( &TrackEvent );
        }
    }

    // Show dialog box
    if( !GetOpenFileName( &OpenFile ) )
    {
        OSAPI_PostMessage( NULL, WINDOW_LEAVE, 0, 0 );
        return false;
    }

    // Tell windows that mouse cursor has left window because it has been
    // unactivated
    OSAPI_PostMessage( NULL, WINDOW_LEAVE, 0, 0 );

    // Find files in string result
    char * File = OpenFile.lpstrFile;
    int i       = OpenFile.nFileOffset;
    int last    = OpenFile.nFileOffset;
    string path;
    string tmpFile;


    // If only one file has been selected
    if( File[OpenFile.nFileOffset - 1] != '\0' )
    {
        FileList.push_back( (string)File );
    }
    // If multiple files have been selected
    else
    {
        // Add \ if not present at end of path
        if( File[OpenFile.nFileOffset - 2] != '\\' )
        {
            path = (string)File + '\\';
        }
        else
        {
            path = (string)File;
        }

        // Search filenames
        while( true )
        {
            if( File[i] == '\0' )
            {
                if( i == last )
                    break;
                else
                {
                    // Add file
                    FileList.push_back( path + (string)&File[last] );
                    last = i + 1;
                }
            }
            i++;
        }
    }

    // Free memory
    delete[] OpenFile.lpstrFile;

    return true;
}
//---------------------------------------------------------------------------

#endif
