/*****************************************************************************
 * dialogs.cpp : WinCE plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/intf.h>

#include "wince.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

/* Dialogs Provider */
class DialogsProvider: public CBaseWindow
{
public:
    /* Constructor */
    DialogsProvider( intf_thread_t *, CBaseWindow *, HINSTANCE = 0 );
    virtual ~DialogsProvider();

protected:
    virtual LRESULT WndProc( HWND, UINT, WPARAM, LPARAM );

private:

    void OnExit( void );
    void OnIdle( void );
    void OnPlaylist( void );
    void OnMessages( void );
    void OnFileInfo( void );
    void OnPreferences( void );
    void OnPopupMenu( void );

    void OnOpen( int, int );
    void OnOpenFileSimple( int );
    void OnOpenDirectory( int );
    void OnOpenFileGeneric( intf_dialog_args_t * );

    /* GetOpenFileName replacement */
    BOOL (WINAPI *GetOpenFile)(void *);
    HMODULE h_gsgetfile_dll;

public:
    /* Secondary windows */
    OpenDialog          *p_open_dialog;
    Playlist            *p_playlist_dialog;
    Messages            *p_messages_dialog;
    PrefsDialog         *p_prefs_dialog;
    FileInfo            *p_fileinfo_dialog;
};

CBaseWindow *CreateDialogsProvider( intf_thread_t *p_intf,
                                    CBaseWindow *p_parent, HINSTANCE h_inst )
{
    return new DialogsProvider( p_intf, p_parent, h_inst );
}

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
DialogsProvider::DialogsProvider( intf_thread_t *p_intf,
                                  CBaseWindow *p_parent, HINSTANCE h_inst )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
    p_open_dialog = NULL;
    p_playlist_dialog = NULL;
    p_messages_dialog = NULL;
    p_fileinfo_dialog = NULL;
    p_prefs_dialog = NULL;

    /* Create dummy window */
    hWnd = CreateWindow( _T("VLC WinCE"), _T("DialogsProvider"), 0,
                         0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
                         p_parent->GetHandle(), NULL, h_inst, (void *)this );

    GetOpenFile = 0;
    h_gsgetfile_dll = LoadLibrary( _T("gsgetfile") );
    if( h_gsgetfile_dll )
    {
        GetOpenFile = (BOOL (WINAPI *)(void *))
            GetProcAddress( h_gsgetfile_dll, _T("gsGetOpenFileName") );
    }

    if( !GetOpenFile )
        GetOpenFile = (BOOL (WINAPI *)(void *))::GetOpenFileName;
}

DialogsProvider::~DialogsProvider()
{
    /* Clean up */
    if( p_open_dialog )     delete p_open_dialog;
    if( p_playlist_dialog ) delete p_playlist_dialog;
    if( p_messages_dialog ) delete p_messages_dialog;
    if( p_fileinfo_dialog ) delete p_fileinfo_dialog;
    if( p_prefs_dialog )    delete p_prefs_dialog;

    if( h_gsgetfile_dll ) FreeLibrary( h_gsgetfile_dll );
}

LRESULT DialogsProvider::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch( msg )
    {
    case WM_APP + INTF_DIALOG_FILE: OnOpen( FILE_ACCESS, wp ); return TRUE;
    case WM_APP + INTF_DIALOG_NET: OnOpen( NET_ACCESS, wp ); return TRUE;
    case WM_APP + INTF_DIALOG_FILE_SIMPLE: OnOpenFileSimple( wp ); return TRUE;
    case WM_APP + INTF_DIALOG_DIRECTORY: OnOpenDirectory( wp ); return TRUE;
    case WM_APP + INTF_DIALOG_FILE_GENERIC:
        OnOpenFileGeneric( (intf_dialog_args_t*)lp ); return TRUE;
    case WM_APP + INTF_DIALOG_PLAYLIST: OnPlaylist(); return TRUE;
    case WM_APP + INTF_DIALOG_MESSAGES: OnMessages(); return TRUE;
    case WM_APP + INTF_DIALOG_FILEINFO: OnFileInfo(); return TRUE;
    case WM_APP + INTF_DIALOG_PREFS: OnPreferences(); return TRUE;
    case WM_APP + INTF_DIALOG_POPUPMENU: OnPopupMenu(); return TRUE;
    }

    return DefWindowProc( hwnd, msg, wp, lp );
}

