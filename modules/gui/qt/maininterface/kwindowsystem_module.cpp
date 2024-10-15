/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#include "windoweffects_module.hpp"

#include <QRegion>

#include <vlc_plugin.h>

#include <KWindowEffects>

static bool isEffectAvailable(const QWindow*, const WindowEffectsModule::Effect effect)
{
    KWindowEffects::Effect kWindowEffect;

    switch (effect)
    {
    case WindowEffectsModule::BlurBehind:
        kWindowEffect = KWindowEffects::BlurBehind;
        break;
    default:
        return false;
    };

    return KWindowEffects::isEffectAvailable(kWindowEffect);
}

static void setBlurBehind(QWindow* const window, const bool enable = true, const QRegion& region = {})
{
    KWindowEffects::enableBlurBehind(window, enable, region);
}

static int Open(vlc_object_t* const p_this)
{
    assert(p_this);

    // If none of the effects are available,
    // it probably means that KWindowEffects
    // does not support the environment.
    // In that case, simply fail here,
    // so that another potentially compatible
    // module can be loaded instead:
    if (!isEffectAvailable(nullptr, WindowEffectsModule::BlurBehind))
        return VLC_EGENERIC;

    const auto obj = reinterpret_cast<WindowEffectsModule*>(p_this);

    obj->setBlurBehind = setBlurBehind;
    obj->isEffectAvailable = isEffectAvailable;

    return VLC_SUCCESS;
}

vlc_module_begin()
    add_shortcut("QtKWindowSystem")
    set_description("Provides window effects through KWindowSystem.")
    set_capability("qtwindoweffects", 10)
    set_callback(Open)
vlc_module_end()
