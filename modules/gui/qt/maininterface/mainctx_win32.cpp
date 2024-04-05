/*****************************************************************************
 * mainctx_win32.cpp : Main interface
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

#include "mainctx_win32.hpp"

#include "maininterface/compositor.hpp"
#include "player/player_controller.hpp"
#include "playlist/playlist_controller.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "widgets/native/interface_widgets.hpp"
#include "util/csdbuttonmodel.hpp"

#include <QBitmap>
#include <QTimer>

#include <cassert>

#include <QWindow>

#ifndef QT_GUI_PRIVATE
#warning "qplatformnativeinterface.h header is required for MainCtxWin32"
#endif
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <dwmapi.h>

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


// XXX: Cygwin (at least) doesn't define these macros. Too bad...
#ifndef GET_X_LPARAM
    #define GET_X_LPARAM(a) ((int16_t)(a))
    #define GET_Y_LPARAM(a) ((int16_t)((a)>>16))
#endif

using namespace vlc::playlist;

#ifndef WM_NCUAHDRAWCAPTION
// Not documented, only available since Windows Vista
#define WM_NCUAHDRAWCAPTION 0x00AE
#endif

#ifndef WM_NCUAHDRAWFRAME
// Not documented, only available since Windows Vista
#define WM_NCUAHDRAWFRAME 0x00AF
#endif

namespace  {

HWND WinId( QWindow *windowHandle )
{
    if( windowHandle && windowHandle->handle() )
        return static_cast<HWND>(QGuiApplication::platformNativeInterface()->
            nativeResourceForWindow("handle", windowHandle));
    else
        return 0;
}


bool isWindowFixedSize(const QWindow *window)
{
    if (window->flags() & Qt::MSWindowsFixedSizeDialogHint)
        return true;

    const auto minSize = window->minimumSize();
    const auto maxSize = window->maximumSize();

    return minSize.isValid() && maxSize.isValid() && minSize == maxSize;
}


class WinSystemMenuButton : public SystemMenuButton
{
public:
    WinSystemMenuButton(QWindow *window, QObject *parent)
        : SystemMenuButton {parent}
        , m_window {window}
    {
        connect(this, &CSDButton::clicked, this, &WinSystemMenuButton::handleClick);
        connect(this, &CSDButton::doubleClicked, this, &WinSystemMenuButton::handleDoubleClick);
    }

    void showSystemMenu(const QPoint &windowpos) override
    {
        HWND hwnd = (HWND)m_window->winId();
        HMENU hmenu = ::GetSystemMenu(hwnd, FALSE);
        if (!hmenu)
            return;

        // Tweak the menu items according to the current window status.
        const auto winState = m_window->windowStates();
        const bool maxOrFull = (winState.testFlag(Qt::WindowMaximized) || winState.testFlag(Qt::WindowFullScreen));
        const bool fixedSize = isWindowFixedSize(m_window);

        EnableMenuItem(hmenu, SC_MOVE, (MF_BYCOMMAND | (!maxOrFull ? MFS_ENABLED : MFS_DISABLED)));
        EnableMenuItem(hmenu, SC_SIZE, (MF_BYCOMMAND | ((!maxOrFull && !fixedSize) ? MFS_ENABLED : MFS_DISABLED)));

        EnableMenuItem(hmenu, SC_RESTORE, (MF_BYCOMMAND | ((maxOrFull && !fixedSize) ? MFS_ENABLED : MFS_DISABLED)));
        EnableMenuItem(hmenu, SC_MINIMIZE, (MF_BYCOMMAND | MFS_ENABLED));
        EnableMenuItem(hmenu, SC_MAXIMIZE, (MF_BYCOMMAND | ((!maxOrFull && !fixedSize) ? MFS_ENABLED : MFS_DISABLED)));
        EnableMenuItem(hmenu, SC_CLOSE, (MF_BYCOMMAND | MFS_ENABLED));

        // map pos to screen points and convert according to device DPR, required on HI-DPI displays
        const auto screenPoints = m_window->mapToGlobal(windowpos) * m_window->devicePixelRatio();

        const auto alignment = (QGuiApplication::isRightToLeft() ? TPM_RIGHTALIGN : TPM_LEFTALIGN);

        // show menu
        emit systemMenuVisibilityChanged(true);

        const int action = TrackPopupMenu(hmenu, (TPM_RETURNCMD | alignment)
                                          , screenPoints.x(), screenPoints.y()
                                          , NULL, hwnd, 0);

        // unlike native system menu which sends WM_SYSCOMMAND, TrackPopupMenu sends WM_COMMAND
        // imitate native system menu by sending the action manually as WM_SYSCOMMAND
        PostMessageW(hwnd, WM_SYSCOMMAND, action, 0);

        emit systemMenuVisibilityChanged(false);
    }

private:
    // target window
    QWindow *m_window = {};

    // used to reject click() incase a doubleClick() is followed
    bool m_triggerSystemMenu = false;

    void handleClick()
    {
        // delay the show of sytem menu to check if this 'click' is
        // a double click, 'm_triggerSystemMenu' is used to reject the
        // queued 'showSystemMenu' call in case this is a double click

        m_triggerSystemMenu = true;
        QTimer::singleShot(100, this, [this]()
        {
            if (!m_triggerSystemMenu)
                return;

            // show system menu 'margin' below the rect
            constexpr QPoint margin {0, 4};
            showSystemMenu(rect().bottomLeft() + margin);
        });
    }

    void handleDoubleClick()
    {
        // reject any queued showSystemMenu call
        m_triggerSystemMenu = false;

        m_window->close();
    }
};

class CSDWin32EventHandler : public QObject, public QAbstractNativeEventFilter
{
public:
    CSDWin32EventHandler(MainCtx* mainctx, QWindow *window, QObject *parent)
        : QObject {parent}
        , m_useClientSideDecoration {mainctx->useClientSideDecoration()}
        , m_window {window}
        , m_buttonmodel {mainctx->csdButtonModel()}
    {
        QApplication::instance()->installNativeEventFilter(this);
        updateCSDSettings();
    }

    static int resizeBorderWidth(QWindow *window)
    {
        const int result = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        if (result > 0)
            return qRound(static_cast<qreal>(result) / window->devicePixelRatio());
        else
            return qRound(static_cast<qreal>(8) * window->devicePixelRatio());
    }

    static int resizeBorderHeight(QWindow *window)
    {
        const int result = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        if (result > 0)
            return qRound(static_cast<qreal>(result) / window->devicePixelRatio());
        else
            return qRound(static_cast<qreal>(8) * window->devicePixelRatio());
    }

    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override
    {
        MSG* msg = static_cast<MSG*>( message );

        if ( !m_useClientSideDecoration || (msg->hwnd != WinId(m_window)) )
            return false;

        switch ( msg->message )
        {
        case WM_SHOWWINDOW:
        {
            // on window show, update client frame in case DWM settings were updated
            if (msg->wParam) updateClientFrame();
            return false;
        }

        case WM_NCCALCSIZE:
        {
            /* This is used to remove the decoration instead of using FramelessWindowHint because
             * frameless window don't support areo snapping
             */

            if (!msg->wParam)
            {
                *result = 0;
                return true;
            }


            bool nonClientAreaExists = false;
            const auto clientRect = &(reinterpret_cast<LPNCCALCSIZE_PARAMS>(msg->lParam)->rgrc[0]);
            // We don't need this correction when we're fullscreen. We will
            // have the WS_POPUP size, so we don't have to worry about
            // borders, and the default frame will be fine.
            if (IsZoomed(msg->hwnd) && (m_window->windowState() != Qt::WindowFullScreen))
            {
                // Windows automatically adds a standard width border to all
                // sides when a window is maximized. We have to remove it
                // otherwise the content of our window will be cut-off from
                // the screen.
                // The value of border width and border height should be
                // identical in most cases, when the scale factor is 1.0, it
                // should be eight pixels.
                const int rbh = resizeBorderHeight(m_window);
                clientRect->top += rbh;
                clientRect->bottom -= rbh;

                const int rbw = resizeBorderWidth(m_window);
                clientRect->left += rbw;
                clientRect->right -= rbw;
                nonClientAreaExists = true;
            }

            *result = nonClientAreaExists ? 0 : WVR_REDRAW;
            return true;
        }

        case WM_NCUAHDRAWCAPTION:
        case WM_NCUAHDRAWFRAME:
        {
            // These undocumented messages are sent to draw themed window
            // borders. Block them to prevent drawing borders over the client
            // area.

            *result = 0;
            return true;
        }

        case WM_NCHITTEST:
        {
            // send to determine on what part of UI is mouse ON
            // handle it to relay if mouse is on the CSD buttons
            // required for snap layouts menu (WINDOWS 11)

            setAllUnhovered();

            // Get the point in screen coordinates.
            POINT point = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };

            // Map the point to client coordinates.
            ::MapWindowPoints(nullptr, msg->hwnd, &point, 1);

            // excluse resize handle area
            if ((m_window->windowState() != Qt::WindowFullScreen)
                && (point.y < resizeBorderHeight(m_window)
                    || point.x > (m_window->width() * m_window->devicePixelRatio() - resizeBorderWidth(m_window))))
                return false;

            const double scaleFactor = m_window->devicePixelRatio();

            //divide by scale factor as buttons coordinates will be in dpr
            const QPoint qtPoint {static_cast<int>(point.x / scaleFactor), static_cast<int>(point.y / scaleFactor)};
            auto button = overlappingButton(qtPoint);
            if (!button)
                return false;

            switch (button->type())
            {
            case CSDButton::Close:
                *result = HTCLOSE;
                return true;
            case CSDButton::Minimize:
                *result = HTMINBUTTON;
                return true;
            case CSDButton::MaximizeRestore:
                *result = HTMAXBUTTON;
                return true;
            default:
                vlc_assert_unreachable();
                return false;
            }

            break;
        }

        case WM_NCMOUSEMOVE:
        {
            // when we handle WM_NCHITTEST, that makes the OS to capture the mouse events
            // and WM_NCMOUSEMOVE is sent in this case, manually handle them here and relay
            // to UI to draw correct button state
            switch ( msg->wParam )
            {
            case HTCLOSE:
                setHovered(CSDButton::Close);
                break;
            case HTMINBUTTON:
                setHovered(CSDButton::Minimize);
                break;
            case HTMAXBUTTON:
                setHovered(CSDButton::MaximizeRestore);
                break;
            }

            // If we haven't previously asked for mouse tracking, request mouse
            // tracking. We need to do this so we can get the WM_NCMOUSELEAVE
            // message when the mouse leave the titlebar. Otherwise, we won't always
            // get that message (especially if the user moves the mouse _real
            // fast_).
            const bool onSystemButton = (msg->wParam == HTCLOSE || msg->wParam == HTMINBUTTON || msg->wParam == HTMAXBUTTON);
            if (!m_trackingMouse && onSystemButton)
            {
                TRACKMOUSEEVENT ev{};
                ev.cbSize = sizeof(TRACKMOUSEEVENT);
                // TME_NONCLIENT is absolutely critical here. In my experimentation,
                // we'd get WM_MOUSELEAVE messages after just a HOVER_DEFAULT
                // timeout even though we're not requesting TME_HOVER, which kinda
                // ruined the whole point of this.
                ev.dwFlags = TME_LEAVE | TME_NONCLIENT;
                ev.hwndTrack = msg->hwnd;
                ev.dwHoverTime = HOVER_DEFAULT; // we don't _really_ care about this.
                TrackMouseEvent(&ev); // TODO check return?
                m_trackingMouse = true;
            }

            break;
        }

        case WM_NCLBUTTONDOWN:
        {

            // manually trigger button here, UI will never get click
            // signal because we have captured the mouse in non client area
            switch ( msg->wParam )
            {
            case HTCLOSE:
                trigger(CSDButton::Close);
                break;
            case HTMINBUTTON:
                trigger(CSDButton::Minimize);
                break;
            case HTMAXBUTTON:
                trigger(CSDButton::MaximizeRestore);
                break;
            }


            // required for win7 compositor, otherwise this
            // paints default min/max/close buttons
            return true;
        }

        case WM_NCMOUSELEAVE:
        case WM_MOUSELEAVE:
        {
            m_trackingMouse = false;

            // release all buttons we may have captured
            setAllUnhovered();

            break;
        }
        }

        return false;
    }

    void setUseClientSideDecoration(bool useClientSideDecoration)
    {
        m_useClientSideDecoration = useClientSideDecoration;

        updateCSDSettings();
    }

