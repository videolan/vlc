/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include "gtk_compat.h"

namespace gtk {

MyInset GtkStyleContextGetPadding(GtkStyleContext* context)
{
    GtkBorder padding;
    gtk_style_context_get_padding(context, gtk_style_context_get_state(context), &padding);
    return MyInset(padding);
}

MyInset GtkStyleContextGetBorder(GtkStyleContext* context)
{
    GtkBorder border;
    gtk_style_context_get_border(context, gtk_style_context_get_state(context), &border);
    return MyInset(border);
}

MyInset GtkStyleContextGetMargin(GtkStyleContext* context)
{
    GtkBorder margin;
    gtk_style_context_get_margin(context, gtk_style_context_get_state(context), &margin);
    return MyInset(margin);
}

gdouble GdkRBGALightness(GdkRGBA& c1)
{
    return 0.2126 * c1.red + 0.7152 * c1.green + 0.0722 * c1.blue;
}

GdkRGBA GdkRBGABlend(GdkRGBA& c1, GdkRGBA& c2, gdouble blend)
{
    GdkRGBA out;
    out.red = c2.red   + (c1.red   - c2.red)   * blend,
    out.green = c2.green + (c1.green - c2.green) * blend,
    out.blue = c2.blue  + (c1.blue  - c2.blue)  * blend,
    out.alpha = c2.alpha + (c1.alpha - c2.alpha) * blend;
    return out;
}

};