void DialogsProvider::OnIdle( void )
{
    /* Update the log window */
    if( p_messages_dialog ) p_messages_dialog->UpdateLog();

    /* Update the playlist */
    if( p_playlist_dialog ) p_playlist_dialog->UpdatePlaylist();

    /* Update the fileinfo windows */
    if( p_fileinfo_dialog ) p_fileinfo_dialog->UpdateFileInfo();
}

void DialogsProvider::OnPopupMenu( void )
{
    POINT point = {0};
    PopupMenu( p_intf, hWnd, point );
}

void DialogsProvider::OnPlaylist( void )
{
#if 1
    Playlist *playlist = new Playlist( p_intf, this, hInst );
    CreateDialogBox( hWnd, playlist );
    delete playlist;
#else
    /* Show/hide the playlist window */
    if( !p_playlist_dialog )
        p_playlist_dialog = new Playlist( p_intf, this, hInst );

    if( p_playlist_dialog )
    {
        p_playlist_dialog->ShowPlaylist( !p_playlist_dialog->IsShown() );
    }
#endif
}

void DialogsProvider::OnMessages( void )
{
    /* Show/hide the log window */
    if( !p_messages_dialog )
        p_messages_dialog = new Messages( p_intf, this, hInst );

    if( p_messages_dialog )
    {
        p_messages_dialog->Show( !p_messages_dialog->IsShown() );
    }
}

void DialogsProvider::OnFileInfo( void )
{
#if 1
    FileInfo *fileinfo = new FileInfo( p_intf, this, hInst );
    CreateDialogBox( hWnd, fileinfo );
    delete fileinfo;
#else
    /* Show/hide the file info window */
    if( !p_fileinfo_dialog )
        p_fileinfo_dialog = new FileInfo( p_intf, this, hInst );

    if( p_fileinfo_dialog )
    {
        p_fileinfo_dialog->Show( !p_fileinfo_dialog->IsShown() );
    }
#endif
}

void DialogsProvider::OnPreferences( void )
{
#if 1
    PrefsDialog *preferences = new PrefsDialog( p_intf, this, hInst );
    CreateDialogBox( hWnd, preferences );
    delete preferences;
#else
    /* Show/hide the open dialog */
    if( !p_prefs_dialog )
        p_prefs_dialog = new PrefsDialog( p_intf, this, hInst );

    if( p_prefs_dialog )
    {
        p_prefs_dialog->Show( !p_prefs_dialog->IsShown() );
    }
#endif
}

void DialogsProvider::OnOpen( int i_access, int i_arg )
{
    /* Show/hide the open dialog */
    if( !p_open_dialog )
        p_open_dialog = new OpenDialog( p_intf, this, hInst, i_access, i_arg );

    if( p_open_dialog )
    {
        p_open_dialog->Show( !p_open_dialog->IsShown() );
    }
}

void DialogsProvider::OnOpenFileGeneric( intf_dialog_args_t *p_arg )
{
    if( p_arg == NULL )
    {
        msg_Dbg( p_intf, "OnOpenFileGeneric() called with NULL arg" );
        return;
    }

    /* Convert the filter string */
    TCHAR *psz_filters = (TCHAR *)
        malloc( (strlen(p_arg->psz_extensions) + 2) * sizeof(TCHAR) );
    _tcscpy( psz_filters, _FROMMB(p_arg->psz_extensions) );

    int i;
    for( i = 0; psz_filters[i]; i++ )
    {
        if( psz_filters[i] == '|' ) psz_filters[i] = 0;
    }
    psz_filters[++i] = 0;

    OPENFILENAME ofn;
    TCHAR szFile[MAX_PATH] = _T("\0");

    memset( &ofn, 0, sizeof(OPENFILENAME) );
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.hInstance = hInst;
    ofn.lpstrFilter = psz_filters;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = (LPTSTR)szFile; 
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL; 
    ofn.nMaxFileTitle = 40;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = _FROMMB(p_arg->psz_title);
    ofn.Flags = 0; 
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0L;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );

    if( p_arg->b_save && GetSaveFileName( &ofn ) )
    {
        p_arg->i_results = 1;
        p_arg->psz_results = (char **)malloc( p_arg->i_results *
                                              sizeof(char *) );
        p_arg->psz_results[0] = strdup( _TOMB(ofn.lpstrFile) );
    }

    if( !p_arg->b_save && GetOpenFile( &ofn ) )
    {
        p_arg->i_results = 1;
        p_arg->psz_results = (char **)malloc( p_arg->i_results *
                                              sizeof(char *) );
        p_arg->psz_results[0] = strdup( _TOMB(ofn.lpstrFile) );
    }

    /* Callback */
    if( p_arg->pf_callback )
    {
        p_arg->pf_callback( p_arg );
    }

    if( p_arg->psz_results )
    {
        for( int i = 0; i < p_arg->i_results; i++ )
        {
            free( p_arg->psz_results[i] );
        }
        free( p_arg->psz_results );
    }
    if( p_arg->psz_title ) free( p_arg->psz_title );
    if( p_arg->psz_extensions ) free( p_arg->psz_extensions );

    free( p_arg );
}

