/*****************************************************************************
 * infopanels.hpp : Panels for the information dialogs
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _INFOPANELS_H_
#define _INFOPANELS_H_

#include <vlc/vlc.h>
#include <vlc_meta.h>

#include <QWidget>

#include "ui/input_stats.h"


class QTreeWidget;
class QTreeWidgetItem;

class InputStatsPanel: public QWidget
{
    Q_OBJECT;
public:
    InputStatsPanel( QWidget *, intf_thread_t * );
    virtual ~InputStatsPanel();
private:
    intf_thread_t *p_intf;
    Ui::InputStats ui;

public slots:
    void Update( input_item_t * );
    void Clear();
};

class MetaPanel: public QWidget
{
    Q_OBJECT;
public:
    MetaPanel( QWidget *, intf_thread_t * );
    virtual ~MetaPanel();
private:
    intf_thread_t *p_intf;

public slots:
    void Update( input_item_t * );
    void Clear();

    char* GetURI();
    char* GetName();
};

class InfoPanel: public QWidget
{
    Q_OBJECT;
public:
    InfoPanel( QWidget *, intf_thread_t * );
    virtual ~InfoPanel();
private:
    intf_thread_t *p_intf;
    QTreeWidget *InfoTree;

public slots:
    void Update( input_item_t * );
    void Clear();
};

#endif
