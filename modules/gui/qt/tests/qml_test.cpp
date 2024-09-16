/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#include <QtQuickTest>
#include "qml_test.hpp"

// not much right now, type registration & initialisation may be required later on
// https://doc.qt.io/qt-5/qtquicktest-index.html#executing-c-before-qml-tests

void Setup::qmlEngineAvailable(QQmlEngine *engine)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    engine->addImportPath(":/qt/qml");
#else
    (void)engine;
#endif
}

int main(int argc, char **argv)
{
    QTEST_SET_MAIN_SOURCE_PATH

    Q_INIT_RESOURCE( util_assets );
#ifdef QT_USE_QMLCACHEGEN
    Q_INIT_RESOURCE( util_cachegen );
#endif

    //run tests offscreen as the CI doesn't have a desktop environment
    qputenv("QT_QPA_PLATFORM", "offscreen");
    Setup setup;
    return quick_test_main_with_setup(argc, argv, "qml_test", QUICK_TEST_SOURCE_DIR, &setup);
}
