/*****************************************************************************
 * EPGWidget.hpp : EPGWidget
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef EPGWIDGET_H
#define EPGWIDGET_H

#include "qt.hpp"

#include <vlc_epg.h>
#include <vlc_input_item.h>

#include <QWidget>
#include <QStackedWidget>

class EPGView;
class EPGItem;
class EPGRuler;
class EPGChannels;

class EPGWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EPGWidget( QWidget* parent = 0 );
    void reset();
    enum
    {
        EPGVIEW_WIDGET = 0,
        NOEPG_WIDGET = 1
    };

public slots:
    void setZoom( int level );
    void updateEPG( input_item_t * );
    void activateProgram( int );

private:
    EPGRuler* m_rulerWidget;
    EPGView* m_epgView;
    EPGChannels *m_channelsWidget;
    QStackedWidget *rootWidget;

    enum input_item_type_e i_event_source_type;
    bool b_input_type_known;

signals:
    void itemSelectionChanged( EPGItem * );
    void programActivated( int );
};

#endif // EPGWIDGET_H
