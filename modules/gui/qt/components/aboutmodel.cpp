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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "aboutmodel.hpp"

#include "vlc_about.h"


AboutModel::AboutModel(QObject *parent) : QObject(parent)
{
}

QString AboutModel::getLicense() const
{
    return QString::fromUtf8(psz_license);
}

QString AboutModel::getAuthors() const
{
    return QString::fromUtf8(psz_authors);
}

QString AboutModel::getThanks() const
{
    return QString::fromUtf8(psz_thanks);
}

QString AboutModel::getVersion() const
{
    return QString::fromUtf8(VERSION_MESSAGE);

}
