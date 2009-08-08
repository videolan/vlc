/*****************************************************************************
 * VLC backend for the Phonon library                                        *
 * Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>               *
 * Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>                *
 * Copyright (C) 2009 Fathi Boudra <fabo@kde.org>                            *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 3 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with this package; if not, write to the Free Software       *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#ifndef Phonon_VLC_EFFECTMANAGER_H
#define Phonon_VLC_EFFECTMANAGER_H

#include <phonon/effectinterface.h>
#include <phonon/effectparameter.h>

#include <QtCore/QObject>

namespace Phonon
{
namespace VLC {
class Backend;
class EffectManager;

class EffectInfo
{
public:

    enum Type {AudioEffect, VideoEffect};

    EffectInfo(const QString &name,
               const QString &description,
               const QString &author,
               int filter,
               Type type);

    QString name() const {
        return m_name;
    }

    QString description() const {
        return m_description;
    }

    QString author() const {
        return m_author;
    }

    int filter() const {
        return m_filter;
    }

    Type type() const {
        return m_type;
    }

private:
    QString m_name;
    QString m_description;
    QString m_author;
    int m_filter;
    Type m_type;
};

class EffectManager : public QObject
{
    Q_OBJECT

public:
    EffectManager(Backend *parent);
    virtual ~EffectManager();

    const QList<EffectInfo *> audioEffects() const;
    const QList<EffectInfo *> videoEffects() const;
    const QList<EffectInfo *> effects() const;

private:
    void updateEffects();

    Backend *m_backend;
    QList<EffectInfo *> m_effectList;
    QList<EffectInfo *> m_audioEffectList;
    QList<EffectInfo *> m_videoEffectList;
    bool m_equalizerEnabled;
};

}
} // namespace Phonon::VLC

#endif // Phonon_VLC_EFFECTMANAGER_H
