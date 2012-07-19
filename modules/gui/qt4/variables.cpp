/*****************************************************************************
 * variables.cpp : VLC variable class
 ****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
 * Copyright (C) 2006 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "qt4.hpp"
#include "variables.hpp"

QVLCVariable::QVLCVariable (vlc_object_t *obj, const char *varname, int type,
                            bool inherit)
    : object (obj), name (qfu(varname))
{
    vlc_object_hold (object);

    if (inherit)
        type |= VLC_VAR_DOINHERIT;
    var_Create (object, qtu(name), type);
    var_AddCallback (object, qtu(name), callback, this);
}

QVLCVariable::~QVLCVariable (void)
{
    var_DelCallback (object, qtu(name), callback, this);
    var_Destroy (object, qtu(name));
    vlc_object_release (object);
}

int QVLCVariable::callback (vlc_object_t *object, const char *,
                            vlc_value_t old, vlc_value_t cur, void *data)
{
    VLC_UNUSED(object);

    QVLCVariable *self = static_cast<QVLCVariable *>(data);

    self->trigger (old, cur);
    return VLC_SUCCESS;
}


QVLCPointer::QVLCPointer (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCVariable (obj, varname, VLC_VAR_ADDRESS, inherit)
{
}

void QVLCPointer::trigger (vlc_value_t, vlc_value_t cur)
{
    emit pointerChanged (cur.p_address);
}

bool QVLCPointer::addCallback (QObject *tgt, const char *method,
                               Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(pointerChanged(void *)), method, type);
}

QVLCInteger::QVLCInteger (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCVariable (obj, varname, VLC_VAR_INTEGER, inherit)
{
}

void QVLCInteger::trigger (vlc_value_t, vlc_value_t cur)
{
    emit integerChanged (cur.i_int);
}

bool QVLCInteger::addCallback (QObject *tgt, const char *method,
                               Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(integerChanged(qlonglong)), method,
                         type);
}

QVLCBool::QVLCBool (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCVariable (obj, varname, VLC_VAR_BOOL, inherit)
{
}

void QVLCBool::trigger (vlc_value_t, vlc_value_t cur)
{
    emit boolChanged (cur.b_bool);
}

bool QVLCBool::addCallback (QObject *tgt, const char *method,
                            Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(boolChanged(bool)), method, type);
}

QVLCFloat::QVLCFloat (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCVariable (obj, varname, VLC_VAR_FLOAT, inherit)
{
}

void QVLCFloat::trigger (vlc_value_t, vlc_value_t cur)
{
    emit floatChanged (cur.f_float);
}

bool QVLCFloat::addCallback (QObject *tgt, const char *method,
                            Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(floatChanged(float)), method, type);
}

QVLCString::QVLCString (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCVariable (obj, varname, VLC_VAR_STRING, inherit)
{
}

void QVLCString::trigger (vlc_value_t, vlc_value_t cur)
{
    QString str = qfu(cur.psz_string);
    emit stringChanged (str);
}

bool QVLCString::addCallback (QObject *tgt, const char *method,
                              Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(stringChanged(QString)), method, type);
}
