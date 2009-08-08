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

#ifndef PHONON_VLC_EFFECT_H
#define PHONON_VLC_EFFECT_H

#include "sinknode.h"
#include "effectmanager.h"

#include <phonon/effectinterface.h>
#include <phonon/effectparameter.h>

namespace Phonon
{
namespace VLC {

class EffectManager;


class Effect : public SinkNode, public EffectInterface
{
    Q_OBJECT
    Q_INTERFACES(Phonon::EffectInterface)

public:

    Effect(EffectManager *p_em, int i_effectId, QObject *p_parent);
    ~Effect();

    void setupEffectParams();
    QList<EffectParameter> parameters() const;
    QVariant parameterValue(const EffectParameter & param) const;
    void setParameterValue(const EffectParameter & param, const QVariant & newValue);

    void connectToMediaObject(PrivateMediaObject *p_media_object);
    void disconnectFromMediaObject(PrivateMediaObject *p_media_object);

private:

    EffectManager *p_effectManager;
    int i_effect_filter;
    EffectInfo::Type effect_type;
    QList<Phonon::EffectParameter> parameterList;
};

}
} // Namespace Phonon::VLC

#endif // PHONON_VLC_EFFECT_H
