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

#include "backend.h"

#include "audiooutput.h"
#include "mediaobject.h"
#include "videowidget.h"
#include "devicemanager.h"
#include "effectmanager.h"
#include "effect.h"
#include "sinknode.h"
#include "vlcloader.h"
#include "vlcmediaobject.h"

#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtCore/QtPlugin>

Q_EXPORT_PLUGIN2(phonon_vlc, Phonon::VLC::Backend)

namespace Phonon
{
namespace VLC {

Backend::Backend(QObject *parent, const QVariantList &)
        : QObject(parent)
        , m_deviceManager(0)
        , m_effectManager(0)
        , m_debugLevel(Warning)
{
    bool wasInit = vlcInit();

    setProperty("identifier",     QLatin1String("phonon_vlc"));
    setProperty("backendName",    QLatin1String("VLC"));
    setProperty("backendComment", QLatin1String("VLC plugin for Phonon"));
    setProperty("backendVersion", QLatin1String("0.1"));
    setProperty("backendWebsite", QLatin1String("http://multimedia.kde.org/"));

    // Check if we should enable debug output
    QString debugLevelString = qgetenv("PHONON_VLC_DEBUG");
    int debugLevel = debugLevelString.toInt();
    if (debugLevel > 3) // 3 is maximum
        debugLevel = 3;
    m_debugLevel = (DebugLevel)debugLevel;

    if (wasInit) {
        logMessage(QString("Using VLC version %0").arg(libvlc_get_version()));
    } else {
        qWarning("Phonon::VLC::vlcInit: Failed to initialize VLC");
    }

    m_deviceManager = new DeviceManager(this);
    m_effectManager = new EffectManager(this);
}

Backend::~Backend()
{
//    vlcRelease();
}

QObject *Backend::createObject(BackendInterface::Class c, QObject *parent, const QList<QVariant> &args)
{
    switch (c) {
    case MediaObjectClass:
        return new VLCMediaObject(parent);
    case VolumeFaderEffectClass:
//        return new VolumeFaderEffect(parent);
        logMessage("createObject() : VolumeFaderEffect not implemented");
        break;
    case AudioOutputClass: {
        AudioOutput *ao = new AudioOutput(this, parent);
        m_audioOutputs.append(ao);
        return ao;
    }
    case AudioDataOutputClass:
//        return new AudioDataOutput(parent);
        logMessage("createObject() : AudioDataOutput not implemented");
        break;
    case VisualizationClass:
//        return new Visualization(parent);
        logMessage("createObject() : Visualization not implemented");
        break;
    case VideoDataOutputClass:
//        return new VideoDataOutput(parent);
        logMessage("createObject() : VideoDataOutput not implemented");
        break;
    case EffectClass:
        return new Effect(m_effectManager, args[0].toInt(), parent);
    case VideoWidgetClass:
        return new VideoWidget(qobject_cast<QWidget *>(parent));
    default:
        logMessage("createObject() : Backend object not available");
    }
    return 0;
}

bool Backend::supportsVideo() const
{
    return true;
}

bool Backend::supportsOSD() const
{
    return true;
}

bool Backend::supportsFourcc(quint32 fourcc) const
{
    return true;
}

bool Backend::supportsSubtitles() const
{
    return true;
}

QStringList Backend::availableMimeTypes() const
{
    if (m_supportedMimeTypes.isEmpty()) {
        const_cast<Backend *>(this)->m_supportedMimeTypes
        << QLatin1String("application/ogg")
        << QLatin1String("application/vnd.rn-realmedia")
        << QLatin1String("application/x-annodex")
        << QLatin1String("application/x-flash-video")
        << QLatin1String("application/x-quicktimeplayer")
        << QLatin1String("audio/168sv")
        << QLatin1String("audio/8svx")
        << QLatin1String("audio/aiff")
        << QLatin1String("audio/basic")
        << QLatin1String("audio/mp3")
        << QLatin1String("audio/mpeg")
        << QLatin1String("audio/mpeg2")
        << QLatin1String("audio/mpeg3")
        << QLatin1String("audio/vnd.rn-realaudio")
        << QLatin1String("audio/wav")
        << QLatin1String("audio/x-16sv")
        << QLatin1String("audio/x-8svx")
        << QLatin1String("audio/x-aiff")
        << QLatin1String("audio/x-basic")
        << QLatin1String("audio/x-m4a")
        << QLatin1String("audio/x-mp3")
        << QLatin1String("audio/x-mpeg")
        << QLatin1String("audio/x-mpeg2")
        << QLatin1String("audio/x-mpeg3")
        << QLatin1String("audio/x-mpegurl")
        << QLatin1String("audio/x-ms-wma")
        << QLatin1String("audio/x-ogg")
        << QLatin1String("audio/x-pn-aiff")
        << QLatin1String("audio/x-pn-au")
        << QLatin1String("audio/x-pn-realaudio-plugin")
        << QLatin1String("audio/x-pn-wav")
        << QLatin1String("audio/x-pn-windows-acm")
        << QLatin1String("audio/x-real-audio")
        << QLatin1String("audio/x-realaudio")
        << QLatin1String("audio/x-speex+ogg")
        << QLatin1String("audio/x-wav")
        << QLatin1String("image/ilbm")
        << QLatin1String("image/png")
        << QLatin1String("image/x-ilbm")
        << QLatin1String("image/x-png")
        << QLatin1String("video/anim")
        << QLatin1String("video/avi")
        << QLatin1String("video/mkv")
        << QLatin1String("video/mng")
        << QLatin1String("video/mp4")
        << QLatin1String("video/mpeg")
        << QLatin1String("video/mpg")
        << QLatin1String("video/msvideo")
        << QLatin1String("video/quicktime")
        << QLatin1String("video/x-anim")
        << QLatin1String("video/x-flic")
        << QLatin1String("video/x-mng")
        << QLatin1String("video/x-mpeg")
        << QLatin1String("video/x-ms-asf")
        << QLatin1String("video/x-ms-wmv")
        << QLatin1String("video/x-msvideo")
        << QLatin1String("video/x-quicktime");
    }
    return m_supportedMimeTypes;
}

QList<int> Backend::objectDescriptionIndexes(ObjectDescriptionType type) const
{
    QList<int> list;

    switch (type) {
    case Phonon::AudioOutputDeviceType: {
        QList<AudioDevice> deviceList = deviceManager()->audioOutputDevices();
        for (int dev = 0 ; dev < deviceList.size() ; ++dev)
            list.append(deviceList[dev].id);
        break;
    }
    break;
    case Phonon::EffectType: {
        QList<EffectInfo*> effectList = effectManager()->effects();
        for (int eff = 0; eff < effectList.size(); ++eff)
            list.append(eff);
        break;
    }
    break;
    default:
        break;
    }

    return list;
}

QHash<QByteArray, QVariant> Backend::objectDescriptionProperties(ObjectDescriptionType type, int index) const
{
    QHash<QByteArray, QVariant> ret;

    switch (type) {
    case Phonon::AudioOutputDeviceType: {
        QList<AudioDevice> audioDevices = deviceManager()->audioOutputDevices();
        if (index >= 0 && index < audioDevices.size()) {
            ret.insert("name", audioDevices[index].vlcId);
            ret.insert("description", audioDevices[index].description);
            ret.insert("icon", QLatin1String("audio-card"));
        }
    }
    break;
    case Phonon::EffectType: {
        QList<EffectInfo*> effectList = effectManager()->effects();
        if (index >= 0 && index <= effectList.size()) {
            const EffectInfo *effect = effectList[ index ];
            ret.insert("name", effect->name());
            ret.insert("description", effect->description());
            ret.insert("author", effect->author());
        } else {
            Q_ASSERT(1); // Since we use list position as ID, this should not happen
        }
    }
    break;
    default:
        break;
    }

    return ret;
}

bool Backend::startConnectionChange(QSet<QObject *> objects)
{
    foreach(QObject *object, objects) {
        logMessage(QString("Object: %0").arg(object->metaObject()->className()));
    }

    // There is nothing we can do but hope the connection changes will not take too long
    // so that buffers would underrun
    // But we should be pretty safe the way xine works by not doing anything here.
    return true;
}

bool Backend::connectNodes(QObject *source, QObject *sink)
{
    logMessage(QString("Backend connected %0 to %1")
               .arg(source->metaObject()->className())
               .arg(sink->metaObject()->className()));

    // Example:
    // source = Phonon::VLC_MPlayer::MediaObject
    // sink = Phonon::VLC_MPlayer::VideoWidget

    // Example:
    // source = Phonon::VLC_MPlayer::MediaObject
    // sink = Phonon::VLC_MPlayer::AudioOutput

    // Example:
    // source = Phonon::VLC_MPlayer::MediaObject
    // sink = Phonon::VLC_MPlayer::Effect

    // Example:
    // source = Phonon::VLC_MPlayer::Effect
    // sink = Phonon::VLC_MPlayer::AudioOutput

    SinkNode *sinkNode = qobject_cast<SinkNode *>(sink);
    if (sinkNode) {
        PrivateMediaObject *mediaObject = qobject_cast<PrivateMediaObject *>(source);
        if (mediaObject) {
            // Connect the SinkNode to a MediaObject
            sinkNode->connectToMediaObject(mediaObject);
            return true;
        } else {
            // FIXME try to find a better way...
//            Effect *effect = qobject_cast<Effect *>(source);
            return true;
        }
    }

    logMessage(QString("Linking %0 to %1 failed")
               .arg(source->metaObject()->className())
               .arg(sink->metaObject()->className()),
               Warning);

    return false;
}

bool Backend::disconnectNodes(QObject *source, QObject *sink)
{
    SinkNode *sinkNode = qobject_cast<SinkNode *>(sink);
    if (sinkNode) {
        PrivateMediaObject *mediaObject = qobject_cast<PrivateMediaObject *>(source);
        if (mediaObject) {
            // Disconnect the SinkNode from a MediaObject
            sinkNode->disconnectFromMediaObject(mediaObject);
            return true;
        } else {
            // FIXME try to find a better way...
//            Effect *effect = qobject_cast<Effect *>(source);
            return true;
        }
    }

    return false;
}

bool Backend::endConnectionChange(QSet<QObject *> objects)
{
    foreach(QObject *object, objects) {
        logMessage(QString("Object: %0").arg(object->metaObject()->className()));
    }

    return true;
}

DeviceManager* Backend::deviceManager() const
{
    return m_deviceManager;
}

EffectManager* Backend::effectManager() const
{
    return m_effectManager;
}

/**
 * Return a debuglevel that is determined by the
 * PHONON_VLC_DEBUG environment variable.
 *
 *  Warning - important warnings
 *  Info    - general info
 *  Debug   - gives extra info
 */
Backend::DebugLevel Backend::debugLevel() const
{
    return m_debugLevel;
}

/**
 * Print a conditional debug message based on the current debug level
 * If obj is provided, classname and objectname will be printed as well
 *
 * see debugLevel()
 */
void Backend::logMessage(const QString &message, int priority, QObject *obj) const
{
    if (debugLevel() > 0) {
        QString output;
        if (obj) {
            // Strip away namespace from className
            QString className(obj->metaObject()->className());
            int nameLength = className.length() - className.lastIndexOf(':') - 1;
            className = className.right(nameLength);
            output.sprintf("%s %s (%s %p)", message.toLatin1().constData(),
                           obj->objectName().toLatin1().constData(),
                           className.toLatin1().constData(), obj);
        } else {
            output = message;
        }
        if (priority <= (int)debugLevel()) {
            qDebug() << QString("PVLC(%1): %2").arg(priority).arg(output);
        }
    }
}

}
} // Namespace Phonon::VLC
