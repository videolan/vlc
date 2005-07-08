/*****************************************************************************
 * QConfigItem.h : includes for the QConfigItem class
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Andres Krapf <dae@chez.com> Sun Mar 25 2001
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

#ifndef _KCONFIGITEM_H_
#define _KCONFIGITEM_H_
#include <qobject.h>
#include <qstring.h>
/*
  A class to handle the information for one configuration item. 
*/

class QConfigItem : public QObject
{
    Q_OBJECT
 public:
    QConfigItem(QObject *parent, QString name, int iType, int i_val);
    QConfigItem(QObject *parent, QString name, int iType, float f_val);
    QConfigItem(QObject *parent, QString name, int iType, QString s_val);
    ~QConfigItem();

    int getType();
    float fValue();
    int iValue();
    QString sValue();
    bool changed();

 public slots:
    void setValue(int val);
    void setValue(float val);
    void setValue(double val);
    void setValue(const QString &val);
    void resetChanged();
    
 private:
    int iVal, type;
    float fVal;
    QString sVal;
    bool bChanged;
};
#endif