private:
    void updateClientFrame()
    {
        auto hwnd = (HWND)m_window->winId();

        MARGINS margin {};

        // set top client area to 1 pixel tall in case of user decorations
        // with winapi magic, this add rounded corners and shadows to the window
        //
        // warning1: if you set margin.cyTopHeight to 1
        // that somehow breaks snaplayouts menu with WS_CAPTION style ^_____^
        //
        // warning2: if you set negative margin, the window will start painting
        // default CSD button underneath the qml layer, you won't be able to see
        // it but those will capture all your CSD events.
        margin.cxLeftWidth = (m_useClientSideDecoration ? 1 : 0);

        DwmExtendFrameIntoClientArea(hwnd, &margin);
    }

    void updateCSDSettings()
    {
        HWND winId = WinId(m_window);
        if ( !winId )
            return;

        updateClientFrame();

        // trigger window update, this applies changes done in updateClientFrame
        SetWindowPos(winId, NULL, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS |
            SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION |
            SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOZORDER);
    }

    CSDButton *overlappingButton(const QPoint point)
    {
        for (auto button : m_buttonmodel->windowCSDButtons())
        {
            if (button->rect().contains(point))
                return button;
        }
        return nullptr;
    }

    void setHovered(CSDButton::ButtonType type)
    {
        for (auto button : m_buttonmodel->windowCSDButtons()) {
            if (button->type() == type) {
                button->setShowHovered(true);
                return ;
            }
        }

        vlc_assert_unreachable();
    }

    void trigger(CSDButton::ButtonType type)
    {
        for (auto button : m_buttonmodel->windowCSDButtons()) {
            if (button->type() == type) {
                button->click();
                return ;
            }
        }

        vlc_assert_unreachable();
    }

    void setAllUnhovered()
    {
        for (auto button : m_buttonmodel->windowCSDButtons())
        {
            button->setShowHovered(false);
        }
    }

    bool m_useClientSideDecoration;
    QWindow *m_window;
    CSDButtonModel *m_buttonmodel;
    bool m_trackingMouse = false;
};

}

