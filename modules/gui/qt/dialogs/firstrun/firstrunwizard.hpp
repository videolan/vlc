/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_FIRSTRUNWIZARD_H
#define VLC_FIRSTRUNWIZARD_H

#include "ui_firstrunwizard.h"
#include "util/singleton.hpp"

#include <QWizard>

class MLFoldersEditor;
class MLFoldersModel;

class FirstRunWizard : public QWizard, public Singleton<FirstRunWizard>
{
    Q_OBJECT
private:
    explicit FirstRunWizard ( qt_intf_t*, QWidget* parent = nullptr );
    enum { WELCOME_PAGE, FOLDER_PAGE, COLOR_SCHEME_PAGE, LAYOUT_PAGE };
    enum { MODERN, CLASSIC };

    void addDefaults();

    int nextId() const;
    void initializePage( int id );
    void reject();

private:
    Ui::firstrun ui;

    MLFoldersEditor *mlFoldersEditor = nullptr;
    MLFoldersModel *mlFoldersModel = nullptr;

    qt_intf_t* p_intf;
    bool mlDefaults = false;

    QButtonGroup* colorSchemeGroup = nullptr;
    QButtonGroup* colorSchemeImages = nullptr;
    QButtonGroup* layoutImages = nullptr;

private slots:
    void finish();
    void MLaddNewFolder();
    void updateColorLabel( QAbstractButton* );
    void updateLayoutLabel (QAbstractButton* );
    void imageColorSchemeClick ( QAbstractButton* );
    void imageLayoutClick( QAbstractButton* );

    friend class Singleton<FirstRunWizard>;
};

#endif // VLC_FIRSTRUNWIZARD_H
