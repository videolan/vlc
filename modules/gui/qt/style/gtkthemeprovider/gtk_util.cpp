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

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtk_util.h"
#include "gtk_compat.h"
#include <gdk/gdk.h>

#include <locale.h>
#include <stddef.h>

#include <memory>
#include <cmath>

namespace gtk {

namespace {



const char kAuraTransientParent[] = "aura-transient-parent";

GtkCssContext AppendCssNodeToStyleContextImpl(
        GtkCssContext context,
        GType gtype,
        const std::string& name,
        const std::string& object_name,
        const std::vector<std::string>& classes,
        GtkStateFlags state,
        float scale) {
#if 0 //FIXME GTK4
    if (GtkCheckVersion(4)) {
        // GTK_TYPE_BOX is used instead of GTK_TYPE_WIDGET because:
        // 1. Widgets are abstract and cannot be created directly.
        // 2. The widget must be a container type so that it unrefs child widgets
        //    on destruction.
        auto* widget_object = object_name.empty()
                ? g_object_new(GTK_TYPE_BOX, nullptr)
                : g_object_new(GTK_TYPE_BOX, "css-name",
                               object_name.c_str(), nullptr);
        auto widget = TakeGObject(GTK_WIDGET(widget_object));

        if (!name.empty())
            gtk_widget_set_name(widget, name.c_str());

        std::vector<const char*> css_classes;
        css_classes.reserve(classes.size() + 1);
        for (const auto& css_class : classes)
            css_classes.push_back(css_class.c_str());
        css_classes.push_back(nullptr);
        gtk_widget_set_css_classes(widget, css_classes.data());

        gtk_widget_set_state_flags(widget, state, false);

        if (context)
            gtk_widget_set_parent(widget, context.widget());

        gtk_style_context_set_scale(gtk_widget_get_style_context(widget), scale);

        return GtkCssContext(widget, context ? context.root() : widget);
    } else
#endif
    {
        GtkWidgetPath* path =
                context ? gtk_widget_path_copy(gtk_style_context_get_path(context))
                        : gtk_widget_path_new();
        gtk_widget_path_append_type(path, gtype);

        if (!object_name.empty()) {
            if (GtkCheckVersion(3, 20))
                gtk_widget_path_iter_set_object_name(path, -1, object_name.c_str());
            else
                gtk_widget_path_iter_add_class(path, -1, object_name.c_str());
        }

        if (!name.empty())
            gtk_widget_path_iter_set_name(path, -1, name.c_str());

        for (const auto& css_class : classes)
            gtk_widget_path_iter_add_class(path, -1, css_class.c_str());

        if (GtkCheckVersion(3, 14))
            gtk_widget_path_iter_set_state(path, -1, state);

        GtkCssContext child_context(TakeGObject(gtk_style_context_new()));
        gtk_style_context_set_path(child_context, path);
        if (GtkCheckVersion(3, 14)) {
            gtk_style_context_set_state(child_context, state);
        } else {
            GtkStateFlags child_state = state;
            if (context) {
                child_state = static_cast<GtkStateFlags>(
                            child_state | gtk_style_context_get_state(context));
            }
            gtk_style_context_set_state(child_context, child_state);
        }

        gtk_style_context_set_scale(child_context, scale);

        gtk_style_context_set_parent(child_context, context);

        gtk_widget_path_unref(path);
        return GtkCssContext(child_context);
    }
}

GtkWidget* CreateDummyWindow() {
    GtkWidget* window =   gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(window);
    return window;
}

}  // namespace

bool GtkCheckVersion(uint32_t major, uint32_t minor, uint32_t micro) {
    if (major < gtk_get_major_version())
        return true;
    else if (major > gtk_get_major_version())
        return false;

    if (minor < gtk_get_minor_version())
        return true;
    else if (minor > gtk_get_minor_version())
        return false;

    return micro <= gtk_get_micro_version();
}

const char* GtkCssMenu() {
    return GtkCheckVersion(4) ? "#popover.background.menu #contents"
                              : "GtkMenu#menu";
}

const char* GtkCssMenuItem() {
    return GtkCheckVersion(4) ? "#modelbutton.flat" : "GtkMenuItem#menuitem";
}

const char* GtkCssMenuScrollbar() {
    return GtkCheckVersion(4) ? "#scrollbar #range"
                              : "GtkScrollbar#scrollbar #trough";
}

bool GtkInitFromCommandLine(int* argc, char** argv) {
    // Callers should have already called setlocale(LC_ALL, "") and
    // setlocale(LC_NUMERIC, "C") by now. Chrome does this in
    // service_manager::Main.
    assert(strcmp(setlocale(LC_NUMERIC, nullptr), "C") == 0);
    // This prevents GTK from calling setlocale(LC_ALL, ""), which potentially
    // overwrites the LC_NUMERIC locale to something other than "C".
    gtk_disable_setlocale();
    return gtk_init_check(argc, &argv);
}


CairoSurface::CairoSurface(VLCPicturePtr& bitmap) 
{
    picture_t* pic = bitmap.get();
    surface_ = (cairo_image_surface_create_for_data(
                   static_cast<unsigned char*>(pic->p[0].p_pixels),
                   CAIRO_FORMAT_ARGB32,
                   pic->format.i_visible_width,
                   pic->format.i_visible_height,
                   pic->p[0].i_pitch));
    cairo_ = cairo_create(surface_);
}

CairoSurface::CairoSurface(const MySize& size)
    : surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                          size.width(),
                                          size.height())),
      cairo_(cairo_create(surface_)) {
    assert(cairo_surface_status(surface_) == CAIRO_STATUS_SUCCESS);
    // Clear the surface.
    cairo_save(cairo_);
    cairo_set_source_rgba(cairo_, 0, 0, 0, 0);
    cairo_set_operator(cairo_, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cairo_);
    cairo_restore(cairo_);
}

