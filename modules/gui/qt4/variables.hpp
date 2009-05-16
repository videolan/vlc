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

#ifndef QVLC_VARIABLES_H_
#define QVLC_VARIABLES_H_ 1

#include <QObject>
#include <vlc_common.h>

class QVLCVariable : public QObject
{
    Q_OBJECT
private:
    static int callback (vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
    vlc_object_t *object;
    QString name;
    virtual void trigger (vlc_object_t *, vlc_value_t, vlc_value_t) = 0;

public:
    QVLCVariable (vlc_object_t *, const char *, int, bool);
    virtual ~QVLCVariable (void);
};

class QVLCPointer : public QVLCVariable
{
    Q_OBJECT
private:
    virtual void trigger (vlc_object_t *, vlc_value_t, vlc_value_t);

public:
    QVLCPointer (vlc_object_t *, const char *, bool inherit = false);

signals:
    void pointerChanged (vlc_object_t *, void *, void *);
    void pointerChanged (vlc_object_t *, void *);
};

class QVLCInteger : public QVLCVariable
{
    Q_OBJECT
private:
    virtual void trigger (vlc_object_t *, vlc_value_t, vlc_value_t);

public:
    QVLCInteger (vlc_object_t *, const char *, bool inherit = false);

signals:
    void integerChanged (vlc_object_t *, int, int);
    void integerChanged (vlc_object_t *, int);
};

#endif
