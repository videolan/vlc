/*****************************************************************************
 * prefs_dialog.hpp : Preferences
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 ****************************************************************************/

#ifndef _PREFS_DIALOG_H_
#define _PREFS_DIALOG_H_

#include "util/qvlcframe.hpp"

class PrefsTree;
class PrefsPanel;
class QTreeWidgetItem;
class QTreeWidget;
class QHBoxLayout;
class QVBoxLayout;
class QGroupBox;
class QRadioButton;
class QWidget;
class QCheckBox;

class PrefsDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static PrefsDialog * getInstance( intf_thread_t *_p_intf )
    {
        fprintf( stderr, "%p\n", _p_intf );
        if( !instance )
        {
            instance = new PrefsDialog( _p_intf );
            instance->init();
        }
        return instance;
    }
    virtual ~PrefsDialog();
private:
    PrefsDialog( intf_thread_t * );
    void init();

    PrefsPanel *advanced_panel;
    PrefsTree *advanced_tree;
    QWidget *simple_panel;
    QTreeWidget *simple_tree;
    QGroupBox *types;
    // The layout for the main part
    QHBoxLayout *layout;
    // Layout for the left pane
    QVBoxLayout *vertical;
    QRadioButton *small,*all;
    QCheckBox *adv_chk;

    static PrefsDialog *instance;
private slots:
     void changePanel( QTreeWidgetItem *);
     void setAll();
     void setSmall();
     void setAdvanced( bool );
};

#endif