CairoSurface::~CairoSurface() {
    // `cairo_destroy` and `cairo_surface_destroy` decrease the reference count on
    // `cairo_` and `surface_` objects respectively. The underlying memory is
    // freed if the reference count goes to zero. We use ExtractAsDangling() here
    // to avoid holding a briefly dangling ptr in case the memory is freed.
    cairo_destroy(cairo_);
    cairo_surface_destroy(surface_);
}



GdkRGBA CairoSurface::GetAveragePixelValue(bool frame) {
    cairo_surface_flush(surface_);
    unsigned char* data =cairo_image_surface_get_data(surface_);
    int width = cairo_image_surface_get_width(surface_);
    int height = cairo_image_surface_get_height(surface_);
    int stride = cairo_image_surface_get_stride(surface_);
    assert(cairo_image_surface_get_format(surface_) == CAIRO_FORMAT_ARGB32);
    long a = 0, r = 0, g = 0, b = 0;
    uint64_t max_alpha = 0;
    for (int line = 0; line < height; line++)
    {
        uint64_t* rgbaLine = (uint64_t*)(&data[line*stride]);
        for (int i = 0; i < width; i++) {
            uint64_t color =  rgbaLine[i];
            max_alpha = std::max((color >> 24) & 0xFF, max_alpha);
            a += (color >> 24) & 0xFF;
            r += (color >> 16) & 0xFF;
            g += (color >> 8) & 0xFF;
            b += (color)  & 0xFF;
        }
    }

    GdkRGBA out = {0,0,0,0};
    if (a == 0) {
        return out;
    }
    out.red = gdouble(r) / a;
    out.green = gdouble(g) / a;
    out.blue = gdouble(b) / a;
    out.alpha = frame ? (max_alpha / 255.) : gdouble(a) / (width * height * 255);
    return out;
}

GtkCssContext::GtkCssContext(GtkWidget* widget, GtkWidget* root)
    : widget_(widget), root_(WrapGObject(root)) {
    assert(GtkCheckVersion(4));
}

GtkCssContext::GtkCssContext(GtkStyleContext* context)
    : context_(WrapGObject(context)) {
    assert(!GtkCheckVersion(4));
}

GtkCssContext::GtkCssContext() = default;
GtkCssContext::GtkCssContext(const GtkCssContext&) = default;
GtkCssContext::GtkCssContext(GtkCssContext&&) = default;
GtkCssContext& GtkCssContext::operator=(const GtkCssContext&) = default;
GtkCssContext& GtkCssContext::operator=(GtkCssContext&&) = default;
GtkCssContext::~GtkCssContext() = default;

GtkCssContext::operator GtkStyleContext*() {
    if (GtkCheckVersion(4))
        return widget_ ? gtk_widget_get_style_context(widget_) : nullptr;
    return context_;
}

GtkCssContext GtkCssContext::GetParent() {
    if (GtkCheckVersion(4)) {
        return GtkCssContext(WrapGObject(gtk_widget_get_parent(widget_)),
                             root_ == widget_ ? ScopedGObject<GtkWidget>() : root_);
    }
    return GtkCssContext(WrapGObject(gtk_style_context_get_parent(context_)));
}

GtkWidget* GtkCssContext::widget() {
    assert(GtkCheckVersion(4));
    return widget_;
}

GtkWidget* GtkCssContext::root() {
    assert(GtkCheckVersion(4));
    return root_;
}


