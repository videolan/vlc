/*****************************************************************************
 * MessagesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: MessagesWindow.cpp,v 1.1 2003/01/25 20:15:41 titer Exp $
 *
 * Authors: Eric Petit <titer@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* BeOS headers */
#include <InterfaceKit.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>

/* BeOS module headers */
#include "VlcWrapper.h"
#include "MessagesWindow.h"

/*****************************************************************************
 * MessagesWindow::MessagesWindow
 *****************************************************************************/
MessagesWindow::MessagesWindow( intf_thread_t * p_intf,
                                BRect frame, const char * name )
	: BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE )
{
	this->p_intf = p_intf;
	p_sub = p_intf->p_sys->p_sub;
	
	BRect rect, rect2;
	
	rect = Bounds();
	rect.right -= B_V_SCROLL_BAR_WIDTH;
	rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
	rect2 = rect;
	rect2.InsetBy( 5, 5 );
	fMessagesView = new BTextView( rect, "messages", rect2,
	                               B_FOLLOW_ALL, B_WILL_DRAW );
	fMessagesView->MakeEditable( false );
	fScrollView = new BScrollView( "scrollview", fMessagesView, B_WILL_DRAW,
	                               B_FOLLOW_ALL, true, true );
	fScrollBar = fScrollView->ScrollBar( B_VERTICAL );
	AddChild( fScrollView );
	
    /* start window thread in hidden state */
    Hide();
    Show();
}

/*****************************************************************************
 * MessagesWindow::~MessagesWindow
 *****************************************************************************/
MessagesWindow::~MessagesWindow()
{
}

/*****************************************************************************
 * MessagesWindow::QuitRequested
 *****************************************************************************/
bool MessagesWindow::QuitRequested()
{
    Hide();
    return false;
}

/*****************************************************************************
 * MessagesWindow::ReallyQuit
 *****************************************************************************/
void MessagesWindow::ReallyQuit()
{
    Hide();
    Quit();
}

/*****************************************************************************
 * MessagesWindow::UpdateMessages
 *****************************************************************************/
void MessagesWindow::UpdateMessages()
{
    int i_start;
    
    vlc_mutex_lock( p_sub->p_lock );
    int i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    if( p_sub->i_start != i_stop )
    {
        for( i_start = p_sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            /* Append all messages to log window */
            /* textctrl->SetDefaultStyle( *dbg_attr );
            (*textctrl) << p_sub->p_msg[i_start].psz_module; */

            /* switch( p_sub->p_msg[i_start].i_type )
            {
            case VLC_MSG_INFO:
                (*textctrl) << ": ";
                textctrl->SetDefaultStyle( *info_attr );
                break;
            case VLC_MSG_ERR:
                (*textctrl) << " error: ";
                textctrl->SetDefaultStyle( *err_attr );
                break;
            case VLC_MSG_WARN:
                (*textctrl) << " warning: ";
                textctrl->SetDefaultStyle( *warn_attr );
                break;
            case VLC_MSG_DBG:
            default:
                (*textctrl) << " debug: ";
                break;
            } */

            /* Add message */
            fMessagesView->LockLooper();
            fMessagesView->Insert( p_sub->p_msg[i_start].psz_msg );
            fMessagesView->Insert( "\n" );
            fMessagesView->UnlockLooper();
            
            /* Scroll at the end */
            fScrollBar->LockLooper();
            float min, max;
            fScrollBar->GetRange( &min, &max );
            fScrollBar->SetValue( max );
            fScrollBar->UnlockLooper();
        }

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }
}
