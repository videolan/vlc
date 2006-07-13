/*****************************************************************************
 * menus.hpp : Menus handling
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _MENUS_H_
#define _MENUS_H_

#include "qt4.hpp"
#include <QObject>
#include <vector>

using namespace std;

class QMenu;
class QPoint;

class MenuItemData : public QObject
{
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

    /* Individual menu builders */
    static QMenu *FileMenu();

    static void AudioPopupMenu( intf_thread_t *, const QPoint& );
    static void VideoPopupMenu( intf_thread_t *, const QPoint& );

    static QMenu *NavigMenu( intf_thread_t * , QMenu * );
    static QMenu *VideoMenu( intf_thread_t * , QMenu * );
    static QMenu *AudioMenu( intf_thread_t * , QMenu * );

    /* Generic automenu methods */
    static QMenu * Populate( intf_thread_t *, QMenu *current,
                             vector<const char*>&, vector<int>& );

    static void CreateAndConnect( QMenu *, const char *, QString, QString,
                                  int, int, vlc_value_t, int, bool c = false );
    static void CreateItem( QMenu *, const char *, vlc_object_t * );
    static QMenu *CreateChoicesMenu( const char *, vlc_object_t *, bool );

    static void DoAction( intf_thread_t *, QObject * );
};

#endif
