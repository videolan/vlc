/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "colorcontext.hpp"
#include "systempalette.hpp"

ColorProperty::ColorProperty(const ColorContext* context, int section)
    : m_context(context)
    , m_section(section)
{}

QColor ColorProperty::primary() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Primary);
}

QColor ColorProperty::secondary() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Secondary);
}

QColor ColorProperty::highlight() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Highlight);
}

QColor ColorProperty::link() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Link);
}

QColor ColorProperty::positive() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Positive);
}

QColor ColorProperty::neutral() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Neutral);
}

QColor ColorProperty::negative() const
{
    return m_context->getColor(
        static_cast<ColorContext::ColorSection>(m_section),
        ColorContext::Negative);
}


//ColorContext

ColorContext::ColorContext(QObject* parent)
    : QObject(parent)
{
}

void ColorContext::classBegin()
{
}

void ColorContext::componentComplete()
{
    QQuickItem* contextParent = qobject_cast<QQuickItem*>(QObject::parent());
    if (!contextParent)
        return;

    connect(contextParent, &QQuickItem::parentChanged, this, &ColorContext::onParentChanged);

    onParentChanged();

    //set ourself as the parent of our child contexts
    std::function<void (QQuickItem*)>  setParentContextInChildrens;
    setParentContextInChildrens = [&](QQuickItem* item) -> void {
        auto colorContext = item->findChild<ColorContext*>({}, Qt::FindDirectChildrenOnly);
        if (colorContext)
        {
            colorContext->setParentContext(this);
            return;
        }

        for (QQuickItem* child: item->childItems())
            setParentContextInChildrens(child);
    };


    for (QQuickItem* child: contextParent->childItems())
        setParentContextInChildrens(child);
}


void ColorContext::onParentChanged()
{
    QQuickItem* contextParent = qobject_cast<QQuickItem*>(QObject::parent());
    if (! contextParent)
        return;

    for (
        //start looking on our parent's parent, our parent's ColorContext is self
        QQuickItem* parent = contextParent->parentItem();
        parent != nullptr;
        parent = parent->parentItem()
    )
    {
        ColorContext* parentCtx = parent->findChild<ColorContext*>({}, Qt::FindDirectChildrenOnly);
        if (parentCtx)
        {
            setParentContext(parentCtx);
            return;
        }
    }
    //no parent context found, reset our inherited properties
    setParentContext(nullptr);
}

void ColorContext::setParentContext(ColorContext* parentContext)
{
    if (m_parentContext) {
        disconnect(m_parentContext, nullptr, this, nullptr);
    }

    m_parentContext = parentContext;

    if (!m_hasExplicitPalette)
    {
        if (m_parentContext)
        {
            connect(m_parentContext, &ColorContext::paletteChanged, this, &ColorContext::setInheritedPalette);
            setInheritedPalette(m_parentContext->m_palette);
        }
        else
            setInheritedPalette(nullptr);
    }

    if (!m_hasExplicitColorSet)
    {
        if (m_parentContext)
        {
            connect(m_parentContext, &ColorContext::colorSetChanged, this, &ColorContext::setInheritedColorSet);
            setInheritedColorSet(m_parentContext->m_colorSet);
        }
        else
            setInheritedColorSet(ColorContext::ColorSetUndefined);
    }

    if (!m_hasExplicitState)
    {
        if (m_parentContext)
        {
            connect(m_parentContext, &ColorContext::sharedStateChanged, this, &ColorContext::onInheritedStateChanged);
            setInheritedState(m_parentContext->m_state);
        }
        else
        {
            std::shared_ptr<ColorContextState> nullstate{};
            setInheritedState(nullstate);
        }
    }
}

bool ColorContext::setInheritedPalette(SystemPalette* palette)
{
    if (m_palette == palette)
        return false;
    if (m_palette)
    {
        disconnect(m_palette, &SystemPalette::sourceChanged, this, &ColorContext::colorsChanged);
        disconnect(m_palette, &SystemPalette::paletteChanged, this, &ColorContext::colorsChanged);
    }

    m_palette = palette;

    if (m_palette)
    {
        connect(m_palette, &SystemPalette::sourceChanged, this, &ColorContext::colorsChanged);
        connect(m_palette, &SystemPalette::paletteChanged, this, &ColorContext::colorsChanged);
    }
    else
        m_initialized = false;

    if (m_initialized)
        emit colorsChanged();
    else
    {
        if (m_palette && m_colorSet != ColorContext::ColorSetUndefined)
        {
            m_initialized = true;
            emit colorsChanged();
            emit initializedChanged();
        }
    }

    emit paletteChanged(m_palette);
    return true;
}

void ColorContext::setPalette(SystemPalette* palette)
{
    if (!m_hasExplicitPalette && m_parentContext)
        disconnect(m_parentContext, &ColorContext::paletteChanged, this, &ColorContext::setInheritedPalette);

    m_hasExplicitPalette = true;
    setInheritedPalette(palette);
}

