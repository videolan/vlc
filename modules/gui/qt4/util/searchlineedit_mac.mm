/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
****************************************************************************/

#include "searchlineedit.hpp"

#ifdef Q_OS_MAC

#include <Cocoa/Cocoa.h>

//![0]
SearchLineEdit::SearchLineEdit(QWidget *parent)
    : QMacCocoaViewContainer(0, parent)
{
    // Many Cocoa objects create temporary autorelease objects,
    // so create a pool to catch them.
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Create the NSSearchField, set it on the QCocoaViewContainer.
    NSSearchField *search = [[NSSearchField alloc] init];
    setCocoaView(search);
    [[search cell] setPlaceholderString:@"Search..."];

    // Use a Qt menu for the search field menu.
    // NSMenu *nsMenu = searchMenu->macMenu(0);
    // [[search cell] setSearchMenuTemplate:nsMenu];

    // Release our reference, since our super class takes ownership and we
    // don't need it anymore.
    [search release];

    // Clean up our pool as we no longer need it.
    [pool release];
}
//![0]

#endif // Q_OS_MAC
