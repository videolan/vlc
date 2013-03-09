/*****************************************************************************
 * EPGWidget.hpp : EPGWidget
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 * $Id$
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

#include "EPGView.hpp"
#include "EPGItem.hpp"
#include "EPGRuler.hpp"
#include "EPGChannels.hpp"

#include <vlc_common.h>
#include <vlc_epg.h>

#include <QWidget>
#include <QStackedWidget>

class QDateTime;

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

private:
    EPGRuler* m_rulerWidget;
    EPGView* m_epgView;
    EPGChannels *m_channelsWidget;
    QStackedWidget *rootWidget;

    uint8_t i_event_source_type;
    bool b_input_type_known;

signals:
    void itemSelectionChanged( EPGItem * );
};

#endif // EPGWIDGET_H
