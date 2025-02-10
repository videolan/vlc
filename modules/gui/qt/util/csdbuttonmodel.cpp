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


#include "csdbuttonmodel.hpp"

#include "maininterface/mainctx.hpp"

#include <QWindow>

CSDButton::CSDButton(ButtonType type, QObject *parent)
    : QObject {parent}
    , m_type {type}
{
}

bool CSDButton::showHovered() const
{
    return m_showHovered;
}

void CSDButton::setShowHovered(bool newShowHovered)
{
    if (m_showHovered == newShowHovered)
        return;
    m_showHovered = newShowHovered;
    emit showHoveredChanged();
}

CSDButton::ButtonType CSDButton::type() const
{
    return m_type;
}

const QRect &CSDButton::rect() const
{
    return m_rect;
}

void CSDButton::setRect(const QRect &newRect)
{
    if (m_rect == newRect)
        return;
    m_rect = newRect;
    emit rectChanged();
}

bool CSDButton::externalPressed() const
{
    return m_externalPressed;
}

void CSDButton::setExternalPressed()
{
    if (m_externalPressed)
        return;
    m_externalPressed = true;
    emit externalPressedChanged();
}

void CSDButton::unsetExternalPressed()
{
    if (!m_externalPressed)
        return;
    m_externalPressed = false;
    emit externalPressedChanged();
};

void CSDButton::externalPress()
{
    setExternalPressed();
}

void CSDButton::externalRelease()
{
    if (!m_externalPressed)
        return;
    unsetExternalPressed();
    click();
}

void CSDButton::click()
{
    emit clicked();
}

void CSDButton::doubleClick()
{
    emit doubleClicked();
}

CSDButtonModel::CSDButtonModel(MainCtx *mainCtx, QObject *parent)
    : QObject {parent}
    , m_mainCtx {mainCtx}
{
    auto newButton = [this](CSDButton::ButtonType type, void (CSDButtonModel::* onClick)())
    {
        auto button = new CSDButton(type, this);
        connect(button, &CSDButton::clicked, this, onClick);
        m_windowCSDButtons.append(button);
    };

    newButton(CSDButton::Minimize, &CSDButtonModel::minimizeButtonClicked);
    newButton(CSDButton::MaximizeRestore, &CSDButtonModel::maximizeRestoreButtonClicked);
    newButton(CSDButton::Close, &CSDButtonModel::closeButtonClicked);
}

const QList<CSDButton *> &CSDButtonModel::windowCSDButtons() const
{
    return m_windowCSDButtons;
}


void CSDButtonModel::minimizeButtonClicked()
{
    emit m_mainCtx->requestInterfaceMinimized();
}

void CSDButtonModel::maximizeRestoreButtonClicked()
{
    const auto visibility = m_mainCtx->intfMainWindow()->visibility();
    if (visibility == QWindow::Maximized)
        emit m_mainCtx->requestInterfaceNormal();
    else
        emit m_mainCtx->requestInterfaceMaximized();
}

void CSDButtonModel::closeButtonClicked()
{
    m_mainCtx->intfMainWindow()->close();
}
