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

// original code from the Chromium project

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <memory>


#include "scoped_gobject.h"
#include "gtk_util.h"
#include "gtk_compat.h"

#include "nav_button_provider_gtk.h"

namespace gtk {

namespace {

struct NavButtonIcon {
    // Used on Gtk3.
    ScopedGObject<GdkPixbuf> pixbuf;

#if 0 //GTK4
    // Used on Gtk4.
    ScopedGObject<GdkTexture> texture;
#endif
};

// gtkheaderbar.c uses GTK_ICON_SIZE_MENU, which is 16px.
const int kNavButtonIconSize = 16;

// Specified in GtkHeaderBar spec.
const int kHeaderSpacing = 6;

const char* ButtonStyleClassFromButtonType(
        vlc_qt_theme_csd_button_type type) {
    switch (type) {
    case VLC_QT_THEME_BUTTON_MINIMIZE:
        return "minimize";
    case VLC_QT_THEME_BUTTON_MAXIMIZE:
    case VLC_QT_THEME_BUTTON_RESTORE:
        return "maximize";
    case VLC_QT_THEME_BUTTON_CLOSE:
        return "close";
    default:
        assert("unreachable" == 0);
        return "";
    }
}

GtkStateFlags GtkStateFlagsFromButtonState(vlc_qt_theme_csd_button_state state) {
    switch (state) {
    case VLC_QT_THEME_BUTTON_STATE_NORMAL:
        return GTK_STATE_FLAG_NORMAL;
    case VLC_QT_THEME_BUTTON_STATE_HOVERED:
        return GTK_STATE_FLAG_PRELIGHT;
    case VLC_QT_THEME_BUTTON_STATE_PRESSED:
        return static_cast<GtkStateFlags>(GTK_STATE_FLAG_PRELIGHT |
                                          GTK_STATE_FLAG_ACTIVE);
    case VLC_QT_THEME_BUTTON_STATE_DISABLED:
        return GTK_STATE_FLAG_INSENSITIVE;
    default:
        assert("unreachable" == 0);
        return GTK_STATE_FLAG_NORMAL;
    }
}

const char* IconNameFromButtonType(vlc_qt_theme_csd_button_type type) {
    switch (type) {
    case VLC_QT_THEME_BUTTON_MINIMIZE:
        return "window-minimize-symbolic";
    case VLC_QT_THEME_BUTTON_MAXIMIZE:
        return "window-maximize-symbolic";
    case VLC_QT_THEME_BUTTON_RESTORE:
        return "window-restore-symbolic";
    case VLC_QT_THEME_BUTTON_CLOSE:
        return "window-close-symbolic";
    default:
        assert("unreachable" == 0);
        return "";
    }
}

MySize LoadNavButtonIcon(
        vlc_qt_theme_csd_button_type type,
        GtkStyleContext* button_context,
        int scale,
        NavButtonIcon* icon = nullptr) {
    const char* icon_name = IconNameFromButtonType(type);
#if 0 //GTK4
    if (!GtkCheckVersion(4)) {
#endif

        auto icon_info = TakeGObject(gtk_icon_theme_lookup_icon_for_scale(
                                         GetDefaultIconTheme(), icon_name, kNavButtonIconSize, scale,
                                         static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_USE_BUILTIN |
                                                                         GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                                                         )));
        auto icon_pixbuf = TakeGObject(gtk_icon_info_load_symbolic_for_context(
                                           icon_info, button_context, nullptr, nullptr));
        MySize size{gdk_pixbuf_get_width(icon_pixbuf),
                    gdk_pixbuf_get_height(icon_pixbuf)};
        if (icon)
            icon->pixbuf = std::move(icon_pixbuf);
        return size;
#if 0 //GTK4
    }
    auto icon_paintable = Gtk4IconThemeLookupIcon(
                GetDefaultIconTheme(), icon_name, nullptr, kNavButtonIconSize, scale,
                GTK_TEXT_DIR_NONE, static_cast<GtkIconLookupFlags>(0));
    auto* paintable =
            GlibCast<GdkPaintable>(icon_paintable.get(), gdk_paintable_get_type());
    int width = scale * gdk_paintable_get_intrinsic_width(paintable);
    int height = scale * gdk_paintable_get_intrinsic_height(paintable);
    if (icon) {
        auto* snapshot = gtk_snapshot_new();
        gdk_paintable_snapshot(paintable, snapshot, width, height);
        auto* node = gtk_snapshot_free_to_node(snapshot);
        GdkTexture* texture = GetTextureFromRenderNode(node);
        size_t nbytes = width * height * sizeof(SkColor);
        SkColor* pixels = reinterpret_cast<SkColor*>(g_malloc(nbytes));
        size_t stride = sizeof(SkColor) * width;
        gdk_texture_download(texture, reinterpret_cast<guchar*>(pixels), stride);
        SkColor fg = GtkStyleContextGetColor(button_context);
        for (int i = 0; i < width * height; ++i)
            pixels[i] = SkColorSetA(fg, SkColorGetA(pixels[i]));
        icon->texture = TakeGObject(
                    gdk_memory_texture_new(width, height, GDK_MEMORY_B8G8R8A8,
                                           g_bytes_new_take(pixels, nbytes), stride));
        gsk_render_node_unref(node);
    }
    return {width, height};
#endif
}

MySize GetMinimumWidgetSize(MySize content_size,
                           GtkStyleContext* content_context,
                           GtkCssContext widget_context) {
    MyRect widget_rect = MyRect({0,0},content_size);
    if (content_context) {
        widget_rect.Inset(-GtkStyleContextGetMargin(content_context));
    }

    int min_width = 0;
    int min_height = 0;
    // On GTK3, get the min size from the CSS directly.
    if (GtkCheckVersion(3, 20) && !GtkCheckVersion(4)) {
        gtk_style_context_get(widget_context, gtk_style_context_get_state(widget_context),
                              "min-width", &min_width,
                              "min-height", &min_height,
                              nullptr);
        widget_rect.set_width(std::max(widget_rect.width(), min_width));
        widget_rect.set_height(std::max(widget_rect.height(), min_height));
    }

    widget_rect.Inset(-GtkStyleContextGetPadding(widget_context));
    widget_rect.Inset(-GtkStyleContextGetBorder(widget_context));

#if 0 //GTK4
    // On GTK4, the CSS properties are hidden, so compute the min size indirectly,
    // which will include the border, margin, and padding.  We can't take this
    // codepath on GTK3 since we only have a widget available in GTK4.
    if (GtkCheckVersion(4)) {
        gtk_widget_measure(widget_context.widget(), GTK_ORIENTATION_HORIZONTAL, -1,
                           &min_width, nullptr, nullptr, nullptr);
        gtk_widget_measure(widget_context.widget(), GTK_ORIENTATION_VERTICAL, -1,
                           &min_height, nullptr, nullptr, nullptr);

        // The returned "minimum size" is the drawn size of the widget, which
        // doesn't include the margin.  However, GTK includes this size in its
        // calculation. So remove the margin, recompute the min size, then add it
        // back.
        auto margin = GtkStyleContextGetMargin(widget_context);
        widget_rect.Inset(-margin);
        widget_rect.set_width(std::max(widget_rect.width(), min_width));
        widget_rect.set_height(std::max(widget_rect.height(), min_height));
        widget_rect.Inset(margin);
    }
#endif

    return widget_rect.size();
}

GtkCssContext CreateHeaderContext(bool maximized) {
    std::string window_selector = "GtkWindow#window.background.csd";
    if (maximized)
        window_selector += ".maximized";
    return AppendCssNodeToStyleContext(
                AppendCssNodeToStyleContext({}, window_selector),
                "GtkHeaderBar#headerbar.header-bar.titlebar");
}

GtkCssContext CreateWindowControlsContext(bool maximized) {
    return AppendCssNodeToStyleContext(CreateHeaderContext(maximized),
                                       "#windowcontrols");
}

void CalculateUnscaledButtonSize(
        vlc_qt_theme_csd_button_type type,
        bool maximized,
        MySize* button_size,
        MyInset* button_margin) {
    // views::ImageButton expects the images for each state to be of the
    // same size, but GTK can, in general, use a differnetly-sized
    // button for each state.  For this reason, render buttons for all
    // states at the size of a GTK_STATE_FLAG_NORMAL button.
    auto button_context = AppendCssNodeToStyleContext(
                CreateWindowControlsContext(maximized),
                "GtkButton#button.titlebutton." +
                std::string(ButtonStyleClassFromButtonType(type)));

    auto icon_size = LoadNavButtonIcon(type, button_context, 1);

    auto image_context =
            AppendCssNodeToStyleContext(button_context, "GtkImage#image");
    MySize image_size =
            GetMinimumWidgetSize(icon_size, nullptr, image_context);

    *button_size =
            GetMinimumWidgetSize(image_size, image_context, button_context);

    *button_margin = GtkStyleContextGetMargin(button_context);
}


class NavButtonImageSource {
public:
    NavButtonImageSource(vlc_qt_theme_csd_button_type type,
                         vlc_qt_theme_csd_button_state state,
                         bool maximized,
                         bool active,
                         MySize button_size)
        : type_(type)
        , state_(state)
        , maximized_(maximized)
        , active_(active)
        , button_size_(button_size)
    {}

