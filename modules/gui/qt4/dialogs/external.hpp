/*****************************************************************************
 * external.hpp : Dialogs from other LibVLC core and other plugins
 ****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#ifndef VLC_QT4_DIALOGS_EXTERNAL_H_
# define VLC_QT4_DIALOGS_EXTERNAL_H 1

#include <QObject>
#include <vlc_common.h>

struct intf_thread_t;

class DialogHandler : public QObject
{
    Q_OBJECT
public:
    DialogHandler (intf_thread_t *);
    ~DialogHandler (void);

private:
    intf_thread_t *intf;
    static int MessageCallback( vlc_object_t *, const char *, vlc_value_t,
                                vlc_value_t, void * );

private slots:
    void displayMessage (const struct dialog_fatal_t *);

signals:
    void message (const struct dialog_fatal_t *);
};

#endif
