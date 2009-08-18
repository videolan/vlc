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

#include "audiooutput.h"
#include "devicemanager.h"
#include "backend.h"

#include "mediaobject.h"
#include "vlcmediaobject.h"

#include "vlcloader.h"

namespace Phonon
{
namespace VLC {

AudioOutput::AudioOutput(Backend *p_back, QObject * p_parent)
        : SinkNode(p_parent),
        f_volume(1.0),
        i_device(0),
        p_backend(p_back)
{
    p_media_object = 0;
}

AudioOutput::~AudioOutput()
{
}

qreal AudioOutput::volume() const
{
    return f_volume;
}

void AudioOutput::setVolume(qreal volume)
{
    if (vlc_instance) {
        libvlc_audio_set_volume(vlc_instance, (int)(f_volume * 100), vlc_exception);
        vlcExceptionRaised();
        f_volume = volume;
        emit volumeChanged(f_volume);
    }
}

int AudioOutput::outputDevice() const
{
    return i_device;
}

bool AudioOutput::setOutputDevice(int device)
{
    if (i_device == device)
        return true;

    const QList<AudioDevice> deviceList = p_backend->deviceManager()->audioOutputDevices();
    if (device >= 0 && device < deviceList.size()) {

        i_device = device;
        const QByteArray deviceName = deviceList.at(device).vlcId;
        libvlc_audio_output_set(vlc_instance, (char *) deviceList.at(device).vlcId.data());
        qDebug() << "set aout " << deviceList.at(device).vlcId.data();
//         if (deviceName == DEFAULT_ID) {
//             libvlc_audio_device_set(p_vlc_instance, DEFAULT, vlc_exception);
//             vlcExceptionRaised();
//         } else if (deviceName.startsWith(ALSA_ID)) {
//             qDebug() << "setting ALSA " << deviceList.at(device).hwId.data();
//             libvlc_audio_device_set(p_vlc_instance, ALSA, vlc_exception);
//             vlcExceptionRaised();
//             libvlc_audio_alsa_device_set(p_vlc_instance,
//                                          deviceList.at(device).hwId,
//                                          vlc_exception);
//             vlcExceptionRaised();
    }

    return true;
}

#if (PHONON_VERSION >= PHONON_VERSION_CHECK(4, 2, 0))
bool AudioOutput::setOutputDevice(const Phonon::AudioOutputDevice & device)
{
    return true;
}
#endif

}
} // Namespace Phonon::VLC