WinTaskbarWidget::WinTaskbarWidget(qt_intf_t *_p_intf, QWindow* windowHandle, QObject* parent)
    : QObject(parent)
    , p_intf(_p_intf)
    , m_window(windowHandle)
{
    taskbar_wmsg = RegisterWindowMessage(TEXT("TaskbarButtonCreated"));
    if (taskbar_wmsg == 0)
        msg_Warn( p_intf, "Failed to register TaskbarButtonCreated message" );
    connect(THEMPL, &PlaylistController::countChanged,
            this, &WinTaskbarWidget::playlistItemCountChanged);
    connect(THEMIM, &PlayerController::fullscreenChanged,
            this, &WinTaskbarWidget::onVideoFullscreenChanged);
    msg_Info(p_intf, "WinTaskbarWidget::WinTaskbarWidget");
}

WinTaskbarWidget::~WinTaskbarWidget()
{
    if( himl )
        ImageList_Destroy( himl );
    if(p_taskbl)
        p_taskbl->Release();
    CoUninitialize();
}

Q_GUI_EXPORT HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &p, int hbitmapFormat = 0);

enum HBitmapFormat
{
    NoAlpha,
    PremultipliedAlpha,
    Alpha
};

void WinTaskbarWidget::createTaskBarButtons()
{
    /*Here is the code for the taskbar thumb buttons
    FIXME:We need pretty buttons in 16x16 px that are handled correctly by masks in Qt
    */
    p_taskbl = NULL;
    himl = NULL;

    auto winId = WinId(m_window);
    if (!winId)
        return;

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

    hr = p_taskbl->ThumbBarSetImageList( winId, himl );
    if( FAILED(hr) )
        msg_Err( p_intf, "%s failed with error %08lx", "ThumbBarSetImageList",
                 hr );
    else
    {
        hr = p_taskbl->ThumbBarAddButtons( winId, 3, thbButtons);
        if( FAILED(hr) )
            msg_Err( p_intf, "%s failed with error %08lx",
                     "ThumbBarAddButtons", hr );
    }
    connect( THEMIM, &PlayerController::playingStateChanged,
             this, &WinTaskbarWidget::changeThumbbarButtons);
    connect( THEMPL, &vlc::playlist::PlaylistController::countChanged,
            this, &WinTaskbarWidget::playlistItemCountChanged );
    if( THEMIM->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING )
        changeThumbbarButtons( THEMIM->getPlayingState() );
}

