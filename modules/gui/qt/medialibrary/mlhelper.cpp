/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#include "mlhelper.hpp"

// MediaLibrary includes
#include "mlbasemodel.hpp"

#include <QDir>

QString toValidLocalFile(const char *mrl)
{
    QUrl url(mrl);
    return url.isLocalFile() ? url.toLocalFile() : QString {};
}

QString urlToDisplayString(const QUrl &url)
{
    const QString displayString = url.toDisplayString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::NormalizePathSegments);
    if (url.isLocalFile())
        return QDir::toNativeSeparators(displayString);

    return displayString;
}
