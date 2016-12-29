/*****************************************************************************
 * main_interface_win32.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "main_interface_win32.hpp"

#include "input_manager.hpp"
#include "actions_manager.hpp"
#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"

#include <QBitmap>

#include <assert.h>

#if defined(_WIN32) && HAS_QT5
# include <QWindow>
# include <qpa/qplatformnativeinterface.h>
#endif

#define WM_APPCOMMAND 0x0319

#define APPCOMMAND_VOLUME_MUTE            8
#define APPCOMMAND_VOLUME_DOWN            9
#define APPCOMMAND_VOLUME_UP              10
#define APPCOMMAND_MEDIA_NEXTTRACK        11
#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
#define APPCOMMAND_MEDIA_STOP             13
#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
#define APPCOMMAND_BASS_DOWN              19
#define APPCOMMAND_BASS_BOOST             20
#define APPCOMMAND_BASS_UP                21
#define APPCOMMAND_TREBLE_DOWN            22
#define APPCOMMAND_TREBLE_UP              23
#define APPCOMMAND_MICROPHONE_VOLUME_MUTE 24
#define APPCOMMAND_MICROPHONE_VOLUME_DOWN 25
#define APPCOMMAND_MICROPHONE_VOLUME_UP   26
#define APPCOMMAND_HELP                   27
#define APPCOMMAND_OPEN                   30
#define APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE    43
#define APPCOMMAND_MIC_ON_OFF_TOGGLE      44
#define APPCOMMAND_MEDIA_PLAY             46
#define APPCOMMAND_MEDIA_PAUSE            47
#define APPCOMMAND_MEDIA_RECORD           48
#define APPCOMMAND_MEDIA_FAST_FORWARD     49
#define APPCOMMAND_MEDIA_REWIND           50
#define APPCOMMAND_MEDIA_CHANNEL_UP       51
#define APPCOMMAND_MEDIA_CHANNEL_DOWN     52

#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_KEY   0
#define FAPPCOMMAND_OEM   0x1000
#define FAPPCOMMAND_MASK  0xF000

#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define GET_DEVICE_LPARAM(lParam)     ((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))
#define GET_MOUSEORKEY_LPARAM         GET_DEVICE_LPARAM
#define GET_FLAGS_LPARAM(lParam)      (LOWORD(lParam))
#define GET_KEYSTATE_LPARAM(lParam)   GET_FLAGS_LPARAM(lParam)

MainInterfaceWin32::MainInterfaceWin32( intf_thread_t *_p_intf )
    : MainInterface( _p_intf )
    , himl( NULL )
    , p_taskbl( NULL )
{
    /* Volume keys */
    _p_intf->p_sys->disable_volume_keys = var_InheritBool( _p_intf, "qt-disable-volume-keys" );
    taskbar_wmsg = RegisterWindowMessage(TEXT("TaskbarButtonCreated"));
    if (taskbar_wmsg == 0)
        msg_Warn( p_intf, "Failed to register TaskbarButtonCreated message" );
}

MainInterfaceWin32::~MainInterfaceWin32()
{
    if( himl )
        ImageList_Destroy( himl );
    if(p_taskbl)
        p_taskbl->Release();
    CoUninitialize();
}

HWND MainInterfaceWin32::WinId( QWidget *w )
{
#if HAS_QT5
    if( w && w->windowHandle() )
        return static_cast<HWND>(QGuiApplication::platformNativeInterface()->
            nativeResourceForWindow("handle", w->windowHandle()));
    else
        return 0;
#else
    return winId();
#endif
}

#if defined(_WIN32) && !HAS_QT5
static const int PremultipliedAlpha = QPixmap::PremultipliedAlpha;
static HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &p, int hbitmapFormat = 0)
{
    return p.toWinHBITMAP((enum QBitmap::HBitmapFormat)hbitmapFormat);
}
#else
Q_GUI_EXPORT HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &p, int hbitmapFormat = 0);
enum HBitmapFormat
{
    NoAlpha,
    PremultipliedAlpha,
    Alpha
};
#endif

