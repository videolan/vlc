/*****************************************************************************
 * legacy_variables.cpp : VLC variable class
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

#include "qt.hpp"
#include "legacy_variables.hpp"

QVLCLEGACYVariable::QVLCLEGACYVariable (vlc_object_t *obj, const char *varname, int type,
                            bool inherit)
    : object (obj), name (qfu(varname))
{
    if (inherit)
        type |= VLC_VAR_DOINHERIT;
    var_Create (object, qtu(name), type);
    var_AddCallback (object, qtu(name), callback, this);
}

QVLCLEGACYVariable::~QVLCLEGACYVariable (void)
{
    var_DelCallback (object, qtu(name), callback, this);
    var_Destroy (object, qtu(name));
}

int QVLCLEGACYVariable::callback(vlc_object_t *, const char *,
                           vlc_value_t old, vlc_value_t cur, void *data)
{
    QVLCLEGACYVariable *self = static_cast<QVLCLEGACYVariable *>(data);

    self->trigger (old, cur);
    return VLC_SUCCESS;
}


QVLCLEGACYPointer::QVLCLEGACYPointer (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCLEGACYVariable (obj, varname, VLC_VAR_ADDRESS, inherit)
{
}

void QVLCLEGACYPointer::trigger (vlc_value_t, vlc_value_t cur)
{
    emit pointerChanged (cur.p_address);
}

bool QVLCLEGACYPointer::addCallback (QObject *tgt, const char *method,
                               Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(pointerChanged(void *)), method, type);
}

QVLCLEGACYInteger::QVLCLEGACYInteger (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCLEGACYVariable (obj, varname, VLC_VAR_INTEGER, inherit)
{
}

void QVLCLEGACYInteger::trigger (vlc_value_t, vlc_value_t cur)
{
    emit integerChanged (cur.i_int);
}

bool QVLCLEGACYInteger::addCallback (QObject *tgt, const char *method,
                               Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(integerChanged(qlonglong)), method,
                         type);
}

QVLCLEGACYBool::QVLCLEGACYBool (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCLEGACYVariable (obj, varname, VLC_VAR_BOOL, inherit)
{
}

void QVLCLEGACYBool::trigger (vlc_value_t, vlc_value_t cur)
{
    emit boolChanged (cur.b_bool);
}

bool QVLCLEGACYBool::addCallback (QObject *tgt, const char *method,
                            Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(boolChanged(bool)), method, type);
}

QVLCLEGACYFloat::QVLCLEGACYFloat (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCLEGACYVariable (obj, varname, VLC_VAR_FLOAT, inherit)
{
}

void QVLCLEGACYFloat::trigger (vlc_value_t, vlc_value_t cur)
{
    emit floatChanged (cur.f_float);
}

bool QVLCLEGACYFloat::addCallback (QObject *tgt, const char *method,
                            Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(floatChanged(float)), method, type);
}

QVLCLEGACYString::QVLCLEGACYString (vlc_object_t *obj, const char *varname, bool inherit)
    : QVLCLEGACYVariable (obj, varname, VLC_VAR_STRING, inherit)
{
}

void QVLCLEGACYString::trigger (vlc_value_t, vlc_value_t cur)
{
    QString str = qfu(cur.psz_string);
    emit stringChanged (str);
}

bool QVLCLEGACYString::addCallback (QObject *tgt, const char *method,
                              Qt::ConnectionType type)
{
    return tgt->connect (this, SIGNAL(stringChanged(QString)), method, type);
}
