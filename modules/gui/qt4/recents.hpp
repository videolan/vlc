/*****************************************************************************
 * recents.hpp : Recents MRL (menu)
 *****************************************************************************
 * Copyright © 2008-2014 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *          Jean-baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef QVLC_RECENTS_H_
#define QVLC_RECENTS_H_

#include "qt4.hpp"
#include "util/singleton.hpp"

#include <QObject>
class QStringList;
class QRegExp;
class QSignalMapper;

#define RECENTS_LIST_SIZE 10

class Open
{
public:
    void static openMRL( intf_thread_t*,
                         const QString &,
                         bool b_start = true,
                         bool b_playlist = true);
};

class RecentsMRL : public QObject, public Singleton<RecentsMRL>
{
    Q_OBJECT
    friend class Singleton<RecentsMRL>;

public:
    void addRecent( const QString & );
    QStringList recents();
    playlist_item_t *toPlaylist(int length);
    QSignalMapper *signalMapper;

private:
    RecentsMRL( intf_thread_t* _p_intf );
    virtual ~RecentsMRL();

    intf_thread_t *p_intf;
    QStringList   *stack;
    QRegExp       *filter;
    bool          isActive;

    void load();
    void save();
public slots:
    void clear();
};

#endif
