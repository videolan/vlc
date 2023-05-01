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

#include "dismiss_popup_event_filter.hpp"

#include <QMouseEvent>
#include <QWidget>
#include <QWindow>
#include <QApplication>

DismissPopupEventFilter::DismissPopupEventFilter(QObject *parent)
    : QObject{parent}
{

}

bool DismissPopupEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (m_filterMouseEvents) {
            // Eat events until all mouse buttons are released.
            return true;
        }

        QWidget *popup = QApplication::activePopupWidget();
        if (!popup) {
            return false;
        }

        QWindow *window = qobject_cast<QWindow *>(watched);
        if (popup->windowHandle() == window) {
            // The popup window handles mouse events before the widget.
            return false;
        }

        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget) {
            // Let the popup widget handle the mouse press event.
            return false;
        }

        popup->close();
        m_filterMouseEvents = true;
        return true;

    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_filterMouseEvents) {
            // Eat events until all mouse buttons are released.
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->buttons() == Qt::NoButton) {
                m_filterMouseEvents = false;
            }
            return true;
        }
    }

    return false;
}
