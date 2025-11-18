/*****************************************************************************
 * qvlcframe.hpp : A few helpers
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef VLC_QT_QVLCFRAME_HPP_
#define VLC_QT_QVLCFRAME_HPP_

#include <QWidget>
#include <QDialog>
#include <QApplication>
#include <QMainWindow>
#include <QKeyEvent>
#include <QScreen>
#include <QSettings>
#include <QStyle>

#include "qt.hpp"

#ifdef _WIN32
    #include <QLibrary>

    // Typedefs for functions
    typedef HRESULT(WINAPI *DwmSetWindowAttributeFunc)(HWND, DWORD, LPCVOID, DWORD);
    typedef HRESULT(WINAPI *DwmGetColorizationColorFunc)(DWORD*, BOOL*);

    // Dark mode constants
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
    constexpr DWORD DWMWA_USE_DARK_MODE_UNDOCUMENTED = 19;

    inline bool setImmersiveDarkModeAttribute(HWND hwnd, bool enable) {
        static const auto dwmSetWindowAttributeFunc = []() -> DwmSetWindowAttributeFunc {
            HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
            if (GetProcAddress(hKernel32, "GetSystemCpuSetInformation") == NULL)
                return nullptr;

            QLibrary dwmapidll("dwmapi");
            return reinterpret_cast<DwmSetWindowAttributeFunc>(dwmapidll.resolve("DwmSetWindowAttribute"));
        }();

        if (!dwmSetWindowAttributeFunc || !hwnd)
            return false;

        const BOOL pvAttribute = enable ? TRUE : FALSE;

        return SUCCEEDED(dwmSetWindowAttributeFunc(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &pvAttribute, sizeof(pvAttribute)))
            || SUCCEEDED(dwmSetWindowAttributeFunc(hwnd, DWMWA_USE_DARK_MODE_UNDOCUMENTED, &pvAttribute, sizeof(pvAttribute)));
    }

    // Overloaded function to apply dark mode to QWidget*
    inline bool setImmersiveDarkModeAttribute(QWidget *widget) {
        if (widget->isWindow()) {
            widget->ensurePolished();
            HWND hwnd = (HWND)widget->winId();  // Get native window handle
            return setImmersiveDarkModeAttribute(hwnd,true);  // Call the HWND version
        }
        return false;
    }

    // Get Windows accent color
    inline QColor getWindowsAccentColor()
    {
        static const auto dwmGetColorizationColorFunc = []() -> DwmGetColorizationColorFunc {
            QLibrary dwmapidll("dwmapi");
            return reinterpret_cast<DwmGetColorizationColorFunc>(dwmapidll.resolve("DwmGetColorizationColor"));
        }();
        static const QColor fallbackColor(42, 130, 218);
        if (!dwmGetColorizationColorFunc) return fallbackColor;

        DWORD color = 0;
        BOOL opaque = FALSE;
        HRESULT hr = dwmGetColorizationColorFunc(&color, &opaque);

        if (FAILED(hr)) return fallbackColor;

        QColor c(qRed(color), qGreen(color), qBlue(color));
        
        int h = c.hue();
        int s = c.saturation();
        int v = c.value();

        // 1) Make the color DARKER overall (to match dark-theme highlights)
        v = qBound(60, v, 160);   // Old: 120–220, New: 60–160 → much darker

        // 2) Ensure enough saturation so it's not muddy/gray
        s = qBound(80, s, 255);   // Old: s >= 55 → now greater minimum

        // 3) Prevent neon colors (very bright + very saturated)
        if (s > 220 && v > 140) {
            s = 200;
            v = 120;
        }

        // 4) Special casing for yellow / green hues.
        // These colors have poor contrast with white unless darkened heavily.
        if (h >= 30 && h <= 85) {         // Yellow to yellow-green
            h = (h - 10 + 360) % 360;     // Shift toward orange (warmer, safer)
            s = qBound(100, s, 180);      // Prevent neon yellows
            v = qBound(70,  v, 130);      // Ensure dark-ish gold
        }
        else if (h >= 85 && h <= 140) {   // Greens → shift toward teal
            h = (h + 20) % 360;           // Move green → teal for better white contrast
            s = qBound(100, s, 220);
            v = qBound(60,  v, 140);
        }

        // 5) Final normalization: ensure contras
        if (v > 150) v = 150;     // Hard cap brightness
        if (s < 80)  s = 80;      // Hard floor saturation

        // Rebuild safe accent
        c.setHsv(h, s, v);

        return c;
    }

#endif

class QVLCTools
{
   public:
       /*
        use this function to save a widgets screen position
        only for windows / dialogs which are floating, if a
        window is docked into another - don't all this function
        or it may write garbage to position info!
       */
       static void saveWidgetPosition( QSettings *settings, QWidget *widget)
       {
         settings->setValue("geometry", widget->saveGeometry());
       }
       static void saveWidgetPosition( intf_thread_t *p_intf,
                                       const QString& configName,
                                       QWidget *widget)
       {
         getSettings()->beginGroup( configName );
         QVLCTools::saveWidgetPosition(getSettings(), widget);
         getSettings()->endGroup();
       }


       /*
         use this method only for restoring window state of non docked
         windows!
       */
       static bool restoreWidgetPosition(QSettings *settings,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ))
       {
          if(!widget->restoreGeometry(settings->value("geometry")
                                      .toByteArray()))
          {
            widget->move(defPos);
            widget->resize(defSize);

            if(defPos.x() == 0 && defPos.y()==0)
               widget->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, widget->size(), QGuiApplication::primaryScreen()->availableGeometry()));
            return true;
          }
          return false;
       }

       static bool restoreWidgetPosition( intf_thread_t *p_intf,
                                           const QString& configName,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ) )
       {
         getSettings()->beginGroup( configName );
         bool defaultUsed = QVLCTools::restoreWidgetPosition( getSettings(),
                                                                   widget,
                                                                   defSize,
                                                                   defPos);
         getSettings()->endGroup();

         return defaultUsed;
       }
};