bool ColorContext::setInheritedColorSet(ColorSet colorSet)
{
    if (m_colorSet == colorSet)
        return false;

    m_colorSet = colorSet;

    if (m_colorSet == ColorContext::ColorSetUndefined)
        m_initialized = false;

    if (m_initialized)
        emit colorsChanged();
    else
    {
        if (m_palette && m_colorSet != ColorContext::ColorSetUndefined)
        {
            m_initialized = true;
            emit colorsChanged();
            emit initializedChanged();
        }
    }

    emit colorSetChanged(m_colorSet);
    return true;
}

void ColorContext::setColorSet(ColorSet colorSet)
{
    if (!m_hasExplicitColorSet && m_parentContext)
        disconnect(m_parentContext, &ColorContext::colorSetChanged, this, &ColorContext::setInheritedColorSet);

    m_hasExplicitColorSet = true;
    setInheritedColorSet(colorSet);
}

bool ColorContext::setInheritedState(std::shared_ptr<ColorContextState>& state)
{
    if (m_state == state)
        return false;

    //disconnect implicit state
    if (m_state)
        disconnect(m_state.get(), nullptr, this, nullptr);

    m_state = state;
    if (m_state)
    {
        connect(m_state.get(), &ColorContextState::colorsChanged, this, &ColorContext::colorsChanged);
        connect(m_state.get(), &ColorContextState::enabledChanged, this, &ColorContext::enabledChanged);
        connect(m_state.get(), &ColorContextState::focusedChanged, this, &ColorContext::focusedChanged);
        connect(m_state.get(), &ColorContextState::hoveredChanged, this, &ColorContext::hoveredChanged);
        connect(m_state.get(), &ColorContextState::pressedChanged, this, &ColorContext::pressedChanged);
    }

    emit sharedStateChanged({});
    if (m_initialized)
        emit colorsChanged();
    return true;
}

void ColorContext::onInheritedStateChanged()
{
    assert(m_parentContext);
    assert(!m_hasExplicitState);
    setInheritedState(m_parentContext->m_state);
}

void ColorContext::ensureExplicitState()
{
    if (!m_hasExplicitState)
    {
        if (m_parentContext)
            disconnect(m_parentContext, &ColorContext::sharedStateChanged, this, &ColorContext::onInheritedStateChanged);

        auto state = std::make_shared<ColorContextState>();
        m_hasExplicitState = true;
        setInheritedState(state);
    }
}

void ColorContext::setEnabled(bool enabled)
{
    ensureExplicitState();

    if (m_state->m_enabled == enabled)
        return;
    m_state->m_enabled = enabled;
    m_state->updateState();
    emit m_state->enabledChanged();
}

void ColorContext::setHovered(bool hovered)
{
    ensureExplicitState();

    if (m_state->m_hovered == hovered)
        return;
    m_state->m_hovered = hovered;
    m_state->updateState();
    emit m_state->hoveredChanged();
}


void ColorContext::setFocused(bool focused)
{
    ensureExplicitState();

    if (m_state->m_focused == focused)
        return;
    m_state->m_focused = focused;
    m_state->updateState();
    emit m_state->focusedChanged();
}

void ColorContext::setPressed(bool pressed)
{
    ensureExplicitState();
    if (m_state->m_pressed == pressed)
        return;
    m_state->m_pressed = pressed;
    m_state->updateState();
    emit m_state->pressedChanged();
}

void ColorContextState::updateState()
{
    ColorContext::ColorState state = ColorContext::Normal;
    if (!m_enabled)
        state = ColorContext::Disabled;
    else if (m_pressed)
        state = ColorContext::Pressed;
    else if (m_focused)
        state = ColorContext::Focused;
    else if (m_hovered)
        state = ColorContext::Hovered;

    if (state == m_state)
        return;

    m_state = state;
    emit colorsChanged();
}

QColor ColorContext::getColor(ColorSection section, ColorName color) const
{
    if (!m_palette)
        return Qt::magenta;

    return m_palette->getColor(m_colorSet, section,  color, m_state ? m_state->m_state : ColorContext::Normal);
}

bool ColorContext::enabled() const { return !m_state || m_state->m_enabled; }

bool ColorContext::focused() const { return m_state && m_state->m_focused; }

bool ColorContext::hovered() const { return m_state && m_state->m_hovered; }

bool ColorContext::pressed() const { return m_state && m_state->m_pressed; }

QColor ColorContext::visualFocus() const
{
    return getColor(Decoration, VisualFocus);
}

QColor ColorContext::border() const
{
    return getColor(Decoration, Border);
}

QColor ColorContext::separator() const
{
    return getColor(Decoration, Separator);
}

QColor ColorContext::shadow() const
{
    return getColor(Decoration, Shadow);
}

QColor ColorContext::accent() const
{
    return getColor(Decoration, Accent);
}
