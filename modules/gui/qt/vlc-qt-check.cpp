/*****************************************************************************
 * vlc-qt-check.cpp: run-time Qt availability test
 ****************************************************************************
 * Copyright © 2018 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <QApplication>

static void messageOutput(QtMsgType type, const QMessageLogContext &,
                          const QString &)
{
    if (type == QtFatalMsg)
        exit(1);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageOutput);
    QApplication app(argc, argv);
}
