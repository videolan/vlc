/*****************************************************************************
 * main_interface_win32.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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


#include "main_interface.hpp"

#include "input_manager.hpp"
#include "actions_manager.hpp"
#include "dialogs_provider.hpp"

#include <QBitmap>
#include <vlc_windows_interfaces.h>

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

void MainInterface::createTaskBarButtons()
{
    /*Here is the code for the taskbar thumb buttons
    FIXME:We need pretty buttons in 16x16 px that are handled correctly by masks in Qt
    FIXME:the play button's picture doesn't changed to pause when clicked
    */

    CoInitializeEx( NULL, COINIT_MULTITHREADED );

    if( S_OK == CoCreateInstance( CLSID_TaskbarList,
                NULL, CLSCTX_INPROC_SERVER,
                IID_ITaskbarList3,
                (void **)&p_taskbl) )
    {
        p_taskbl->HrInit();

        if( (himl = ImageList_Create( 16, //cx
                        16, //cy
                        ILC_COLOR32,//flags
                        4,//initial nb of images
                        0//nb of images that can be added
                        ) ) != NULL )
        {
            QPixmap img   = QPixmap(":/win7/prev");
            QPixmap img2  = QPixmap(":/win7/pause");
            QPixmap img3  = QPixmap(":/win7/play");
            QPixmap img4  = QPixmap(":/win7/next");
            QBitmap mask  = img.createMaskFromColor(Qt::transparent);
            QBitmap mask2 = img2.createMaskFromColor(Qt::transparent);
            QBitmap mask3 = img3.createMaskFromColor(Qt::transparent);
            QBitmap mask4 = img4.createMaskFromColor(Qt::transparent);

            if(-1 == ImageList_Add(himl, img.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask.toWinHBITMAP()))
                msg_Err( p_intf, "First ImageList_Add failed" );
            if(-1 == ImageList_Add(himl, img2.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask2.toWinHBITMAP()))
                msg_Err( p_intf, "Second ImageList_Add failed" );
            if(-1 == ImageList_Add(himl, img3.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask3.toWinHBITMAP()))
                msg_Err( p_intf, "Third ImageList_Add failed" );
            if(-1 == ImageList_Add(himl, img4.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask4.toWinHBITMAP()))
                msg_Err( p_intf, "Fourth ImageList_Add failed" );
        }

        // Define an array of two buttons. These buttons provide images through an
        // image list and also provide tooltips.
        THUMBBUTTONMASK dwMask = THUMBBUTTONMASK(THB_BITMAP | THB_FLAGS);

        THUMBBUTTON thbButtons[3];
        thbButtons[0].dwMask = dwMask;
        thbButtons[0].iId = 0;
        thbButtons[0].iBitmap = 0;
        thbButtons[0].dwFlags = THBF_HIDDEN;

        thbButtons[1].dwMask = dwMask;
        thbButtons[1].iId = 1;
        thbButtons[1].iBitmap = 2;
        thbButtons[1].dwFlags = THBF_HIDDEN;

        thbButtons[2].dwMask = dwMask;
        thbButtons[2].iId = 2;
        thbButtons[2].iBitmap = 3;
        thbButtons[2].dwFlags = THBF_HIDDEN;

        HRESULT hr = p_taskbl->ThumbBarSetImageList(winId(), himl );
        if(S_OK != hr)
            msg_Err( p_intf, "ThumbBarSetImageList failed with error %08lx", hr );
        else
        {
            hr = p_taskbl->ThumbBarAddButtons(winId(), 3, thbButtons);
            if(S_OK != hr)
                msg_Err( p_intf, "ThumbBarAddButtons failed with error %08lx", hr );
        }
        CONNECT( THEMIM->getIM(), playingStatusChanged( int ), this, changeThumbbarButtons( int ) );
    }
    else
    {
        himl = NULL;
        p_taskbl = NULL;
    }

}

bool MainInterface::winEvent ( MSG * msg, long * result )
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

void MainInterface::changeThumbbarButtons( int i_status )
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

    //play/pause
    thbButtons[1].dwMask = dwMask;
    thbButtons[1].iId = 1;

    //next
    thbButtons[2].dwMask = dwMask;
    thbButtons[2].iId = 2;
    thbButtons[2].iBitmap = 3;

    switch( i_status )
    {
        case OPENING_S:
        case PLAYING_S:
            {
                thbButtons[0].dwFlags = THBF_ENABLED;
                thbButtons[1].dwFlags = THBF_ENABLED;
                thbButtons[2].dwFlags = THBF_ENABLED;
                thbButtons[1].iBitmap = 1;
                break;
            }
        case END_S:
        case PAUSE_S:
        case ERROR_S:
            {
                thbButtons[0].dwFlags = THBF_ENABLED;
                thbButtons[1].dwFlags = THBF_ENABLED;
                thbButtons[2].dwFlags = THBF_ENABLED;
                thbButtons[1].iBitmap = 2;
                break;
            }
        default:
            return;
    }
    HRESULT hr =  p_taskbl->ThumbBarUpdateButtons(this->winId(), 3, thbButtons);
    if(S_OK != hr)
        msg_Err( p_intf, "ThumbBarUpdateButtons failed with error %08lx", hr );
}