GtkCssContext AppendCssNodeToStyleContext(GtkCssContext context,
                                          const std::string& css_node) {
    enum {
        CSS_TYPE,
        CSS_NAME,
        CSS_OBJECT_NAME,
        CSS_CLASS,
        CSS_PSEUDOCLASS,
        CSS_NONE,
    } part_type = CSS_TYPE;

    static const struct {
        const char* name;
        GtkStateFlags state_flag;
    } pseudo_classes[] = {
    {"active", GTK_STATE_FLAG_ACTIVE},
    {"hover", GTK_STATE_FLAG_PRELIGHT},
    {"selected", GTK_STATE_FLAG_SELECTED},
    {"disabled", GTK_STATE_FLAG_INSENSITIVE},
    {"indeterminate", GTK_STATE_FLAG_INCONSISTENT},
    {"focus", GTK_STATE_FLAG_FOCUSED},
    {"backdrop", GTK_STATE_FLAG_BACKDROP},
    {"link", GTK_STATE_FLAG_LINK},
    {"visited", GTK_STATE_FLAG_VISITED},
    {"checked", GTK_STATE_FLAG_CHECKED},
};

    GType gtype = G_TYPE_NONE;
    std::string name;
    std::string object_name;
    std::vector<std::string> classes;
    GtkStateFlags state = GTK_STATE_FLAG_NORMAL;

    auto it = css_node.cbegin();

    //base::StringTokenizer t(css_node, ".:#()");
    //t.set_options(base::StringTokenizer::RETURN_DELIMS);
    while (it != css_node.cend()) {

        auto found = std::find_if(it, css_node.cend(), [](const char c) {
            return (c == '.') || (c == ':') || (c == '#') || (c == '(') || (c == ')');
        });

        std::string token(it, found);

        if (token != "")
        {
            switch (part_type) {
            case CSS_NAME:
                name = token;
                break;
            case CSS_OBJECT_NAME:
                object_name = token;
                break;
            case CSS_TYPE: {
                if (!GtkCheckVersion(4)) {
                    gtype = g_type_from_name(token.c_str());
                    assert(gtype);
                }
                break;
            }
            case CSS_CLASS:
                classes.push_back(token);
                break;
            case CSS_PSEUDOCLASS: {
                GtkStateFlags state_flag = GTK_STATE_FLAG_NORMAL;
                for (const auto& pseudo_class_entry : pseudo_classes) {
                    if (strcmp(pseudo_class_entry.name, token.c_str()) == 0) {
                        state_flag = pseudo_class_entry.state_flag;
                        break;
                    }
                }
                state = static_cast<GtkStateFlags>(state | state_flag);
                break;
            }
            case CSS_NONE:
                assert(false);
            }
        }

        if (found == css_node.cend())
            break;

        switch (*found) {
        case '(':
            part_type = CSS_NAME;
            break;
        case ')':
            part_type = CSS_NONE;
            break;
        case '#':
            part_type = CSS_OBJECT_NAME;
            break;
        case '.':
            part_type = CSS_CLASS;
            break;
        case ':':
            part_type = CSS_PSEUDOCLASS;
            break;
        default:
            assert("unreachable" == 0);
        }

        it = found + 1;
    }

    // Always add a "chromium" class so that themes can style chromium
    // widgets specially if they want to.
    classes.push_back("vlc");

    float scale = std::round(GetDeviceScaleFactor());

    return AppendCssNodeToStyleContextImpl(context, gtype, name, object_name,
                                           classes, state, scale);
}

GtkCssContext GetStyleContextFromCss(const std::string& css_selector) {
    // Prepend a window node to the selector since all widgets must live
    // in a window, but we don't want to specify that every time.
    auto context = AppendCssNodeToStyleContext({}, "GtkWindow#window.background");

    auto it = css_selector.cbegin();
    while (it != css_selector.cend())
    {
        auto found = std::find(it, css_selector.cend(), ' ');
        std::string widget_type(it, found);
        if (widget_type != "")
            context = AppendCssNodeToStyleContext(context, widget_type);

        if (found == css_selector.cend())
            break;

        it = found + 1;
    }
    return context;
}

GdkRGBA GetBgColorFromStyleContext(GtkCssContext context) {
    // Backgrounds are more general than solid colors (eg. gradients),
    // but chromium requires us to boil this down to one color.  We
    // cannot use the background-color here because some themes leave it
    // set to a garbage color because a background-image will cover it
    // anyway.  So we instead render the background into a 24x24 bitmap,
    // removing any borders, and hope that we get a good color.
    ApplyCssToContext(context,
                      "* {"
                      "border-radius: 0px;"
                      "border-style: none;"
                      "box-shadow: none;"
                      "}");
    MySize size(24, 24);
    CairoSurface surface(size);
    RenderBackground(size, surface.cairo(), context);
    return surface.GetAveragePixelValue(false);
}

GdkRGBA GtkStyleContextGetColor(GtkStyleContext* context) {
    GdkRGBA color;
    gtk_style_context_get_color(context, gtk_style_context_get_state(context), &color );
    return color;
}


GdkRGBA GetFgColor(const std::string& css_selector) {
    return GtkStyleContextGetColor(GetStyleContextFromCss(css_selector));
}



