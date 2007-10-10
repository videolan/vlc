/*****************************************************************************
 * menus.hpp : Menus handling
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef _MENUS_H_
#define _MENUS_H_

#include "qt4.hpp"

#include <QObject>
#include <vector>

/* Folder vs. Directory */
#ifdef WIN32
#define I_OPEN_FOLDER "Open &Folder..."
#else
#define I_OPEN_FOLDER "Open D&irectory..."
#endif //WIN32

using namespace std;

class QMenu;
class QMenuBar;
class QSystemTrayIcon;

class MenuItemData : public QObject
{

Q_OBJECT

public:
    MenuItemData( int i_id, int _i_type, vlc_value_t _val, const char *_var )
    {
        i_object_id = i_id;
        i_val_type = _i_type;
        val = _val;
        psz_var = strdup( _var );
    }
    virtual ~MenuItemData()
    {
        if( psz_var ) free( psz_var );
        if( ((i_val_type & VLC_VAR_TYPE) == VLC_VAR_STRING)
            && val.psz_string ) free( val.psz_string );
    }
    int i_object_id;
    int i_val_type;
    vlc_value_t val;
    char *psz_var;
};

class QVLCMenu : public QObject
{
    Q_OBJECT;
public:
    static void createMenuBar( MainInterface *mi, intf_thread_t *,
                               bool );

    /* Menus */
    static QMenu *FileMenu();
    static QMenu *SDMenu( intf_thread_t * );
    static QMenu *PlaylistMenu( intf_thread_t *, MainInterface * );
    static QMenu *ToolsMenu( intf_thread_t *, MainInterface *, bool, bool with = true );
    static QMenu *NavigMenu( intf_thread_t * , QMenu * );
    static QMenu *VideoMenu( intf_thread_t * , QMenu * );
    static QMenu *AudioMenu( intf_thread_t * , QMenu * );
    static QMenu *InterfacesMenu( intf_thread_t *p_intf, QMenu * );
    static QMenu *HelpMenu();

    /* Popups Menus */
    static void AudioPopupMenu( intf_thread_t * );
    static void VideoPopupMenu( intf_thread_t * );
    static void MiscPopupMenu( intf_thread_t * );
    static void PopupMenu( intf_thread_t *, bool );

    /* Systray */
    static void updateSystrayMenu( MainInterface *,intf_thread_t  *,
                                   bool b_force_visible = false);

    /* Actions */
    static void DoAction( intf_thread_t *, QObject * );
private:
    /* Generic automenu methods */
    static QMenu * Populate( intf_thread_t *, QMenu *current,
                             vector<const char*>&, vector<int>&,
                             bool append = false );

    static void CreateAndConnect( QMenu *, const char *, QString, QString,
                                  int, int, vlc_value_t, int, bool c = false );
    static void CreateItem( QMenu *, const char *, vlc_object_t *, bool );
    static int CreateChoicesMenu( QMenu *,const char *, vlc_object_t *, bool );
};

class MenuFunc : public QObject
{
Q_OBJECT

public:
    MenuFunc( QMenu *_menu, int _id ) { menu = _menu; id = _id; };
    void doFunc( intf_thread_t *p_intf)
    {
        switch( id )
        {
        case 1: QVLCMenu::VideoMenu( p_intf, menu ); break;
        case 2: QVLCMenu::AudioMenu( p_intf, menu ); break;
        case 3: QVLCMenu::NavigMenu( p_intf, menu ); break;
        case 4: QVLCMenu::InterfacesMenu( p_intf, menu ); break;
        }
    };
    int id; QMenu *menu;
};

#endif