void MainInterfaceWin32::createTaskBarButtons()
{
    /*Here is the code for the taskbar thumb buttons
    FIXME:We need pretty buttons in 16x16 px that are handled correctly by masks in Qt
    */
    p_taskbl = NULL;
    himl = NULL;

    HRESULT hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    if( FAILED(hr) )
        return;

    void *pv;
    hr = CoCreateInstance( CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                           IID_ITaskbarList3, &pv);
    if( FAILED(hr) )
    {
        CoUninitialize();
        return;
    }

    p_taskbl = (ITaskbarList3 *)pv;
    p_taskbl->HrInit();

    himl = ImageList_Create( 16 /*cx*/, 16 /*cy*/, ILC_COLOR32 /*flags*/,
                             4 /*cInitial*/, 0 /*cGrow*/);
    if( himl == NULL )
    {
        p_taskbl->Release();
        p_taskbl = NULL;
        CoUninitialize();
        return;
    }

    QPixmap img   = QPixmap(":/win7/prev");
    QPixmap img2  = QPixmap(":/win7/pause");
    QPixmap img3  = QPixmap(":/win7/play");
    QPixmap img4  = QPixmap(":/win7/next");
    QBitmap mask  = img.createMaskFromColor(Qt::transparent);
    QBitmap mask2 = img2.createMaskFromColor(Qt::transparent);
    QBitmap mask3 = img3.createMaskFromColor(Qt::transparent);
    QBitmap mask4 = img4.createMaskFromColor(Qt::transparent);

    if( -1 == ImageList_Add(himl, qt_pixmapToWinHBITMAP(img, PremultipliedAlpha), qt_pixmapToWinHBITMAP(mask)))
        msg_Err( p_intf, "%s ImageList_Add failed", "First" );
    if( -1 == ImageList_Add(himl, qt_pixmapToWinHBITMAP(img2, PremultipliedAlpha), qt_pixmapToWinHBITMAP(mask2)))
        msg_Err( p_intf, "%s ImageList_Add failed", "Second" );
    if( -1 == ImageList_Add(himl, qt_pixmapToWinHBITMAP(img3, PremultipliedAlpha), qt_pixmapToWinHBITMAP(mask3)))
        msg_Err( p_intf, "%s ImageList_Add failed", "Third" );
    if( -1 == ImageList_Add(himl, qt_pixmapToWinHBITMAP(img4, PremultipliedAlpha), qt_pixmapToWinHBITMAP(mask4)))
        msg_Err( p_intf, "%s ImageList_Add failed", "Fourth" );

    // Define an array of two buttons. These buttons provide images through an
    // image list and also provide tooltips.
    THUMBBUTTONMASK dwMask = THUMBBUTTONMASK(THB_BITMAP | THB_FLAGS);
    THUMBBUTTON thbButtons[3];

    thbButtons[0].dwMask = dwMask;
    thbButtons[0].iId = 0;
    thbButtons[0].iBitmap = 0;
    thbButtons[0].dwFlags = THEPL->items.i_size > 1 ? THBF_ENABLED : THBF_HIDDEN;

    thbButtons[1].dwMask = dwMask;
    thbButtons[1].iId = 1;
    thbButtons[1].iBitmap = 2;
    thbButtons[1].dwFlags = THEPL->items.i_size > 0 ? THBF_ENABLED : THBF_HIDDEN;

    thbButtons[2].dwMask = dwMask;
    thbButtons[2].iId = 2;
    thbButtons[2].iBitmap = 3;
    thbButtons[2].dwFlags = THEPL->items.i_size > 1 ? THBF_ENABLED : THBF_HIDDEN;

    hr = p_taskbl->ThumbBarSetImageList( WinId(this), himl );
    if( FAILED(hr) )
        msg_Err( p_intf, "%s failed with error %08lx", "ThumbBarSetImageList",
                 hr );
    else
    {
        hr = p_taskbl->ThumbBarAddButtons( WinId(this), 3, thbButtons);
        if( FAILED(hr) )
            msg_Err( p_intf, "%s failed with error %08lx",
                     "ThumbBarAddButtons", hr );
    }
    CONNECT( THEMIM->getIM(), playingStatusChanged( int ),
             this, changeThumbbarButtons( int ) );
    CONNECT( THEMIM, playlistItemAppended( int, int ),
            this, playlistItemAppended( int, int ) );
    CONNECT( THEMIM, playlistItemRemoved( int ),
            this, playlistItemRemoved( int ) );
    if( THEMIM->getIM()->playingStatus() == PLAYING_S )
        changeThumbbarButtons( THEMIM->getIM()->playingStatus() );
}

