/*****************************************************************************
 * Copyright (C) 2022 the VideoLAN team
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#ifndef CSDBUTTONCONTROLLER_HPP
#define CSDBUTTONCONTROLLER_HPP

#include <QObject>
#include <QRect>

class CSDButton : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ButtonType type READ type CONSTANT)
    Q_PROPERTY(bool showHovered READ showHovered WRITE setShowHovered NOTIFY showHoveredChanged)
    Q_PROPERTY(QRect rect READ rect WRITE setRect NOTIFY rectChanged)

public:
    enum ButtonType
    {
        Minimize,
        MaximizeRestore,
        Close,

        TypeCount
    };

    Q_ENUM(ButtonType);

    CSDButton(ButtonType type, QObject *parent);

    ButtonType type() const;

    // 'showHovered' is hint for UI to the show this
    // button as in 'hovered' state
    // used by implmentation incase custom event handling is required for CSD
    bool showHovered() const;
    void setShowHovered(bool newShowHovered);

    // 'rect' is location of the button in the UI
    // may be used by implementation to relay the information
    // to OS such as to show snaplay out menu on Windows 11
    const QRect &rect() const;
    void setRect(const QRect &newRect);

public slots:
    // signals to perfrom action associated with button
    void click();

signals:
    void showHoveredChanged();
    void rectChanged();
    void clicked();

private:
    const ButtonType m_type;
    bool m_showHovered = false;
    QRect m_rect;
};


class MainCtx;

class CSDButtonModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<CSDButton *> windowCSDButtons READ windowCSDButtons CONSTANT)

public:
    CSDButtonModel(MainCtx *mainCtx, QObject *parent = nullptr);

    QList<CSDButton *> windowCSDButtons() const;

private slots:
    void minimizeButtonClicked();
    void maximizeRestoreButtonClicked();
    void closeButtonClicked();

private:
    MainCtx *m_mainCtx;
    QList<CSDButton *> m_windowCSDButtons;
};


Q_DECLARE_METATYPE(CSDButton *)

#endif // CSDBUTTONCONTROLLER_HPP
