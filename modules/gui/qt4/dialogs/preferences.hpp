/*****************************************************************************
 * preferences.hpp : Preferences
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _PREFS_DIALOG_H_
#define _PREFS_DIALOG_H_

#include "util/qvlcframe.hpp"
#include "components/simple_preferences.hpp"

class PrefsTree;
class SPrefsCatList;
class AdvPrefsPanel;
class SPrefsPanel;
class QTreeWidgetItem;
class QTreeWidget;
class QHBoxLayout;
class QVBoxLayout;
class QGroupBox;
class QRadioButton;
class QWidget;
class QCheckBox;
class QLabel;

class PrefsDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static PrefsDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance )
            instance = new PrefsDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf );
        return instance;
    }
    virtual ~PrefsDialog() {};
#if 0
    /*Called from extended settings, is not used anymore, but could be useful one day*/
    void showModulePrefs( char* );
#endif

private:
    PrefsDialog( QWidget *, intf_thread_t * );
    QGridLayout *main_layout;

    void destroyPanels();

    QWidget *main_panel;
    QHBoxLayout *main_panel_l;

    AdvPrefsPanel *advanced_panel;
    SPrefsPanel *current_simple_panel;
    SPrefsPanel *simple_panels[SPrefsMax];

    QWidget *tree_panel;
    QHBoxLayout *tree_panel_l;

    SPrefsCatList *simple_tree;
    PrefsTree *advanced_tree;

    QGroupBox *types;
    QRadioButton *small,*all;

    static PrefsDialog *instance;

    bool b_small;

private slots:
    void setAdvanced();
    void setSmall();

    void changeAdvPanel( QTreeWidgetItem * );
    void changeSimplePanel( int );

    void save();
    void cancel();
    void reset();
    void close() { save(); };
};

#endif
