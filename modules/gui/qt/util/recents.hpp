/*****************************************************************************
 * recents.hpp : Recents MRL (menu)
 *****************************************************************************
 * Copyright © 2008-2014 VideoLAN and VLC authors
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

#include "qt.hpp"
#include "util/singleton.hpp"

#include <QObject>
#include <QStringList>
class QRegExp;
class QSignalMapper;

#define RECENTS_LIST_SIZE 30

class Open
{
public:
    static int openMRL( intf_thread_t*,
                        const QString &,
                        bool b_start = true);

    static int openMRLwithOptions( intf_thread_t*,
                                   const QString &,
                                   QStringList *options,
                                   bool b_start = true,
                                   const char* title = NULL);
};

class RecentsMRL : public QObject, public Singleton<RecentsMRL>
{
    Q_OBJECT
    friend class Singleton<RecentsMRL>;

public:
    
    void addRecent( const QString & );
    QSignalMapper *signalMapper;

    vlc_tick_t time( const QString &mrl );
    void setTime( const QString &mrl, const vlc_tick_t time );
    virtual ~RecentsMRL();

private:
    RecentsMRL( intf_thread_t* _p_intf );

    intf_thread_t *p_intf;

    QStringList   recents;
    QStringList   times;
    QRegExp       *filter;
    bool          isActive;

    void load();
    void save();
     
signals:
       void saved();
public slots:
    QStringList recentList();
    void clear();
    void playMRL( const QString & );
};

#endif
