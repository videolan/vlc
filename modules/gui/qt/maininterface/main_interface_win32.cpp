/*****************************************************************************
 * main_interface_win32.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
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

#include "player/player_controller.hpp"
#include "playlist/playlist_controller.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "widgets/native/interface_widgets.hpp"

#include <QBitmap>

#include <assert.h>

#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

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
    if( w && w->windowHandle() )
        return static_cast<HWND>(QGuiApplication::platformNativeInterface()->
            nativeResourceForWindow("handle", w->windowHandle()));
    else
        return 0;
}

Q_GUI_EXPORT HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &p, int hbitmapFormat = 0);

enum HBitmapFormat
{
    NoAlpha,
    PremultipliedAlpha,
    Alpha
};

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

    int iconX = GetSystemMetrics(SM_CXSMICON);
    int iconY = GetSystemMetrics(SM_CYSMICON);
    himl = ImageList_Create( iconX /*cx*/, iconY /*cy*/, ILC_COLOR32 /*flags*/,
                             4 /*cInitial*/, 0 /*cGrow*/);
    if( himl == NULL )
    {
        p_taskbl->Release();
        p_taskbl = NULL;
        CoUninitialize();
        return;
    }

    QPixmap img   = QPixmap(":/win7/prev.svg").scaled( iconX, iconY );
    QPixmap img2  = QPixmap(":/win7/pause.svg").scaled( iconX, iconY );
    QPixmap img3  = QPixmap(":/win7/play.svg").scaled( iconX, iconY );
    QPixmap img4  = QPixmap(":/win7/next.svg").scaled( iconX, iconY );
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
    thbButtons[0].dwFlags = THEMPL->count() > 1 ? THBF_ENABLED : THBF_HIDDEN;

    thbButtons[1].dwMask = dwMask;
    thbButtons[1].iId = 1;
    thbButtons[1].iBitmap = 2;
    thbButtons[1].dwFlags = THEMPL->count() > 0 ? THBF_ENABLED : THBF_HIDDEN;

    thbButtons[2].dwMask = dwMask;
    thbButtons[2].iId = 2;
    thbButtons[2].iBitmap = 3;
    thbButtons[2].dwFlags = THEMPL->count() > 1 ? THBF_ENABLED : THBF_HIDDEN;

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
    connect( THEMIM, &PlayerController::playingStateChanged,
             this, &MainInterfaceWin32::changeThumbbarButtons);
    connect( THEMPL, &vlc::playlist::PlaylistControllerModel::countChanged,
            this, &MainInterfaceWin32::playlistItemCountChanged );
    if( THEMIM->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING )
        changeThumbbarButtons( THEMIM->getPlayingState() );
}

bool MainInterfaceWin32::nativeEvent(const QByteArray &, void *message, long *result)
{
    return winEvent( static_cast<MSG*>( message ), result );
}

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
                        THEMPL->prev();
                        break;
                    case 1:
                        THEMPL->togglePlayPause();
                        break;
                    case 2:
                        THEMPL->next();
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
                    THEMPL->togglePlayPause();
                    break;
                case APPCOMMAND_MEDIA_PLAY:
                    THEMPL->play();
                    break;
                case APPCOMMAND_MEDIA_PAUSE:
                    THEMPL->pause();
                    break;
                case APPCOMMAND_MEDIA_CHANNEL_DOWN:
                case APPCOMMAND_MEDIA_PREVIOUSTRACK:
                    THEMPL->prev();
                    break;
                case APPCOMMAND_MEDIA_CHANNEL_UP:
                case APPCOMMAND_MEDIA_NEXTTRACK:
                    THEMPL->next();
                    break;
                case APPCOMMAND_MEDIA_STOP:
                    THEMPL->stop();
                    break;
                case APPCOMMAND_MEDIA_RECORD:
                    THEMIM->toggleRecord();
                    break;
                case APPCOMMAND_VOLUME_DOWN:
                    THEMIM->setVolumeDown();
                    break;
                case APPCOMMAND_VOLUME_UP:
                    THEMIM->setVolumeUp();
                    break;
                case APPCOMMAND_VOLUME_MUTE:
                    THEMIM->toggleMuted();
                    break;
                case APPCOMMAND_MEDIA_FAST_FORWARD:
                    THEMIM->faster();
                    break;
                case APPCOMMAND_MEDIA_REWIND:
                    THEMIM->slower();
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
        changeThumbbarButtons( THEMIM->getPlayingState() );
}

