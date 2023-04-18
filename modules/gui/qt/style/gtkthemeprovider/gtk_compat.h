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

#ifndef GTK_COMPAT_H
#define GTK_COMPAT_H


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_picture.h>

#include <algorithm>

#include <gtk/gtk.h>

class VLCPicturePtr
{
public:
    VLCPicturePtr()
    {
    }

    VLCPicturePtr(vlc_fourcc_t i_chroma, int i_width, int i_height)
    {
        m_picture = picture_New(i_chroma, i_width, i_height, 1, 1);
    }

    VLCPicturePtr(const VLCPicturePtr& o)
        : m_picture(o.m_picture)
    {
        Ref();
    }

    VLCPicturePtr(VLCPicturePtr&& o)
        : m_picture(o.m_picture)
    {
        o.m_picture = nullptr;
    }

    VLCPicturePtr& operator=(const VLCPicturePtr& o)
    {
        Unref();
        m_picture = o.m_picture;
        Ref();
        return *this;
    }

    VLCPicturePtr& operator=(VLCPicturePtr&& o)
    {
        Unref();
        m_picture = o.m_picture;
        o.m_picture = nullptr;
        return *this;
    }

    ~VLCPicturePtr()
    {
        Unref();
    }

    picture_t* get() const
    {
        return m_picture;
    }

private:
    void Ref() {
        if (m_picture) {
            picture_Hold(m_picture);
        }
    }

    // This function is necessary so that gtk can overload it in
    // the case of T = GtkStyleContext.
    void Unref() {
        if (m_picture)
            picture_Release(m_picture);
    }

    picture_t* m_picture = nullptr;
};


namespace gtk {

//Basic geometry types

gdouble GdkRBGALightness(GdkRGBA& c1);


GdkRGBA GdkRBGABlend(GdkRGBA& c1, GdkRGBA& c2, gdouble blend);

class MySize {
public:
    MySize()
    {}

    MySize(int w, int h)
        : m_width(std::max(w, 0))
        , m_height(std::max(h, 0))
    {}

    MySize(const MySize&) = default;
    MySize(MySize&&) = default;
    MySize& operator=(const MySize&) = default;
    MySize& operator=(MySize&&) = default;

    inline bool IsEmpty() const { return !m_width  || !m_height ; }

    inline int width() const { return m_width; }
    inline int height() const { return m_height; }

    inline void set_width(int width) {
        m_width = std::max(width, 0);
    }

    inline void set_height(int height) {
        m_height = std::max(height, 0);
    }


private:
    int m_width = 0;
    int m_height = 0;
};


class MyPoint {
public:
    MyPoint(int w, int h)
        : x(w)
        , y(h)
    {}

    MyPoint(const MyPoint&) = default;
    MyPoint(MyPoint&&) = default;
    MyPoint& operator=(const MyPoint&) = default;
    MyPoint& operator=(MyPoint&&) = default;

    int x = -1;
    int y = -1;
};

class MyInset {
public:
    MyInset() {}

    MyInset(int l,int r,int t,int b)
        : m_top(t)
        , m_bottom(b)
        , m_left(l)
        , m_right(r)
    {}

    MyInset(const GtkBorder& border)
        : m_top (border.top)
        , m_bottom (border.bottom)
        , m_left (border.left)
        , m_right (border.right)
    {
    }

    MyInset(const MyInset&) = default;
    MyInset(MyInset&&) = default;
    MyInset& operator=(const MyInset&) = default;
    MyInset& operator=(MyInset&&) = default;

    inline int right() const { return m_right;  }
    inline int left() const { return m_left;  }
    inline int bottom() const { return m_bottom;  }
    inline int top() const { return m_top;  }
    int width() const { return m_left + m_right; }
    int height() const { return m_top + m_bottom; }



    MyInset operator-() const {
      return MyInset(-m_left, -m_right, -m_top, -m_bottom);
    }


private:
    int m_top = -1;
    int m_bottom = -1;
    int m_left = -1;
    int m_right = -1;
};

class MyRect {
public:
    MyRect() {}

    MyRect(const MySize& s)
        : m_top(0)
        , m_left(0)
        , m_size(s)
    {}

    MyRect(const MyPoint& p, const MySize& s)
        : m_top(p.y)
        , m_left(p.x)
        , m_size(s)
    {}

    MyRect(const MyRect&) = default;
    MyRect(MyRect&&) = default;
    MyRect& operator=(const MyRect&) = default;
    MyRect& operator=(MyRect&&) = default;

    inline void Inset(const MyInset& i)
    {
        m_top += i.top();
        m_left += i.left();
        set_width(m_size.width() - i.width());
        set_height(m_size.height() - i.height());
    }


    inline int top() const { return m_top; };
    inline int left() const { return m_left; };
    inline int width() const { return m_size.width(); };
    inline int height() const { return m_size.height(); };

    inline void set_width(int width) {
        m_size.set_width(width);
    }

    inline void set_height(int height) {
        m_size.set_height(height);
    }

    MySize size() const { return m_size; }

private:
    int m_top= 0;
    int m_left = 0;
    MySize m_size;
};


MyInset GtkStyleContextGetPadding(GtkStyleContext* context);
MyInset GtkStyleContextGetBorder(GtkStyleContext* context);
MyInset GtkStyleContextGetMargin(GtkStyleContext* context);

};


#endif // GTK_COMPAT_H
