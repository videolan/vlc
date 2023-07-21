/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifndef MAINCTX_SUBMODELS_HPP
#define MAINCTX_SUBMODELS_HPP

#include <QObject>
#include <QString>

class SearchCtx: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString pattern MEMBER m_pattern NOTIFY patternChanged FINAL)
    Q_PROPERTY(bool available MEMBER m_available NOTIFY availableChanged FINAL)

signals:
    void askShow();

public:
    using  QObject::QObject;

signals:
    void patternChanged(const QString& pattern);
    void availableChanged(bool available);

private:
    QString m_pattern;
    bool m_available = false;
};

#endif // MAINCTX_SUBMODELS_HPP
