/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#include "mainctx_submodels.hpp"
#include "mainctx.hpp"


SidePanelCtx::SidePanelCtx(MainCtx* ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    assert(m_ctx);
    connect(
        m_ctx, &MainCtx::intfScaleFactorChanged,
        this, [this]() {
            emit widthChanged(width());
        });
}

double SidePanelCtx::width() const {
    return m_ctx->dp(m_width);
}

void SidePanelCtx::setDocked(bool value) {
    if (value == m_docked)
        return;
    m_docked = value;
    emit dockedChanged(value);
}

void SidePanelCtx::setVisible(bool value) {
    if (value == m_visible)
        return;
    m_visible = value;
    emit visibleChanged(value);
}

void SidePanelCtx::setWidth(double value)
{
    double unscaledValue = value / m_ctx->getIntfScaleFactor();
    if (qFuzzyCompare(m_width, unscaledValue))
        return;
    m_width = unscaledValue;
    emit widthChanged(width());
}
