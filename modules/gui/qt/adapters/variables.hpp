/*****************************************************************************
 * variables.hpp : Dialogs from other LibVLC core and other plugins
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "qt.hpp"

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
    virtual void trigger (vlc_value_t, vlc_value_t) = 0;

public:
    QVLCVariable (vlc_object_t *, const char *, int, bool);
    virtual ~QVLCVariable (void);
};

class QVLCPointer : public QVLCVariable
{
    Q_OBJECT
private:
    void trigger (vlc_value_t, vlc_value_t) Q_DECL_OVERRIDE;

public:
    QVLCPointer (vlc_object_t *, const char *, bool inherit = false);
    bool addCallback (QObject *, const char *,
                      Qt::ConnectionType type = Qt::AutoConnection);

signals:
    void pointerChanged (void *);
};

class QVLCInteger : public QVLCVariable
{
    Q_OBJECT
private:
    void trigger (vlc_value_t, vlc_value_t) Q_DECL_OVERRIDE;

public:
    QVLCInteger (vlc_object_t *, const char *, bool inherit = false);
    bool addCallback (QObject *, const char *,
                      Qt::ConnectionType type = Qt::AutoConnection);

signals:
    void integerChanged (qlonglong);
};

class QVLCBool : public QVLCVariable
{
    Q_OBJECT
private:
   void trigger (vlc_value_t, vlc_value_t) Q_DECL_OVERRIDE;

public:
    QVLCBool (vlc_object_t *, const char *, bool inherit = false);
    bool addCallback (QObject *, const char *,
                      Qt::ConnectionType type = Qt::AutoConnection);

signals:
    void boolChanged (bool);
};

class QVLCFloat : public QVLCVariable
{
    Q_OBJECT
private:
    void trigger (vlc_value_t, vlc_value_t) Q_DECL_OVERRIDE;

public:
    QVLCFloat (vlc_object_t *, const char *, bool inherit = false);
    bool addCallback (QObject *, const char *,
                      Qt::ConnectionType type = Qt::AutoConnection);

signals:
    void floatChanged (float);
};

class QVLCString : public QVLCVariable
{
    Q_OBJECT
private:
    void trigger (vlc_value_t, vlc_value_t) Q_DECL_OVERRIDE;

public:
    QVLCString (vlc_object_t *, const char *, bool inherit = false);
    bool addCallback (QObject *, const char *,
                      Qt::ConnectionType type = Qt::AutoConnection);

signals:
    void stringChanged (QString);
};
#endif