#if HAS_QT5
bool MainInterfaceWin32::nativeEvent(const QByteArray &, void *message, long *result)
{
    return winEvent( static_cast<MSG*>( message ), result );
}
#endif

bool MainInterfaceWin32::winEvent ( MSG * msg, long * result )
{
    if (msg->message == taskbar_wmsg)
    {
        //We received the taskbarbuttoncreated, now we can really create the buttons
        createTaskBarButtons();
    }

    short cmd;
    switch( msg->message )
    {
        case WM_COMMAND:
            if (HIWORD(msg->wParam) == THBN_CLICKED)
            {
                switch(LOWORD(msg->wParam))
                {
                    case 0:
                        THEMIM->prev();
                        break;
                    case 1:
                        THEMIM->togglePlayPause();
                        break;
                    case 2:
                        THEMIM->next();
                        break;
                }
            }
            break;
        case WM_APPCOMMAND:
            cmd = GET_APPCOMMAND_LPARAM(msg->lParam);

            if( p_intf->p_sys->disable_volume_keys &&
                    (   cmd == APPCOMMAND_VOLUME_DOWN   ||
                        cmd == APPCOMMAND_VOLUME_UP     ||
                        cmd == APPCOMMAND_VOLUME_MUTE ) )
            {
                break;
            }

            *result = TRUE;

            switch(cmd)
            {
                case APPCOMMAND_MEDIA_PLAY_PAUSE:
                    THEMIM->togglePlayPause();
                    break;
                case APPCOMMAND_MEDIA_PLAY:
                    THEMIM->play();
                    break;
                case APPCOMMAND_MEDIA_PAUSE:
                    THEMIM->pause();
                    break;
                case APPCOMMAND_MEDIA_CHANNEL_DOWN:
                case APPCOMMAND_MEDIA_PREVIOUSTRACK:
                    THEMIM->prev();
                    break;
                case APPCOMMAND_MEDIA_CHANNEL_UP:
                case APPCOMMAND_MEDIA_NEXTTRACK:
                    THEMIM->next();
                    break;
                case APPCOMMAND_MEDIA_STOP:
                    THEMIM->stop();
                    break;
                case APPCOMMAND_MEDIA_RECORD:
                    THEAM->record();
                    break;
                case APPCOMMAND_VOLUME_DOWN:
                    THEAM->AudioDown();
                    break;
                case APPCOMMAND_VOLUME_UP:
                    THEAM->AudioUp();
                    break;
                case APPCOMMAND_VOLUME_MUTE:
                    THEAM->toggleMuteAudio();
                    break;
                case APPCOMMAND_MEDIA_FAST_FORWARD:
                    THEMIM->getIM()->faster();
                    break;
                case APPCOMMAND_MEDIA_REWIND:
                    THEMIM->getIM()->slower();
                    break;
                case APPCOMMAND_HELP:
                    THEDP->mediaInfoDialog();
                    break;
                case APPCOMMAND_OPEN:
                    THEDP->simpleOpenDialog();
                    break;
                default:
                     msg_Dbg( p_intf, "unknown APPCOMMAND = %d", cmd);
                     *result = FALSE;
                     break;
            }
            if (*result) return true;
            break;
    }
    return false;
}

