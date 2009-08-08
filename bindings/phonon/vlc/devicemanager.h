/*  This file is part of the KDE project.

    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 2.1 or 3 of the License.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef Phonon_VLC_DEVICEMANAGER_H
#define Phonon_VLC_DEVICEMANAGER_H

#include <phonon/audiooutputinterface.h>

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

namespace Phonon
{
namespace VLC {

class Backend;
class DeviceManager;
class AbstractRenderer;
class VideoWidget;

class AudioDevice
{
public :
    AudioDevice(DeviceManager *s, const QByteArray &deviceId, const QByteArray &hw_id = "");
    int id;
    QByteArray vlcId;
    QByteArray description;
    QByteArray hwId;
};

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    DeviceManager(Backend *parent);
    virtual ~DeviceManager();
    const QList<AudioDevice> audioOutputDevices() const;
    int deviceId(const QByteArray &vlcId) const;
    QByteArray deviceDescription(int id) const;

signals:
    void deviceAdded(int);
    void deviceRemoved(int);

public slots:
    void updateDeviceList();

private:
    bool canOpenDevice() const;
    Backend *m_backend;
    QList <AudioDevice> m_audioDeviceList;
};
}
} // namespace Phonon::VLC

QT_END_NAMESPACE

#endif // Phonon_VLC_DEVICEMANAGER_H