    ~NavButtonImageSource() = default;

    VLCPicturePtr GetImageForScale(float scale) {
        // gfx::ImageSkia kindly caches the result of this function, so
        // RenderNavButton() is called at most once for each needed scale
        // factor.  Additionally, buttons in the HOVERED or PRESSED states
        // are not actually rendered until they are needed.
        if (button_size_.IsEmpty())
            return {};

        auto button_context =
                AppendCssNodeToStyleContext(CreateWindowControlsContext(maximized_),
                                            std::string("GtkButton#button.titlebutton.") + ButtonStyleClassFromButtonType(type_));

        GtkStateFlags button_state = GtkStateFlagsFromButtonState(state_);
        if (!active_) {
            button_state =
                    static_cast<GtkStateFlags>(button_state | GTK_STATE_FLAG_BACKDROP);
        }
        gtk_style_context_set_state(button_context, button_state);

        // Gtk header bars usually have the same height in both maximized and
        // restored windows.  But chrome's tabstrip background has a smaller height
        // when maximized.  To prevent buttons from clipping outside of this region,
        // they are scaled down.  However, this is problematic for themes that do
        // not expect this case and use bitmaps for frame buttons (like the Breeze
        // theme).  When the background-size is set to auto, the background bitmap
        // is not scaled for the (unexpected) smaller button size, and the button's
        // edges appear cut off.  To fix this, manually set the background to scale
        // to the button size when it would have clipped.
        //
        // GTK's "contain" is unlike CSS's "contain".  In CSS, the image would only
        // be downsized when it would have clipped.  In GTK, the image is always
        // scaled to fit the drawing region (preserving aspect ratio).  Only add
        // "contain" if clipping would occur.
        int bg_width = 0;
        int bg_height = 0;
#if 0 //GTK4
        if (GtkCheckVersion(4)) {
            auto* snapshot = gtk_snapshot_new();
            gtk_snapshot_render_background(snapshot, button_context, 0, 0,
                                           button_size_.width(),
                                           button_size_.height());
            if (auto* node = gtk_snapshot_free_to_node(snapshot)) {
                if (GdkTexture* texture = GetTextureFromRenderNode(node)) {
                    bg_width = gdk_texture_get_width(texture);
                    bg_height = gdk_texture_get_height(texture);
                }
                gsk_render_node_unref(node);
            }
        } else
#endif
        {
            cairo_pattern_t* cr_pattern = nullptr;
            cairo_surface_t* cr_surface = nullptr;
            gtk_style_context_get(button_context, gtk_style_context_get_state(button_context),
                        "background-image", &cr_pattern,
                        nullptr);
            if (cr_pattern) {
                cairo_pattern_get_surface(cr_pattern, &cr_surface);
                if (cr_surface &&
                        cairo_surface_get_type(cr_surface) == CAIRO_SURFACE_TYPE_IMAGE) {
                    bg_width = cairo_image_surface_get_width(cr_surface);
                    bg_height = cairo_image_surface_get_height(cr_surface);
                }
                cairo_pattern_destroy(cr_pattern);
            }
        }
        if (bg_width > button_size_.width() || bg_height > button_size_.height()) {
            ApplyCssToContext(button_context,
                              ".titlebutton { background-size: contain; }");
        }

        // Gtk doesn't support fractional scale factors, but chrome does.
        // Rendering the button background and border at a fractional
        // scale factor is easy, since we can adjust the cairo context
        // transform.  But the icon is loaded from a pixbuf, so we pick
        // the next-highest integer scale and manually downsize.
        int pixbuf_scale = scale == static_cast<int>(scale) ? scale : scale + 1;

        auto icon_context = AppendCssNodeToStyleContext(button_context, "GtkImage#image");

        NavButtonIcon icon;
        auto icon_size =
                LoadNavButtonIcon(type_, icon_context, pixbuf_scale, &icon);

        VLCPicturePtr bitmap(
                    VLC_CODEC_ARGB,
                    scale * button_size_.width(),
                    scale * button_size_.height());

        if (!bitmap.get())
            return {};

        CairoSurface surface(bitmap);
        cairo_t* cr = surface.cairo();

        cairo_save(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        cairo_restore(cr);

        cairo_save(cr);
        cairo_scale(cr, scale, scale);
        if (GtkCheckVersion(3, 11, 3) ||
                (button_state & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE))) {
            gtk_render_background(button_context, cr, 0, 0, button_size_.width(),
                                  button_size_.height());
            gtk_render_frame(button_context, cr, 0, 0, button_size_.width(),
                             button_size_.height());
        }
        cairo_restore(cr);

        cairo_save(cr);
        float pixbuf_extra_scale = scale / pixbuf_scale;
        cairo_scale(cr, pixbuf_extra_scale, pixbuf_extra_scale);
        gtk_render_icon(icon_context, cr, icon.pixbuf,
                    ((pixbuf_scale * button_size_.width() - icon_size.width()) / 2),
                    ((pixbuf_scale * button_size_.height() - icon_size.height()) / 2));
        cairo_restore(cr);


        return bitmap;
    }

