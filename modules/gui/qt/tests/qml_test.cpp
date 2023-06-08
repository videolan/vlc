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

// not much right now, type registration & initialisation may be required later on
// https://doc.qt.io/qt-5/qtquicktest-index.html#executing-c-before-qml-tests

int main(int argc, char **argv)
{
    QTEST_SET_MAIN_SOURCE_PATH
    //run tests offscreen as the CI doesn't have a desktop environment
    qputenv("QT_QPA_PLATFORM", "offscreen");
    Q_INIT_RESOURCE(vlc);
    return quick_test_main(argc, argv, "qml_test", QUICK_TEST_SOURCE_DIR);
}
