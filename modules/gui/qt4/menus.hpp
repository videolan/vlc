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

#ifndef QVLC_MENUS_H_
#define QVLC_MENUS_H_

#include "qt4.hpp"

#include <QObject>
#include <QMenu>
#include <QVector>

using namespace std;

class QMenuBar;
class QSystemTrayIcon;

class MenuItemData : public QObject
{
    Q_OBJECT

public:
    MenuItemData( QObject* parent, vlc_object_t *_p_obj, int _i_type,
                  vlc_value_t _val, const char *_var ) : QObject( parent )
    {
        p_obj = _p_obj;
        if( p_obj )
            vlc_object_hold( p_obj );
        i_val_type = _i_type;
        val = _val;
        psz_var = strdup( _var );
    }
    virtual ~MenuItemData()
    {
        free( psz_var );
        if( ( i_val_type & VLC_VAR_TYPE) == VLC_VAR_STRING )
            free( val.psz_string );
        if( p_obj )
            vlc_object_release( p_obj );
    }

    vlc_object_t *p_obj;
    vlc_value_t val;
    char *psz_var;

private:
    int i_val_type;
};

class VLCMenuBar : public QObject
{
    Q_OBJECT
    friend class MenuFunc;

public:
    /* Main bar creation */
    static void createMenuBar( MainInterface *mi, intf_thread_t * );

    /* Popups Menus */
    static void PopupMenu( intf_thread_t *, bool );
    static void AudioPopupMenu( intf_thread_t *, bool );
    static void VideoPopupMenu( intf_thread_t *, bool );
    static void MiscPopupMenu( intf_thread_t *, bool );

    /* Systray */
    static void updateSystrayMenu( MainInterface *, intf_thread_t  *,
                                   bool b_force_visible = false);

    /* Actions */
    static void DoAction( QObject * );
    enum actionflag {
        ACTION_NONE = 0x0,
        ACTION_ALWAYS_ENABLED = 0x1,
        ACTION_MANAGED = 0x2, /* managed using EnableStatic(bool)? */
        ACTION_NO_CLEANUP = 0x4,
        ACTION_STATIC = 0x6, /* legacy shortcut */
        ACTION_DELETE_ON_REBUILD = 0x8
    };
    Q_DECLARE_FLAGS(actionflags, actionflag)

private:
    /* All main Menus */
    static QMenu *FileMenu( intf_thread_t *, QWidget *, MainInterface * mi = NULL );

    static QMenu *ToolsMenu( intf_thread_t *, QMenu * );
    static QMenu *ToolsMenu( intf_thread_t * p_intf, QWidget *parent )
        { return ToolsMenu( p_intf, new QMenu( parent ) ); }

    static QMenu *ViewMenu( intf_thread_t *, QMenu *, MainInterface * mi = NULL );

    static QMenu *InterfacesMenu( intf_thread_t *p_intf, QMenu * );
    static void ExtensionsMenu( intf_thread_t *p_intf, QMenu * );

    static QMenu *NavigMenu( intf_thread_t *, QMenu * );
    static QMenu *NavigMenu( intf_thread_t *p_intf, QWidget *parent ) {
        return NavigMenu( p_intf, new QMenu( parent ) );
    }
    static QMenu *RebuildNavigMenu( intf_thread_t *, QMenu *, bool b_keep = false );

    static QMenu *VideoMenu( intf_thread_t *, QMenu * );
    static QMenu *VideoMenu( intf_thread_t *p_intf, QWidget *parent ) {
        return VideoMenu( p_intf, new QMenu( parent ) );
    }
    static QMenu *SubtitleMenu( intf_thread_t *, QMenu *current, bool b_popup = false );
    static QMenu *SubtitleMenu( intf_thread_t *p_intf, QWidget *parent) {
        return SubtitleMenu( p_intf, new QMenu( parent ) );
    }

    static QMenu *AudioMenu( intf_thread_t *, QMenu * );
    static QMenu *AudioMenu( intf_thread_t *p_intf, QWidget *parent ) {
        return AudioMenu( p_intf, new QMenu( parent ) );
    }

    static QMenu *HelpMenu( QWidget * );

    /* Popups Menus */
    static void PopupMenuStaticEntries( QMenu *menu );
    static void PopupMenuPlaylistEntries( QMenu *menu, intf_thread_t *p_intf,
                                          input_thread_t *p_input );
    static void PopupMenuPlaylistControlEntries( QMenu *menu, intf_thread_t *p_intf );
    static void PopupMenuControlEntries( QMenu *menu, intf_thread_t *p_intf, bool b = true );

    /* Generic automenu methods */
    static QMenu * Populate( intf_thread_t *, QMenu *current,
                             QVector<const char*>&, QVector<vlc_object_t *>& );

    static void CreateAndConnect( QMenu *, const char *, const QString&,
                                  const QString&, int, vlc_object_t *,
                                  vlc_value_t, int, bool c = false );
    static void UpdateItem( intf_thread_t *, QMenu *, const char *,
                            vlc_object_t *, bool );
    static int CreateChoicesMenu( QMenu *,const char *, vlc_object_t *, bool );
    static void EnableStaticEntries( QMenu *, bool );

    /* recentMRL menu */
    static QMenu *recentsMenu, *audioDeviceMenu;

    static void updateAudioDevice( intf_thread_t *, audio_output_t *, QMenu* );

public slots:
    static void updateRecents( intf_thread_t * );
};
Q_DECLARE_OPERATORS_FOR_FLAGS(VLCMenuBar::actionflags)

class MenuFunc : public QObject
{
    Q_OBJECT

public:
    MenuFunc( QMenu *_menu, int _id ) : QObject( (QObject *)_menu ),
                                        menu( _menu ), id( _id ){}

    void doFunc( intf_thread_t *p_intf)
    {
        switch( id )
        {
            case 1: VLCMenuBar::AudioMenu( p_intf, menu ); break;
            case 2: VLCMenuBar::VideoMenu( p_intf, menu ); break;
            case 3: VLCMenuBar::RebuildNavigMenu( p_intf, menu ); break;
            case 4: VLCMenuBar::ViewMenu( p_intf, menu ); break;
            case 5: VLCMenuBar::SubtitleMenu( p_intf, menu ); break;
        }
    }
private:
    QMenu *menu;
    int id;
};

#endif