bool WinTaskbarWidget::nativeEventFilter(const QByteArray &, void *message, qintptr* /* result */)
{
    MSG * msg = static_cast<MSG*>( message );
    if (msg->hwnd != WinId(m_window))
        return false;

    if (msg->message == taskbar_wmsg)
    {
        //We received the taskbarbuttoncreated, now we can really create the buttons
        createTaskBarButtons();
    }

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
    }
    return false;
}

void WinTaskbarWidget::playlistItemCountChanged( size_t  )
{
    changeThumbbarButtons( THEMIM->getPlayingState() );
}

void WinTaskbarWidget::onVideoFullscreenChanged( bool fs )
{
    if( !fs )
        changeThumbbarButtons( THEMIM->getPlayingState() );
}

void WinTaskbarWidget::changeThumbbarButtons( PlayerController::PlayingState i_status )
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

    auto winId = WinId(m_window);
    if (!winId)
    {
        msg_Err( p_intf, "ThumbBarUpdateButtons, window handle is null" );
        return;
    }

    HRESULT hr =  p_taskbl->ThumbBarUpdateButtons(winId, 3, thbButtons);

    if(S_OK != hr)
        msg_Err( p_intf, "ThumbBarUpdateButtons failed with error %08lx", hr );

    // If a video is playing, let the vout handle the thumbnail.
    if( !THEMIM->hasVideoOutput() )
    {
        hr = p_taskbl->SetThumbnailClip(winId, NULL);
        if(S_OK != hr)
            msg_Err( p_intf, "SetThumbnailClip failed with error %08lx", hr );
    }
}