void DialogsProvider::OnOpenFileSimple( int i_arg )
{
    OPENFILENAME ofn;
    TCHAR szFile[MAX_PATH] = _T("\0");
    static TCHAR szFilter[] = _T("All (*.*)\0*.*\0");

    playlist_t *p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    memset( &ofn, 0, sizeof(OPENFILENAME) );
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.hInstance = hInst;
    ofn.lpstrFilter = szFilter;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;     
    ofn.lpstrFile = (LPTSTR)szFile; 
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL; 
    ofn.nMaxFileTitle = 40;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = _T("Quick Open File");
    ofn.Flags = 0; 
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0L;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );

    if( GetOpenFile( &ofn ) )
    {
        char *psz_filename = _TOMB(ofn.lpstrFile);
        playlist_Add( p_playlist, psz_filename, psz_filename,
                      PLAYLIST_APPEND | (i_arg?PLAYLIST_GO:0), PLAYLIST_END );
    }

    vlc_object_release( p_playlist );
}

void DialogsProvider::OnOpenDirectory( int i_arg )
{
    TCHAR psz_result[MAX_PATH];
    LPMALLOC p_malloc = 0;
    LPITEMIDLIST pidl;
    BROWSEINFO bi;
    playlist_t *p_playlist = 0;

#ifdef UNDER_CE
#   define SHGetMalloc MySHGetMalloc
#   define SHBrowseForFolder MySHBrowseForFolder
#   define SHGetPathFromIDList MySHGetPathFromIDList

    HMODULE ceshell_dll = LoadLibrary( _T("ceshell") );
    if( !ceshell_dll ) return;

    HRESULT (WINAPI *SHGetMalloc)(LPMALLOC *) =
        (HRESULT (WINAPI *)(LPMALLOC *))
        GetProcAddress( ceshell_dll, _T("SHGetMalloc") );
    LPITEMIDLIST (WINAPI *SHBrowseForFolder)(LPBROWSEINFO) =
        (LPITEMIDLIST (WINAPI *)(LPBROWSEINFO))
        GetProcAddress( ceshell_dll, _T("SHBrowseForFolder") );
    BOOL (WINAPI *SHGetPathFromIDList)(LPCITEMIDLIST, LPTSTR) =
        (BOOL (WINAPI *)(LPCITEMIDLIST, LPTSTR))
        GetProcAddress( ceshell_dll, _T("SHGetPathFromIDList") );

    if( !SHGetMalloc || !SHBrowseForFolder || !SHGetPathFromIDList )
    {
        msg_Err( p_intf, "couldn't load SHBrowseForFolder API" );
        FreeLibrary( ceshell_dll );
        return;
    }
#endif

    if( !SUCCEEDED( SHGetMalloc(&p_malloc) ) ) goto error;

    p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist ) goto error;

    memset( &bi, 0, sizeof(BROWSEINFO) );
    bi.hwndOwner = hWnd;
    bi.pszDisplayName = psz_result;
    bi.ulFlags = BIF_EDITBOX;
#ifndef UNDER_CE
    bi.ulFlags |= BIF_USENEWUI;
#endif

    if( (pidl = SHBrowseForFolder( &bi ) ) )
    {
        if( SHGetPathFromIDList( pidl, psz_result ) )
        {
            char *psz_filename = _TOMB(psz_result);
            playlist_Add( p_playlist, psz_filename, psz_filename,
                          PLAYLIST_APPEND | (i_arg ? PLAYLIST_GO : 0),
                          PLAYLIST_END );
        }
        p_malloc->Free( pidl );
    }

 error:

    if( p_malloc) p_malloc->Release();
    if( p_playlist ) vlc_object_release( p_playlist );

#ifdef UNDER_CE
    FreeLibrary( ceshell_dll );
#endif
}
