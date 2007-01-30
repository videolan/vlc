/*****************************************************************************
 * infopanels.hpp : Panels for the information dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
#include <QTabWidget>
#include <QLabel>

class QTreeWidget;
class QTreeWidgetItem;

class MetaPanel: public QWidget
{
    Q_OBJECT;
public:
    MetaPanel( QWidget *, intf_thread_t * );
    virtual ~MetaPanel();
private:
    intf_thread_t *p_intf;
    QLabel *uri_text;
    QLabel *name_text;
    QLabel *artist_text;
    QLabel *genre_text;
    QLabel *copyright_text;
    QLabel *collection_text;
    QLabel *seqnum_text;
    QLabel *description_text;
    QLabel *rating_text;
    QLabel *date_text;
    QLabel *setting_text;
    QLabel *language_text;
    QLabel *nowplaying_text;
    QLabel *publisher_text;

public slots:
    void update( input_item_t * );
    void clear();
};


class InputStatsPanel: public QWidget
{
    Q_OBJECT;
public:
    InputStatsPanel( QWidget *, intf_thread_t * );
    virtual ~InputStatsPanel();
private:
    intf_thread_t *p_intf;

    QTreeWidget *StatsTree;
    QTreeWidgetItem *input;
    QTreeWidgetItem *read_media_stat;
    QTreeWidgetItem *input_bitrate_stat;
    QTreeWidgetItem *demuxed_stat;
    QTreeWidgetItem *stream_bitrate_stat;

    QTreeWidgetItem *video;
    QTreeWidgetItem *vdecoded_stat;
    QTreeWidgetItem *vdisplayed_stat;
    QTreeWidgetItem *vlost_frames_stat;

    QTreeWidgetItem *streaming;
    QTreeWidgetItem *send_stat;
    QTreeWidgetItem *send_bytes_stat;
    QTreeWidgetItem *send_bitrate_stat;

    QTreeWidgetItem *audio;
    QTreeWidgetItem *adecoded_stat;
    QTreeWidgetItem *aplayed_stat;
    QTreeWidgetItem *alost_stat;

public slots:
    void update( input_item_t * );
    void clear();
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
    void update( input_item_t * );
    void clear();
};

class InfoTab: public QTabWidget
{
    Q_OBJECT;
public:
    InfoTab( QWidget *, intf_thread_t *, bool );
    virtual ~InfoTab();
    void update( input_item_t *, bool, bool );
    void clear();
private:
    bool stats;
    intf_thread_t *p_intf;
    InputStatsPanel *ISP;
    MetaPanel *MP;
    InfoPanel *IP;
    int i_runs;
};

#endif
