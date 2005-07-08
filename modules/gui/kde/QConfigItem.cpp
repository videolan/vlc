/*****************************************************************************
 * QConfigItem.cpp: The QConfigItem class
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no> Mon 12.08.2002
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include "QConfigItem.h"
#include <vlc/vlc.h>
QConfigItem::QConfigItem(QObject *parent, QString name, int iType, int i_val) :
    QObject(parent, name)
{
    type = iType;
    iVal = i_val;
    bChanged = false;
}

QConfigItem::QConfigItem(QObject *parent, QString name, int iType, float f_val) :
    QObject(parent, name)
{
    type = iType;
    fVal = f_val;
    bChanged = false;
}

QConfigItem::QConfigItem(QObject *parent, QString name, int iType, QString s_val) :
    QObject(parent, name)
{
    type = iType;
    sVal = s_val;
    bChanged = false;
}

QConfigItem::~QConfigItem()
{
    ;
}

int QConfigItem::getType()
{
    return type;
}

int QConfigItem::iValue()
{
    return iVal;
}

float QConfigItem::fValue()
{
    return fVal;
}

QString QConfigItem::sValue()
{
    return sVal;
}

void QConfigItem::setValue(int val)
{
    iVal = val;
    bChanged = true;
}

void QConfigItem::setValue(float val)
{
    fVal = val;
    bChanged = true;
}

void QConfigItem::setValue(double val)
{
    fVal = (float)val;
    bChanged = true;
}

void QConfigItem::setValue(const QString &val)
{
    sVal = val;
    bChanged = true;
}

bool QConfigItem::changed()
{
    return bChanged;
}

void QConfigItem::resetChanged()
{
    bChanged = false;
}