// MainInterface

MainCtxWin32::MainCtxWin32(qt_intf_t * _p_intf )
    : MainCtx( _p_intf )
{
    /* Volume keys */
    p_intf->disable_volume_keys = var_InheritBool( _p_intf, "qt-disable-volume-keys" );
}

void MainCtxWin32::reloadPrefs()
{
    p_intf->disable_volume_keys = var_InheritBool( p_intf, "qt-disable-volume-keys" );
    MainCtx::reloadPrefs();
}

// InterfaceWindowHandlerWin32

InterfaceWindowHandlerWin32::InterfaceWindowHandlerWin32(qt_intf_t *_p_intf, MainCtx* mainCtx, QWindow* window, QObject *parent)
    : InterfaceWindowHandler(_p_intf, mainCtx, window, parent)

#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
    , m_CSDWindowEventHandler(new CSDWin32EventHandler(mainCtx, window, window))
#endif

{
    auto systemMenuButton = std::make_shared<WinSystemMenuButton>(mainCtx->intfMainWindow(), nullptr);
    mainCtx->csdButtonModel()->setSystemMenuButton(systemMenuButton);

    QApplication::instance()->installNativeEventFilter(this);
}

InterfaceWindowHandlerWin32::~InterfaceWindowHandlerWin32()
{
    QApplication::instance()->removeNativeEventFilter(this);
}

void InterfaceWindowHandlerWin32::toggleWindowVisibility()
{

    switch ( m_window->visibility() )
    {
    case QWindow::Hidden:
        /* If hidden, show it */
        setInterfaceShown();
        requestActivate();
        break;
    case QWindow::Minimized:
        setInterfaceMinimized();
        requestActivate();
        break;
    default:
        {
            /* check if any visible window is above vlc in the z-order,
             * but ignore the ones always on top
             * and the ones which can't be activated */
            HWND winId = WinId(m_window);
            if ( !winId )
                break;

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
                setInterfaceHiden();
            else
                requestActivate();
        }
        break;
    }

}


bool InterfaceWindowHandlerWin32::eventFilter(QObject* obj, QEvent* ev)
{
    bool ret = InterfaceWindowHandler::eventFilter(obj, ev);
    if (ret)
        return ret;

    if (ev->type() == QEvent::Resize)
    {
        /*
         * Detects if window placement is not in its normal position (ex: win7 aero snap)
         * This function compares the normal position (non snapped) to the current position.
         * The current position is translated from screen referential to workspace referential
         * to workspace referential
         */
        m_isWindowTiled = false;
        HWND winHwnd = WinId( m_window );
        if ( !winHwnd )
            return ret;

        WINDOWPLACEMENT windowPlacement;
        windowPlacement.length = sizeof( windowPlacement );
        if ( GetWindowPlacement( winHwnd, &windowPlacement ) == 0 )
            return ret;

        if ( windowPlacement.showCmd != SW_SHOWNORMAL )
            return ret;

        HMONITOR monitor = MonitorFromWindow( winHwnd, MONITOR_DEFAULTTONEAREST );

        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof( monitorInfo );
        if ( GetMonitorInfo( monitor, &monitorInfo )  == 0 )
            return ret;

        RECT windowRect;
        if ( GetWindowRect( winHwnd, &windowRect ) == 0 )
            return ret;

        OffsetRect( &windowRect,
                    monitorInfo.rcMonitor.left - monitorInfo.rcWork.left ,
                    monitorInfo.rcMonitor.top - monitorInfo.rcWork.top );

        m_isWindowTiled = ( EqualRect( &windowPlacement.rcNormalPosition, &windowRect ) == 0 );
    }

    return ret;
}

    bool InterfaceWindowHandlerWin32::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
    MSG* msg = static_cast<MSG*>( message );

    short cmd;
    switch( msg->message )
    {
        case WM_APPCOMMAND:
            cmd = GET_APPCOMMAND_LPARAM(msg->lParam);

            if( p_intf->disable_volume_keys &&
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



#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
void InterfaceWindowHandlerWin32::updateCSDWindowSettings()
{
    static_cast<CSDWin32EventHandler *>(m_CSDWindowEventHandler)->setUseClientSideDecoration(m_mainCtx->useClientSideDecoration());
}
#endif
