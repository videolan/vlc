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

#ifndef PHONON_VLC_AUDIOOUTPUT_H
#define PHONON_VLC_AUDIOOUTPUT_H

#include "sinknode.h"

#include <phonon/audiooutputinterface.h>

namespace Phonon
{
namespace VLC {
class Backend;

class AudioOutput : public SinkNode, public AudioOutputInterface
{
    Q_OBJECT
    Q_INTERFACES(Phonon::AudioOutputInterface)

public:

    AudioOutput(Backend *p_back, QObject * p_parent);
    ~AudioOutput();

    qreal volume() const;
    void setVolume(qreal volume);

    int outputDevice() const;
    bool setOutputDevice(int);
#if (PHONON_VERSION >= PHONON_VERSION_CHECK(4, 2, 0))
    bool setOutputDevice(const AudioOutputDevice & device);
#endif

signals:

    void volumeChanged(qreal volume);
    void audioDeviceFailed();

private:

    qreal f_volume;
    int i_device;
    Backend *p_backend;

};

}
} // Namespace Phonon::VLC

#endif // PHONON_VLC_AUDIOOUTPUT_H