void MainInterfaceWin32::setVideoFullScreen( bool fs )
{
    MainInterface::setVideoFullScreen( fs );
    if( !fs )
        changeThumbbarButtons( THEMIM->getIM()->playingStatus() );
}

void MainInterfaceWin32::toggleUpdateSystrayMenuWhenVisible()
{
    /* check if any visible window is above vlc in the z-order,
     * but ignore the ones always on top
     * and the ones which can't be activated */
    HWND winId;
#if HAS_QT5
    QWindow *window = windowHandle();
    winId = static_cast<HWND>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window));
#else
    winId = internalWinId();
#endif

    WINDOWINFO wi;
    HWND hwnd;
    wi.cbSize = sizeof( WINDOWINFO );
    for( hwnd = GetNextWindow( winId, GW_HWNDPREV );
            hwnd && ( !IsWindowVisible( hwnd ) || ( GetWindowInfo( hwnd, &wi ) &&
                                                    ( wi.dwExStyle&WS_EX_NOACTIVATE ) ) );
            hwnd = GetNextWindow( hwnd, GW_HWNDPREV ) )
    {
    }
    if( !hwnd || !GetWindowInfo( hwnd, &wi ) || (wi.dwExStyle&WS_EX_TOPMOST) )
        hide();
    else
        activateWindow();
}

void MainInterfaceWin32::reloadPrefs()
{
    p_intf->p_sys->disable_volume_keys = var_InheritBool( p_intf, "qt-disable-volume-keys" );
    MainInterface::reloadPrefs();
}

void MainInterfaceWin32::playlistItemAppended( int, int )
{
    changeThumbbarButtons( THEMIM->getIM()->playingStatus() );
}

void MainInterfaceWin32::playlistItemRemoved( int )
{
    changeThumbbarButtons( THEMIM->getIM()->playingStatus() );
}

void MainInterfaceWin32::changeThumbbarButtons( int i_status )
{
    if( p_taskbl == NULL )
        return;

    // Define an array of three buttons. These buttons provide images through an
    // image list and also provide tooltips.
    THUMBBUTTONMASK dwMask = THUMBBUTTONMASK(THB_BITMAP | THB_FLAGS);

    THUMBBUTTON thbButtons[3];
    //prev
    thbButtons[0].dwMask = dwMask;
    thbButtons[0].iId = 0;
    thbButtons[0].iBitmap = 0;
    thbButtons[0].dwFlags = THEPL->items.i_size > 1 ? THBF_ENABLED : THBF_HIDDEN;

    //play/pause
    thbButtons[1].dwMask = dwMask;
    thbButtons[1].iId = 1;
    thbButtons[1].dwFlags = THBF_ENABLED;

    //next
    thbButtons[2].dwMask = dwMask;
    thbButtons[2].iId = 2;
    thbButtons[2].iBitmap = 3;
    thbButtons[2].dwFlags = THEPL->items.i_size > 1 ? THBF_ENABLED : THBF_HIDDEN;

    switch( i_status )
    {
        case OPENING_S:
        case PLAYING_S:
            {
                thbButtons[1].iBitmap = 1;
                break;
            }
        case END_S:
        case PAUSE_S:
        case ERROR_S:
            {
                thbButtons[1].iBitmap = 2;
                break;
            }
        default:
            return;
    }

    HRESULT hr =  p_taskbl->ThumbBarUpdateButtons(WinId(this), 3, thbButtons);

    if(S_OK != hr)
        msg_Err( p_intf, "ThumbBarUpdateButtons failed with error %08lx", hr );

    if( videoWidget && THEMIM->getIM()->hasVideo() )
    {
        RECT rect;
        GetClientRect(WinId(videoWidget), &rect);
        hr = p_taskbl->SetThumbnailClip(WinId(this), &rect);
    }
    else
        hr = p_taskbl->SetThumbnailClip(WinId(this), NULL);
    if(S_OK != hr)
        msg_Err( p_intf, "SetThumbnailClip failed with error %08lx", hr );
}
