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

#ifndef UI_GTK_GTK_UTIL_H_
#define UI_GTK_GTK_UTIL_H_

#include <string>
#include <vector>


#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include "gtk_compat.h"
#include "scoped_gobject.h"

#include "vlc_picture.h"

namespace gtk {

bool GtkCheckVersion(uint32_t major, uint32_t minor = 0, uint32_t micro = 0);

const char* GtkCssMenu();
const char* GtkCssMenuItem();
const char* GtkCssMenuScrollbar();

bool GtkInitFromCommandLine(int* argc, char** argv);

// Sets |dialog| as transient for |parent|, which will keep it on top and center
// it above |parent|. Do nothing if |parent| is nullptr.
//void SetGtkTransientForAura(GtkWidget* dialog, aura::Window* parent);

// Gets the transient parent aura window for |dialog|.
//aura::Window* GetAuraTransientParent(GtkWidget* dialog);

// Clears the transient parent for |dialog|.
//void ClearAuraTransientParent(GtkWidget* dialog, aura::Window* parent);

// Parses |button_string| into |leading_buttons| and
// |trailing_buttons|.  The string is of the format
// "<button>*:<button*>", for example, "close:minimize:maximize".
// This format is used by GTK settings and gsettings.
//void ParseButtonLayout(const std::string& button_string,
//                       std::vector<views::FrameButton>* leading_buttons,
//                       std::vector<views::FrameButton>* trailing_buttons);

class CairoSurface {
 public:
  // Attaches a cairo surface to an SkBitmap so that GTK can render
  // into it.  |bitmap| must outlive this CairoSurface.
  explicit CairoSurface(VLCPicturePtr& bitmap);

  // Creates a new cairo surface with the given size.  The memory for
  // this surface is deallocated when this CairoSurface is destroyed.
  explicit CairoSurface(const MySize& size);

  ~CairoSurface();

  // Get the drawing context for GTK to use.
  cairo_t* cairo() { return cairo_; }

  // Returns the average of all pixels in the surface.  If |frame| is
  // true, the resulting alpha will be the average alpha, otherwise it
  // will be the max alpha across all pixels.
  GdkRGBA GetAveragePixelValue(bool frame);

 private:
  cairo_surface_t* surface_;
  cairo_t* cairo_;
};

class GtkCssContext {
 public:
  GtkCssContext();
  GtkCssContext(const GtkCssContext&);
  GtkCssContext(GtkCssContext&&);
  GtkCssContext& operator=(const GtkCssContext&);
  GtkCssContext& operator=(GtkCssContext&&);
  ~GtkCssContext();

  // GTK3 constructor.
  explicit GtkCssContext(GtkStyleContext* context);

  // GTK4 constructor.
  GtkCssContext(GtkWidget* widget, GtkWidget* root);

  // As a convenience, allow using a GtkCssContext as a gtk_style_context()
  // to avoid repeated use of an explicit getter.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator GtkStyleContext*();

  GtkCssContext GetParent();

  // Only available on GTK4.
  GtkWidget* widget();
  GtkWidget* root();

 private:
  // GTK3 state.
  ScopedGObject<GtkStyleContext> context_;

  // GTK4 state.
  // GTK widgets own their children, so instead of keeping a reference to the
  // widget directly, keep a reference to the root widget.
  GtkWidget* widget_ = nullptr;
  ScopedGObject<GtkWidget> root_;
};

using ScopedCssProvider = ScopedGObject<GtkCssProvider>;

}  // namespace gtk

// Template override cannot be in the gtk namespace.
template <>
inline void ScopedGObject<GtkStyleContext>::Unref() {
  // Versions of GTK earlier than 3.15.4 had a bug where a g_assert
  // would be triggered when trying to free a GtkStyleContext that had
  // a parent whose only reference was the child context in question.
  // This is a hack to work around that case.  See GTK commit
  // "gtkstylecontext: Don't try to emit a signal when finalizing".
  GtkStyleContext* context = obj_;
  while (context) {
    GtkStyleContext* parent = gtk_style_context_get_parent(context);
    if (parent && G_OBJECT(context)->ref_count == 1 &&
        !gtk::GtkCheckVersion(3, 15, 4)) {
      g_object_ref(parent);
      gtk_style_context_set_parent(context, nullptr);
      g_object_unref(context);
    } else {
      g_object_unref(context);
      return;
    }
    context = parent;
  }
}

namespace gtk {

// If |context| is nullptr, creates a new top-level style context
// specified by parsing |css_node|.  Otherwise, creates the child
// context with |context| as the parent.
GtkCssContext AppendCssNodeToStyleContext(GtkCssContext context,
                                          const std::string& css_node);

// Parses |css_selector| into a StyleContext.  The format is a
// sequence of whitespace-separated objects.  Each object may have at
// most one object name at the beginning of the string, and any number
// of '.'-prefixed classes and ':'-prefixed pseudoclasses.  An example
// is "GtkButton.button.suggested-action:hover:active".  The caller
// must g_object_unref() the returned context.
GtkCssContext GetStyleContextFromCss(const std::string& css_selector);


GdkRGBA GtkStyleContextGetColor(GtkStyleContext* context);

GdkRGBA GetBgColorFromStyleContext(GtkCssContext context);

// Overrides properties on |context| and all its parents with those
// provided by |css|.
void ApplyCssToContext(GtkCssContext context, const std::string& css);

// Get the 'color' property from the style context created by
// GetStyleContextFromCss(|css_selector|).
GdkRGBA GetFgColor(const std::string& css_selector);

ScopedCssProvider GetCssProvider(const std::string& css);

// Renders the backgrounds of all ancestors of |context|, then renders
// the background for |context| itself.
void RenderBackground(const MySize& size,
                      cairo_t* cr,
                      GtkCssContext context);

// Renders a background from the style context created by
// GetStyleContextFromCss(|css_selector|) into a 24x24 bitmap and
// returns the average color.
GdkRGBA GetBgColor(const std::string& css_selector);

// Renders the border from the style context created by
// GetStyleContextFromCss(|css_selector|) into a 24x24 bitmap and
// returns the average color.
GdkRGBA GetBorderColor(const std::string& css_selector);

// Renders focus indicator from the style context created by
// GetStyleContextFromCss(|css_selector|) into a 24x24 bitmap and
// returns the average color.
GdkRGBA GetFocusColor(const std::string& css_selector);
GdkRGBA GetFocusColorFromContext(GtkCssContext context);

// On Gtk3.20 or later, behaves like GetBgColor.  Otherwise, returns
// the background-color property.
GdkRGBA GetSelectionBgColor(const std::string& css_selector);

// Get the color of the GtkSeparator specified by |css_selector|.
GdkRGBA GetSeparatorColor(const std::string& css_selector);

// Get a GtkSettings property as a C++ string.
std::string GetGtkSettingsStringProperty(GtkSettings* settings,
                                         const gchar* prop_name);


GtkIconTheme* GetDefaultIconTheme();

void GtkWindowDestroy(GtkWidget* widget);

GtkWidget* GetDummyWindow();

MySize GetSeparatorSize(bool horizontal);

void SetDeviceScaleFactor(float scaleFactor);
float GetDeviceScaleFactor();

}  // namespace gtk

#endif  // UI_GTK_GTK_UTIL_H_