    bool HasRepresentationAtAllScales() const { return true; }

private:
    vlc_qt_theme_csd_button_type type_;
    vlc_qt_theme_csd_button_state state_;
    bool maximized_;
    bool active_;
    MySize button_size_;
};

}  // namespace

NavButtonProviderGtk::NavButtonProviderGtk() = default;

NavButtonProviderGtk::~NavButtonProviderGtk() = default;

void NavButtonProviderGtk::RedrawImages(int top_area_height,
                                        bool maximized,
                                        bool active) {
    auto header_context = CreateHeaderContext(maximized);

    MyInset header_padding = GtkStyleContextGetPadding(header_context);

    double scale = 1.0f;
    std::map<vlc_qt_theme_csd_button_type, MySize>
            button_sizes;
    std::map<vlc_qt_theme_csd_button_type, MyInset>
            button_margins;
    std::vector<vlc_qt_theme_csd_button_type> display_types{
        VLC_QT_THEME_BUTTON_MINIMIZE,
        maximized ? VLC_QT_THEME_BUTTON_RESTORE
                  : VLC_QT_THEME_BUTTON_MAXIMIZE,
        VLC_QT_THEME_BUTTON_CLOSE,
    };
    for (auto type : display_types) {
        CalculateUnscaledButtonSize(type, maximized, &button_sizes[type],
                                    &button_margins[type]);
        int button_unconstrained_height = button_sizes[type].height() +
                button_margins[type].top() +
                button_margins[type].bottom();

        int needed_height = header_padding.top() + button_unconstrained_height +
                header_padding.bottom();

        if (needed_height > top_area_height)
            scale = std::min(scale, static_cast<double>(top_area_height) / needed_height);
    }

    top_area_spacing_ = MyInset(std::round(scale * header_padding.top()),
                                    std::round(scale * header_padding.left()),
                                    std::round(scale * header_padding.bottom()),
                                    std::round(scale * header_padding.right()));

    inter_button_spacing_ = std::round(scale * kHeaderSpacing);

    for (auto type : display_types) {
        double button_height =
                scale * (button_sizes[type].height() + button_margins[type].top() +
                         button_margins[type].bottom());
        double available_height =
                top_area_height -
                scale * (header_padding.top() + header_padding.bottom());
        double scaled_button_offset = (available_height - button_height) / 2;

        MySize size = button_sizes[type];
        size = MySize(std::round(scale * size.width()),
                     std::round(scale * size.height()));
        MyInset margin = button_margins[type];
        margin =
                MyInset(std::round(scale * (header_padding.top() + margin.top()) +
                                       scaled_button_offset),
                            std::round(scale * margin.left()), 0,
                            std::round(scale * margin.right()));

        button_margins_[type] = margin;

        for (size_t state = 0; state < VLC_QT_THEME_BUTTON_STATE_COUNT; state++) {
            auto buttonState = static_cast<vlc_qt_theme_csd_button_state>(state);
            std::unique_ptr<NavButtonImageSource> source = std::make_unique<NavButtonImageSource>(
                                    type, buttonState,
                                    maximized, active, size);
            button_images_[type][buttonState] = source->GetImageForScale(scale);
        }
    }
}

VLCPicturePtr NavButtonProviderGtk::GetImage(
        vlc_qt_theme_csd_button_type type,
        vlc_qt_theme_csd_button_state state) const {
    auto it = button_images_.find(type);
    assert(it != button_images_.end());
    auto picIt = it->second.find(state);
    assert(picIt != it->second.cend());
    return picIt->second;
}

MyInset NavButtonProviderGtk::GetNavButtonMargin(
        vlc_qt_theme_csd_button_type type) const {
    auto it = button_margins_.find(type);
    assert(it != button_margins_.end());
    return it->second;
}

MyInset NavButtonProviderGtk::GetTopAreaSpacing() const {
    return top_area_spacing_;
}

int NavButtonProviderGtk::GetInterNavButtonSpacing() const {
    return inter_button_spacing_;
}

}  // namespace gtk