void MainInterfaceWin32::toggleUpdateSystrayMenuWhenVisible()
{
    /* check if any visible window is above vlc in the z-order,
     * but ignore the ones always on top
     * and the ones which can't be activated */
    HWND winId;
    QWindow *window = windowHandle();
    winId = static_cast<HWND>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window));

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


void MainInterfaceWin32::resizeEvent(QResizeEvent *event)
{
    MainInterface::resizeEvent(event);

    /*
     * Detects if window placement is not in its normal position (ex: win7 aero snap)
     * This function compares the normal position (non snapped) to the current position.
     * The current position is translated from screen referential to workspace referential
     * to workspace referential
     */
    b_isWindowTiled = false;
    HWND winHwnd = WinId( this );

    WINDOWPLACEMENT windowPlacement;
    windowPlacement.length = sizeof( windowPlacement );
    if ( GetWindowPlacement( winHwnd, &windowPlacement ) == 0 )
        return;

    if ( windowPlacement.showCmd != SW_SHOWNORMAL )
        return;

    HMONITOR monitor = MonitorFromWindow( winHwnd, MONITOR_DEFAULTTONEAREST );

    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof( monitorInfo );
    if ( GetMonitorInfo( monitor, &monitorInfo )  == 0 )
        return;

    RECT windowRect;
    if ( GetWindowRect( winHwnd, &windowRect ) == 0 )
        return;

    OffsetRect( &windowRect,
                monitorInfo.rcMonitor.left - monitorInfo.rcWork.left ,
                monitorInfo.rcMonitor.top - monitorInfo.rcWork.top );

    b_isWindowTiled = ( EqualRect( &windowPlacement.rcNormalPosition, &windowRect ) == 0 );
}

void MainInterfaceWin32::reloadPrefs()
{
    p_intf->p_sys->disable_volume_keys = var_InheritBool( p_intf, "qt-disable-volume-keys" );
    MainInterface::reloadPrefs();
}

void MainInterfaceWin32::playlistItemCountChanged( size_t  )
{
    changeThumbbarButtons( THEMIM->getPlayingState() );
}

void MainInterfaceWin32::changeThumbbarButtons( PlayerController::PlayingState i_status )
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
    thbButtons[0].dwFlags = THEMPL->count() > 1 ? THBF_ENABLED : THBF_HIDDEN;

    //play/pause
    thbButtons[1].dwMask = dwMask;
    thbButtons[1].iId = 1;
    thbButtons[1].dwFlags = THBF_ENABLED;

    //next
    thbButtons[2].dwMask = dwMask;
    thbButtons[2].iId = 2;
    thbButtons[2].iBitmap = 3;
    thbButtons[2].dwFlags = THEMPL->count() > 1 ? THBF_ENABLED : THBF_HIDDEN;

    switch( i_status )
    {
        case PlayerController::PLAYING_STATE_PLAYING:
            {
                thbButtons[1].iBitmap = 1;
                break;
            }
        case PlayerController::PLAYING_STATE_STARTED:
        case PlayerController::PLAYING_STATE_PAUSED:
        case PlayerController::PLAYING_STATE_STOPPING:
        case PlayerController::PLAYING_STATE_STOPPED:
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

    // If a video is playing, let the vout handle the thumbnail.
    //if( !videoWidget || !THEMIM->hasVideoOutput() )
    //{
    //    hr = p_taskbl->SetThumbnailClip(WinId(this), NULL);
    //    if(S_OK != hr)
    //        msg_Err( p_intf, "SetThumbnailClip failed with error %08lx", hr );
    //}
}
