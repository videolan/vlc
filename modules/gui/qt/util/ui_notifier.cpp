/*****************************************************************************
 * Copyright (C) 2025 the VideoLAN team
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

#include "ui_notifier.hpp"

#include "maininterface/mainctx.hpp"

UINotifier::UINotifier(MainCtx *ctx, QObject *parent)
    : QObject {parent}
    , m_ctx {ctx}
{
    setupNotifications();
}

void UINotifier::setupNotifications()
{
    connect(m_ctx, &MainCtx::intfScaleFactorChanged, this, [this]()
    {
        const QString scale = QString::number(m_ctx->getIntfScaleFactor() * 100);
        emit showNotification(Scale
                              , qtr("Scale: %1%").arg(scale));
    });

    connect(m_ctx, &MainCtx::minimalViewChanged, this, [this]()
    {
        if (m_ctx->isMinimalView())
            emit showNotification(MinimalView
                                  , qtr("Minimal View"));
    });
}
