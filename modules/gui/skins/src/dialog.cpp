/*****************************************************************************
 * dialog.cpp: Classes for some dialog boxes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialog.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "dialog.h"



//---------------------------------------------------------------------------
// Open file dialog box
//---------------------------------------------------------------------------
OpenFileDialog::OpenFileDialog( string title, bool multiselect )
{
    MultiSelect  = multiselect;
    Title        = title;
    Filter       = new char[200];
    FilterLength = 0;
}
//---------------------------------------------------------------------------
OpenFileDialog::~OpenFileDialog()
{
    delete[] Filter;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Log Window
//---------------------------------------------------------------------------
LogWindow::LogWindow( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
    Visible = false;
}
//---------------------------------------------------------------------------
LogWindow::~LogWindow()
{
}
//---------------------------------------------------------------------------
void LogWindow::Update( msg_subscription_t *Sub )
{
    //if( !Visible )
    //    return;

    int i_start, i_stop;
    int i_max_lines;

    vlc_mutex_lock( Sub->p_lock );
    i_stop = *Sub->pi_stop;
    vlc_mutex_unlock( Sub->p_lock );

    if( Sub->i_start != i_stop )
    {
        for( i_start = Sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {


            // Append all messages to log window
            switch( Sub->p_msg[i_start].i_type )
            {
                case VLC_MSG_ERR:
                    ChangeColor( RGB( 255, 0, 0 ), true );
                    break;
                case VLC_MSG_WARN:
                    ChangeColor( RGB( 255, 128, 0 ), true );
                    break;
                default:
                    ChangeColor( RGB( 128, 128, 128 ) );
                    break;
            }

            // Add message
            if( i_max_lines )
            {
                AddLine( (string)Sub->p_msg[i_start].psz_msg );
            }
        }

        vlc_mutex_lock( Sub->p_lock );
        Sub->i_start = i_start;
        vlc_mutex_unlock( Sub->p_lock );
    }
}
//---------------------------------------------------------------------------

