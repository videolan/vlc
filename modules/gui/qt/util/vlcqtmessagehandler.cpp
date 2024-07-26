/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "vlcqtmessagehandler.hpp"

#include <QtGlobal>
#include <QString>
#include <QLoggingCategory>

#include <vlc_common.h>

static vlc_object_t* g_intf = nullptr;
static bool g_logQtMessages = false;
static QtMessageHandler g_defaultMessageHandler = nullptr;

static void vlcQtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    //application logs from QML should be logged using vlc log system
    //"qml" and "js" are the category used by for console.xxxx
    //"default" is for qDebug(), most of our code logs using msg_Dbg but some parts will use qDebug,
    //usually when they don't have access to the vlc_object. Qt itself uses categorised logger so it
    //should be safe to assume that the default logger is only used by us.
    if (g_logQtMessages
        || qstrcmp(context.category, "default") == 0
        || qstrcmp(context.category, "qml") == 0
        || qstrcmp(context.category, "js") == 0)
    {
        const char *file = context.file ? context.file : "";
        const char *function = context.function ? context.function : "";
        int vlcLogLevel = 0;
        switch (type)
        {
        case QtDebugMsg:
            vlcLogLevel = VLC_MSG_DBG;
            break;
        case QtWarningMsg:
            vlcLogLevel = VLC_MSG_WARN;
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            vlcLogLevel = VLC_MSG_ERR;
            break;
        case QtInfoMsg:
            vlcLogLevel = VLC_MSG_INFO;
            break;
        default:
            vlcLogLevel = VLC_MSG_DBG;
        }

        vlc_object_Log(g_intf, vlcLogLevel, vlc_module_name, file, context.line, function, "(%s) %s", context.category,  msg.toUtf8().constData());
    }
    else
    {
        g_defaultMessageHandler(type, context, msg);
    }
};

void setupVlcQtMessageHandler(vlc_object_t* p_intf)
{
    assert(g_intf == nullptr);
    assert(p_intf != nullptr);

    g_intf = p_intf;
    g_logQtMessages = var_InheritBool(p_intf, "qt-verbose");

    g_defaultMessageHandler = qInstallMessageHandler(vlcQtMessageHandler);
    qSetMessagePattern("[qt] (%{category}) %{message}");

    QString filterRules;

    if (g_logQtMessages)
    {
        filterRules = QStringLiteral("*=true\n"
                                    "qt.*.debug=false\n" /* Qt's own debug messages are way too much verbose */
                                    "qt.widgets.painting=false\n" /* Not necessary */);
    }

    QLoggingCategory::setFilterRules(filterRules);
}

void cleanupVlcQtMessageHandler()
{
    assert(g_intf != nullptr);

    qInstallMessageHandler(nullptr);
    g_defaultMessageHandler = nullptr;
    g_intf = nullptr;
}
