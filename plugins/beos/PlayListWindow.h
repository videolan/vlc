/*****************************************************************************
 * PlayListWindow.h: BeOS interface window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PlayListWindow.h,v 1.1 2001/06/15 09:07:10 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Tony Castley <tcastley@mail.powerup.com.au>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
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
class CDMenu;
class PlayListWindow : public BWindow
{
public:
    PlayListWindow( BRect frame, const char *name, playlist_t *p_pl);
    ~PlayListWindow();

    // standard window member
    virtual void    MessageReceived(BMessage *message);
    
private:	
    playlist_t  *p_playlist;
    BListView  *p_listview;
    BFilePanel *file_panel;
};


