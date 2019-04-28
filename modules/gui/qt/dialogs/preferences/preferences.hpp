/*****************************************************************************
 * preferences.hpp : Preferences
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef QVLC_PREFS_DIALOG_H_
#define QVLC_PREFS_DIALOG_H_ 1

#include "widgets/native/qvlcframe.hpp"
#include "dialogs/preferences/simple_preferences.hpp"

class ExpertPrefsTable;
class PrefsTree;
class SPrefsCatList;
class SPrefsPanel;
class QTreeWidgetItem;
class QGroupBox;
class QRadioButton;
class QWidget;
class QCheckBox;
class SearchLineEdit;
class QStackedWidget;
class QSplitter;

class PrefsDialog : public QVLCDialog
{
    Q_OBJECT
public:
    PrefsDialog( QWindow *, qt_intf_t * );
    virtual ~PrefsDialog();

private:
    size_t count;
    module_t **p_list;

    /* View stack */
    QStackedWidget *stack;

    /* View selection */
    enum { SIMPLE, ADVANCED, EXPERT };
    QGroupBox *types;
    QRadioButton *simple, *all, *expert;

    /* Simple view components */
    QWidget *simple_split_widget;
    QWidget *simple_tree_panel;
    SPrefsCatList *simple_tree;
    QStackedWidget *simple_panels_stack;
    SPrefsPanel *simple_panels[SPrefsMax];

    /* Advanced view components */
    QSplitter *advanced_split_widget;
    QWidget *advanced_tree_panel;
    SearchLineEdit *tree_filter;
    QCheckBox *current_filter;
    PrefsTree *advanced_tree;
    QStackedWidget *advanced_panels_stack;

    /* Expert view components */
    QWidget *expert_widget;
    QGridLayout *expert_widget_layout;
    SearchLineEdit *expert_table_filter;
    QCheckBox *expert_table_filter_modified;
    ExpertPrefsTable *expert_table;
    QLabel *expert_text;
    QLabel *expert_longtext;

private slots:
    void setExpert();
    void setAdvanced();
    void setSimple();

    void changeExpertDesc( const QModelIndex &, const QModelIndex & );
    void changeAdvPanel( QTreeWidgetItem * );
    void changeSimplePanel( int );
    void advancedTreeFilterChanged( const QString & );
    void expertTableFilterChanged( const QString & );
    void expertTableFilterModifiedToggled( bool );
    void onlyLoadedToggled();

    void save();
    void cancel();
    void reset();
    void close() { save(); }
};

#endif
