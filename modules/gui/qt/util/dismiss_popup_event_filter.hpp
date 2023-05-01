/*
    SPDX-FileCopyrightText: 2008 Aaron Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2013 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2013 Ivan Cukic <ivan.cukic@kde.org>
    SPDX-FileCopyrightText: 2013 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/*
    This file is part of the KDE Plasma project.
    Origin: plasma-workspace/shell/shellcorona.cpp
*/

#ifndef DISMISSPOPUPEVENTFILTER_HPP
#define DISMISSPOPUPEVENTFILTER_HPP

#include <QObject>

/**
 * @internal
 *
 * The DismissPopupEventFilter class monitors mouse button press events and
 * when needed dismisses the active popup widget.
 *
 * plasmashell uses both QtQuick and QtWidgets under a single roof, QtQuick is
 * used for most of things, while QtWidgets is used for things such as context
 * menus, etc.
 *
 * If user clicks outside a popup window, it's expected that the popup window
 * will be closed.  On X11, it's achieved by establishing both a keyboard grab
 * and a pointer grab. But on Wayland, you can't grab keyboard or pointer. If
 * user clicks a surface of another app, the compositor will dismiss the popup
 * surface.  However, if user clicks some surface of the same application, the
 * popup surface won't be dismissed, it's up to the application to decide
 * whether the popup must be closed. In 99% cases, it must.
 *
 * Qt has some code that dismisses the active popup widget if another window
 * of the same app has been clicked. But, that code works only if the
 * application uses solely Qt widgets. See QTBUG-83972. For plasma it doesn't
 * work, because as we said previously, it uses both Qt Quick and Qt Widgets.
 *
 * Ideally, this bug needs to be fixed upstream, but given that it'll involve
 * major changes in Qt, the chances of it being fixed any time soon are slim.
 *
 * In order to work around the popup dismissal bug, we install an event filter
 * that monitors Qt::MouseButtonPress events. If it happens that user has
 * clicked outside an active popup widget, that popup will be closed. This
 * event filter is not needed on X11!
 */
class DismissPopupEventFilter : public QObject
{
    Q_OBJECT

public:
    explicit DismissPopupEventFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_filterMouseEvents = false;

};

#endif // DISMISSPOPUPEVENTFILTER_HPP