class QVLCFrame : public QWidget
{
public:
    QVLCFrame( intf_thread_t *_p_intf ) : QWidget( NULL ), p_intf( _p_intf )
    {
#ifdef Q_OS_WIN
        if (isDarkPaletteEnabled(p_intf))
            setImmersiveDarkModeAttribute(this);
#endif
    };
    virtual ~QVLCFrame()   {};

    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;

    void restoreWidgetPosition( const QString& name,
                       QSize defSize = QSize( 1, 1 ),
                       QPoint defPos = QPoint( 0, 0 ) )
    {
        QVLCTools::restoreWidgetPosition(p_intf, name, this, defSize, defPos);
    }

    void saveWidgetPosition( const QString& name )
    {
        QVLCTools::saveWidgetPosition( p_intf, name, this);
    }

    virtual void cancel()
    {
        hide();
    }
    virtual void close()
    {
        hide();
    }
    void keyPressEvent( QKeyEvent *keyEvent ) Q_DECL_OVERRIDE
    {
        if( keyEvent->key() == Qt::Key_Escape )
        {
            this->cancel();
        }
        else if( keyEvent->key() == Qt::Key_Return
              || keyEvent->key() == Qt::Key_Enter )
        {
             this->close();
        }
    }
};

class QVLCDialog : public QDialog
{
public:
    QVLCDialog( QWidget* parent, intf_thread_t *_p_intf ) :
                                    QDialog( parent ), p_intf( _p_intf )
    {
        setWindowFlags( Qt::Dialog|Qt::WindowMinMaxButtonsHint|
                        Qt::WindowSystemMenuHint|Qt::WindowCloseButtonHint );
#ifdef Q_OS_WIN
        if (isDarkPaletteEnabled(p_intf))
            setImmersiveDarkModeAttribute(this);
#endif
    }
    virtual ~QVLCDialog() {};
    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }

protected:
    intf_thread_t *p_intf;

    virtual void cancel()
    {
        hide();
    }
    virtual void close()
    {
        hide();
    }
    void keyPressEvent( QKeyEvent *keyEvent ) Q_DECL_OVERRIDE
    {
        if( keyEvent->key() == Qt::Key_Escape )
        {
            this->cancel();
        }
        else if( keyEvent->key() == Qt::Key_Return
              || keyEvent->key() == Qt::Key_Enter )
        {
            this->close();
        }
    }
};

class QVLCMW : public QMainWindow
{
public:
    QVLCMW( intf_thread_t *_p_intf ) : QMainWindow( NULL ), p_intf( _p_intf )
    {
#ifdef Q_OS_WIN
        if (isDarkPaletteEnabled(p_intf))
            setImmersiveDarkModeAttribute(this);
#endif
    }
    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;
    QSize mainSize;

    void readSettings( const QString& name, QSize defSize )
    {
        QVLCTools::restoreWidgetPosition( p_intf, name, this, defSize);
    }

    void readSettings( const QString& name )
    {
        QVLCTools::restoreWidgetPosition( p_intf, name, this);
    }
    void readSettings( QSettings *settings )
    {
        QVLCTools::restoreWidgetPosition(settings, this);
    }

    void readSettings( QSettings *settings, QSize defSize)
    {
        QVLCTools::restoreWidgetPosition(settings, this, defSize);
    }

    void writeSettings( const QString& name )
    {
        QVLCTools::saveWidgetPosition( p_intf, name, this);
    }
    void writeSettings(QSettings *settings )
    {
        QVLCTools::saveWidgetPosition(settings, this);
    }
};

#endif
