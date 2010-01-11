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

#include "vlcloader.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QLibrary>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtCore/QStringList>

// Global variables
libvlc_instance_t *vlc_instance = 0;
libvlc_exception_t *vlc_exception = new libvlc_exception_t();
libvlc_media_player_t *vlc_current_media_player = 0;

namespace Phonon
{
namespace VLC {

bool vlcInit()
{
    // Global variables
    vlc_instance = 0;
    vlc_exception = new libvlc_exception_t();

    QString path = vlcPath();
    if (!path.isEmpty()) {
        QString pluginsPath = QString("--plugin-path=") + QDir::toNativeSeparators(QFileInfo(vlcPath()).dir().path());
#if defined(Q_OS_UNIX)
        pluginsPath.append("/vlc");
#elif defined(Q_OS_WIN)
        pluginsPath.append("\\plugins");
#endif
        QByteArray p = path.toLatin1();
        QByteArray pp = pluginsPath.toLatin1();
        // VLC command line options. See vlc --full-help
        const char *vlcArgs[] = {
            p.constData(),
            pp.constData(),
            "--verbose=2",
            "--intf=dummy",
            "--extraintf=logger",
            "--ignore-config",
            "--reset-plugins-cache",
            "--no-media-library",
            "--no-one-instance",
            "--no-osd",
            "--no-stats",
            "--no-video-title-show"
        };

        libvlc_exception_init(vlc_exception);

        // Create and initialize a libvlc instance (it should be done only once)
        vlc_instance = libvlc_new(sizeof(vlcArgs) / sizeof(*vlcArgs),
                                  vlcArgs,
                                  vlc_exception);
        vlcExceptionRaised();

        return true;
    } else {
        return false;
    }
}

void vlcRelease()
{
    libvlc_release(vlc_instance);
    vlcExceptionRaised();
    vlcUnload();
}

void vlcExceptionRaised()
{
    if (libvlc_exception_raised(vlc_exception)) {
        qDebug() << "libvlc exception:" << libvlc_errmsg();
        libvlc_exception_clear(vlc_exception);
    }
}

#if defined(Q_OS_UNIX)
static bool libGreaterThan(const QString &lhs, const QString &rhs)
{
    QStringList lhsparts = lhs.split(QLatin1Char('.'));
    QStringList rhsparts = rhs.split(QLatin1Char('.'));
    Q_ASSERT(lhsparts.count() > 1 && rhsparts.count() > 1);

    for (int i = 1; i < rhsparts.count(); ++i) {
        if (lhsparts.count() <= i)
            // left hand side is shorter, so it's less than rhs
            return false;

        bool ok = false;
        int b = 0;
        int a = lhsparts.at(i).toInt(&ok);
        if (ok)
            b = rhsparts.at(i).toInt(&ok);
        if (ok) {
            // both toInt succeeded
            if (a == b)
                continue;
            return a > b;
        } else {
            // compare as strings;
            if (lhsparts.at(i) == rhsparts.at(i))
                continue;
            return lhsparts.at(i) > rhsparts.at(i);
        }
    }

    // they compared strictly equally so far
    // lhs cannot be less than rhs
    return true;
}
#endif

static QStringList findAllLibVlc()
{
    QStringList paths;
#if defined(Q_OS_UNIX)
    paths = QString::fromLatin1(qgetenv("LD_LIBRARY_PATH"))
            .split(QLatin1Char(':'), QString::SkipEmptyParts);
    paths << QLatin1String("/usr/lib") << QLatin1String("/usr/local/lib");

    QStringList foundVlcs;
    foreach (const QString &path, paths) {
        QDir dir = QDir(path);
        QStringList entryList = dir.entryList(QStringList() << QLatin1String("libvlc.*"), QDir::Files);

        qSort(entryList.begin(), entryList.end(), libGreaterThan);
        foreach (const QString &entry, entryList)
            foundVlcs << path + QLatin1Char('/') + entry;
    }

    return foundVlcs;
#elif defined(Q_OS_WIN)
    // Read VLC version and installation directory from Windows registry
    QSettings settings(QSettings::SystemScope, "VideoLAN", "VLC");
    QString vlcVersion = settings.value("Version").toString();
    QString vlcInstallDir = settings.value("InstallDir").toString();
    if (vlcVersion.startsWith("1.0") && !vlcInstallDir.isEmpty()) {
        paths << vlcInstallDir + QLatin1Char('\\') + "libvlc.dll";
        return paths;
    } else {
        return QStringList();
    }
#endif
}

static QLibrary *vlcLibrary = 0;

QString vlcPath()
{
    static QString path;
    if (!path.isEmpty()) {
        return path;
    }

    vlcLibrary = new QLibrary();
    QStringList paths = findAllLibVlc();
    foreach(path, paths) {
        vlcLibrary->setFileName(path);

        if (vlcLibrary->resolve("libvlc_exception_init")) {
            return path;
        } else {
            qDebug("Cannot resolve the symbol or load VLC library");
        }
        qWarning() << vlcLibrary->errorString();
    }

    vlcUnload();

    return QString();
}

void vlcUnload()
{
    vlcLibrary->unload();
    delete vlcLibrary;
    vlcLibrary = 0;
}

}
} // Namespace Phonon::VLC
