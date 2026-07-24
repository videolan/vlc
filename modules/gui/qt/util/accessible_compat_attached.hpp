/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ACCESSIBLE_COMPAT_ATTACHED_HPP
#define ACCESSIBLE_COMPAT_ATTACHED_HPP

#include <QObject>
#include <QWidget>
#include <QtQml/qqml.h>

class AccessibleCompatAttached : public QObject {
    Q_OBJECT

    QML_NAMED_ELEMENT(AccessibleCompat)
    QML_UNCREATABLE("Compatibility for accessible")
    QML_ATTACHED(AccessibleCompatAttached)

    Q_PROPERTY(QString id READ getId WRITE setId NOTIFY IdChanged FINAL)

    public:
    AccessibleCompatAttached(QObject* parent = nullptr);

    void setId(const QString& id);
    static void setId(QWidget* wd, const QString& id);
    
    QString getId();

    static AccessibleCompatAttached* qmlAttachedProperties(QObject* parent);
signals:
    void IdChanged();
};

#endif // ACCESSIBLE_COMPAT_ATTACHED_HPP
