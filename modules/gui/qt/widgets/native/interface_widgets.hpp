/*****************************************************************************
 * interface_widgets.hpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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

#ifndef VLC_QT_INTERFACE_WIDGETS_HPP_
#define VLC_QT_INTERFACE_WIDGETS_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util/shared_input_item.hpp"

#include <QLabel>

class QWidget;
class QMouseEvent;
class MetaPanel;

struct qt_intf_t;
struct input_item_t;

class CoverArtLabel : public QLabel
{
    Q_OBJECT
public:
    CoverArtLabel( QWidget *parent, qt_intf_t * );
    void setItem( const SharedInputItem & );
    virtual ~CoverArtLabel();

protected:
    virtual void mouseDoubleClickEvent( QMouseEvent *event ) override;

private:
    qt_intf_t *p_intf;
    SharedInputItem p_item;

public slots:
    void showArtUpdate( const QString& );
    void showArtUpdate( input_item_t * );
    void askForUpdate();
    void setArtFromFile();
    void clear();
};

#endif