ScopedCssProvider GetCssProvider(const std::string& css) {
    auto provider = TakeGObject(gtk_css_provider_new());

    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    return provider;
}

void ApplyCssProviderToContext(GtkCssContext context,
                               GtkCssProvider* provider) {
    while (context) {
        gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                       G_MAXUINT);
        context = context.GetParent();
    }
}

void ApplyCssToContext(GtkCssContext context, const std::string& css) {
    auto provider = GetCssProvider(css);
    ApplyCssProviderToContext(context, provider);
}

void RenderBackground(const MySize& size,
                      cairo_t* cr,
                      GtkCssContext context) {
    if (!context)
        return;
    RenderBackground(size, cr, context.GetParent());
    gtk_render_background(context, cr, 0, 0, size.width(), size.height());
}

GdkRGBA GetBgColor(const std::string& css_selector) {
    return GetBgColorFromStyleContext(GetStyleContextFromCss(css_selector));
}

GdkRGBA GetBorderColor(const std::string& css_selector) {
    // Borders have the same issue as backgrounds, due to the
    // border-image property.
    auto context = GetStyleContextFromCss(css_selector);
    MySize size(24, 24);
    CairoSurface surface(size);
    gtk_render_frame(context, surface.cairo(), 0, 0, size.width(), size.height());
    return surface.GetAveragePixelValue(true);
}

GdkRGBA GetSelectionBgColor(const std::string& css_selector) {
    auto context = GetStyleContextFromCss(css_selector);
    return GetBgColorFromStyleContext(context);
}



bool ContextHasClass(GtkCssContext context, const std::string& style_class) {
    bool has_class = gtk_style_context_has_class(context, style_class.c_str());
    if (!GtkCheckVersion(4)) {
        has_class |= gtk_widget_path_iter_has_class(
                    gtk_style_context_get_path(context), -1, style_class.c_str());
    }
    return has_class;
}

GdkRGBA GetSeparatorColor(const std::string& css_selector) {
    auto context = GetStyleContextFromCss(css_selector);
    bool horizontal = ContextHasClass(context, "horizontal");

    int w = 1, h = 1;
    if (GtkCheckVersion(4)) {
        auto size = GetSeparatorSize(horizontal);
        w = size.width();
        h = size.height();
    } else {
        gtk_style_context_get(context, gtk_style_context_get_state(context), "min-width", &w, "min-height", &h, nullptr);
    }

    MyInset border = GtkStyleContextGetBorder(context);
    MyInset padding = GtkStyleContextGetPadding(context);

    w += border.left() + padding.left() + padding.right() + border.right();
    h += border.top() + padding.top() + padding.bottom() + border.bottom();

    if (horizontal) {
        w = 24;
        h = std::max(h, 1);
    } else {
        assert(ContextHasClass(context, "vertical"));
        h = 24;
        w = std::max(w, 1);
    }

    CairoSurface surface(MySize(w, h));
    gtk_render_background(context, surface.cairo(), 0, 0, w, h);
    gtk_render_frame(context, surface.cairo(), 0, 0, w, h);
    return surface.GetAveragePixelValue(false);
}

std::string GetGtkSettingsStringProperty(GtkSettings* settings,
                                         const gchar* prop_name) {
    GValue layout = G_VALUE_INIT;
    g_value_init(&layout, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(settings), prop_name, &layout);
    assert(G_VALUE_HOLDS_STRING(&layout));
    std::string prop_value(g_value_get_string(&layout));
    g_value_unset(&layout);
    return prop_value;
}

GtkIconTheme* GetDefaultIconTheme() {
#if 0 //FIXME GTK4
    return GtkCheckVersion(4)
            ? gtk_icon_theme_get_for_display(gdk_display_get_default())
            : gtk_icon_theme_get_default();
#endif
    return gtk_icon_theme_get_default();
}

void GtkWindowDestroy(GtkWidget* widget) {
#if 0 //FIXME GTK4
    if (GtkCheckVersion(4))
        gtk_window_destroy(GTK_WINDOW(widget));
#endif
    gtk_widget_destroy(widget);
}

GtkWidget* GetDummyWindow() {
    static GtkWidget* window = CreateDummyWindow();
    return window;
}

MySize GetSeparatorSize(bool horizontal) {
    auto widget = TakeGObject(gtk_separator_new(
                                  horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL));
    GtkRequisition natural_size;
    gtk_widget_get_preferred_size(widget, nullptr, &natural_size);
    return {natural_size.width, natural_size.height};
}

static float g_deviceScaleFactor =  1.0f;

void SetDeviceScaleFactor(float scaleFactor) {
    g_deviceScaleFactor = scaleFactor;
}

float GetDeviceScaleFactor() {
    return g_deviceScaleFactor;
}


}  // namespace gtk
