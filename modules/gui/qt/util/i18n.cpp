/*****************************************************************************
 * Access to vlc_gettext from QML
 ****************************************************************************
 * Copyright (C) 2019 the VideoLAN team
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
# include "config.h"
#endif

#include "i18n.hpp"
#include <vlc_common.h>
#include <QDebug>

#ifdef qtr
#undef qtr
#endif

I18n::I18n(QObject *parent)
    : QObject(parent)
{
}

QString I18n::qtr(const QString msgid) const
{
    //we need msgIdUtf8 to stay valid for the whole scope,
    //as vlc_gettext may return the incoming pointer
    QByteArray msgIdUtf8 = msgid.toUtf8();
    const char * msgstr_c = vlc_gettext(msgIdUtf8.constData());
    return QString::fromUtf8( msgstr_c );
}
