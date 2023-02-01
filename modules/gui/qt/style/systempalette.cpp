/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include "systempalette.hpp"
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QFile>
#include <maininterface/mainctx.hpp>
#include <vlc_modules.h>

namespace {

QColor blendColors(QColor c1, QColor c2, float blend = 0.5)
{
    return QColor::fromRgbF(c2.redF()   + (c1.redF()   - c2.redF())   * blend,
                  c2.greenF() + (c1.greenF() - c2.greenF()) * blend,
                  c2.blueF()  + (c1.blueF()  - c2.blueF())  * blend,
                  c2.alphaF() + (c1.alphaF() - c2.alphaF()) * blend);
}

QColor setColorAlpha(const QColor& c1, float alpha)
{
    QColor c(c1);
    c.setAlphaF(alpha);
    return c;
}

#ifndef _WIN32
/**
 * function taken from QtBase gui/platform/unix/qgenericunixservices.cpp
 */
static inline QByteArray detectLinuxDesktopEnvironment()
{
    const QByteArray xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (!xdgCurrentDesktop.isEmpty())
        return xdgCurrentDesktop.toUpper(); // KDE, GNOME, UNITY, LXDE, MATE, XFCE...

    // Classic fallbacks
    if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))
        return QByteArrayLiteral("KDE");
    if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID"))
        return QByteArrayLiteral("GNOME");

    // Fallback to checking $DESKTOP_SESSION (unreliable)
    QByteArray desktopSession = qgetenv("DESKTOP_SESSION");

    // This can be a path in /usr/share/xsessions
    int slash = desktopSession.lastIndexOf('/');
    if (slash != -1) {
        QSettings desktopFile(QFile::decodeName(desktopSession + ".desktop"), QSettings::IniFormat);
        desktopFile.beginGroup(QStringLiteral("Desktop Entry"));
        QByteArray desktopName = desktopFile.value(QStringLiteral("DesktopNames")).toByteArray();
        if (!desktopName.isEmpty())
            return desktopName;

        // try decoding just the basename
        desktopSession = desktopSession.mid(slash + 1);
    }

    if (desktopSession == "gnome")
        return QByteArrayLiteral("GNOME");
    else if (desktopSession == "xfce")
        return QByteArrayLiteral("XFCE");
    else if (desktopSession == "kde")
        return QByteArrayLiteral("KDE");

    return QByteArrayLiteral("UNKNOWN");
}

bool isGTKBasedEnvironment()
{
    QList<QByteArray> gtkBasedEnvironments{
        "GNOME",
        "X-CINNAMON",
        "UNITY",
        "MATE",
        "XFCE",
        "LXDE"
    };
    const QList<QByteArray> desktopNames = detectLinuxDesktopEnvironment().split(':');
    for (const QByteArray& desktopName: desktopNames)
    {
        if (gtkBasedEnvironments.contains(desktopName))
            return true;
    }
    return false;
}
#endif


static void PaletteChangedCallback(vlc_qt_theme_provider_t*, void* data)
{
    auto priv = static_cast<ExternalPaletteImpl*>(data);
    emit priv->paletteChanged();
}

static void MetricsChangedCallback(vlc_qt_theme_provider_t*, vlc_qt_theme_image_type type, void* data)
{
    auto priv = static_cast<ExternalPaletteImpl*>(data);
    assert(priv);
    priv->updateMetrics(type);
}


static void ReleaseVLCPictureCb(void* data)
{
    auto pic = static_cast<picture_t*>(data);
    if (pic)
        picture_Release(pic);
}


static void setQColorRBGAInt(void* data, int r, int g, int b, int a)
{
    auto color = static_cast<QColor*>(data);
    color->setRgb(r,g,b,a);
}

static void setQColorRBGAFloat(void* data, double r, double g, double b, double a)
{
    auto color = static_cast<QColor*>(data);
    color->setRgbF(r,g,b,a);
}


}

ExternalPaletteImpl::ExternalPaletteImpl(MainCtx* ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
}

ExternalPaletteImpl::~ExternalPaletteImpl()
{
    if (m_provider)
    {
        if (m_provider->close)
            m_provider->close(m_provider);
        if (m_module)
            module_unneed(m_provider, m_module);
        vlc_object_delete(m_provider);
    }
}

bool ExternalPaletteImpl::init()
{
    QString preferedProvider;
#ifndef _WIN32
    if (isGTKBasedEnvironment())
        preferedProvider = "qt-themeprovider-gtk";
#endif

    m_provider = static_cast<vlc_qt_theme_provider_t*>(vlc_object_create(m_ctx->getIntf(), sizeof(vlc_qt_theme_provider_t)));
    if (!m_provider)
        return false;
    m_provider->paletteUpdated = PaletteChangedCallback;
    m_provider->paletteUpdatedData = this;

    m_provider->metricsUpdated = MetricsChangedCallback;
    m_provider->metricsUpdatedData = this;

    m_provider->setColorF = setQColorRBGAFloat;
    m_provider->setColorInt = setQColorRBGAInt;

    m_module = module_need(m_provider, "qt theme provider",
                           preferedProvider.isNull() ? nullptr : qtu(preferedProvider),
                           true);
    if (!m_module)
        return false;
    return true;
}

bool ExternalPaletteImpl::isThemeDark() const
{
    if (!m_provider->isThemeDark)
        return false;
    return m_provider->isThemeDark(m_provider);
}

bool ExternalPaletteImpl::hasCSDImages() const
{
    if (!m_provider->supportThemeImage)
        return false;
    return m_provider->supportThemeImage(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON);
}


QImage ExternalPaletteImpl::getCSDImage(vlc_qt_theme_csd_button_type type, vlc_qt_theme_csd_button_state state, bool maximized, bool active, int bannerHeight)
{
    if (!m_provider->getThemeImage)
        return {};
    vlc_qt_theme_image_setting imageSettings;
    imageSettings.windowScaleFactor = m_ctx->intfMainWindow()->devicePixelRatio();
    imageSettings.userScaleFacor = m_ctx->getIntfUserScaleFactor();
    imageSettings.u.csdButton.buttonType = type;
    imageSettings.u.csdButton.state = state;
    imageSettings.u.csdButton.maximized = maximized;
    imageSettings.u.csdButton.active = active;
    imageSettings.u.csdButton.bannerHeight = bannerHeight;
    picture_t* pic = m_provider->getThemeImage(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, &imageSettings);
    if (!pic)
        return {};

    QImage::Format format = QImage::Format_Invalid;
    switch (pic->format.i_chroma)
    {
    case VLC_CODEC_ARGB:
        format = QImage::Format_ARGB32_Premultiplied;
        break;
    case VLC_CODEC_RGBA:
        format = QImage::Format_RGBA8888_Premultiplied;
        break;
    case VLC_CODEC_RGB24:
        format = QImage::Format_RGB888;
        break;
    default:
        msg_Err(m_ctx->getIntf(), "unexpected image format from theme provider");
        break;
    }

    return QImage(pic->p[0].p_pixels,
            pic->format.i_visible_width, pic->format.i_visible_height, pic->p[0].i_pitch,
            format,
            ReleaseVLCPictureCb, pic
            );
}

CSDMetrics* ExternalPaletteImpl::getCSDMetrics() const
{
    return m_csdMetrics.get();
}

int ExternalPaletteImpl::update(vlc_qt_palette_t& p)
{
    if (m_provider->updatePalette)
        return m_provider->updatePalette(m_provider, &p);
    return VLC_EGENERIC;
}

void ExternalPaletteImpl::updateMetrics(vlc_qt_theme_image_type type)
{
    if (m_provider->getThemeMetrics)
    {
        if (type == VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON)
        {
            m_csdMetrics.reset();
            [&](){
                if (!m_provider->getThemeMetrics)
                    return;
                vlc_qt_theme_metrics metrics;
                memset(&metrics, 0, sizeof(metrics));
                bool ret = m_provider->getThemeMetrics(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, &metrics);
                if (!ret)
                    return;
                m_csdMetrics = std::make_unique<CSDMetrics>();
                m_csdMetrics->interNavButtonSpacing = metrics.u.csd.interNavButtonSpacing;

                m_csdMetrics->csdFrameMarginLeft = metrics.u.csd.csdFrameMarginLeft;
                m_csdMetrics->csdFrameMarginRight = metrics.u.csd.csdFrameMarginRight;
                m_csdMetrics->csdFrameMarginTop = metrics.u.csd.csdFrameMarginTop;
                m_csdMetrics->csdFrameMarginBottom = metrics.u.csd.csdFrameMarginBottom;
            }();
            emit CSDMetricsChanged();
        }
    }
}

SystemPalette::SystemPalette(QObject* parent)
    : QObject(parent)
{
    updatePalette();
}

ColorSchemeModel::ColorScheme SystemPalette::source() const
{
    return m_source;
}

QImage SystemPalette::getCSDImage(vlc_qt_theme_csd_button_type type, vlc_qt_theme_csd_button_state state, bool maximized, bool active, int bannerHeight)
{
    if (!hasCSDImage())
        return QImage();
    assert(m_palettePriv);
    return m_palettePriv->getCSDImage(type, state, maximized, active, bannerHeight);

}

CSDMetrics* SystemPalette::getCSDMetrics() const
{
    if (!m_palettePriv)
        return nullptr;
    return m_palettePriv->getCSDMetrics();
}

bool SystemPalette::hasCSDImage() const
{
    if (!m_palettePriv)
        return false;
    return m_palettePriv->hasCSDImages();
}

QColor SystemPalette::blendColors(const QColor& c1, const QColor& c2, float blend)
{
    return ::blendColors(c1, c2, blend);
}
QColor SystemPalette::setColorAlpha(const QColor& c1, float alpha)
{
    return ::setColorAlpha(c1, alpha);
}

void SystemPalette::setSource(ColorSchemeModel::ColorScheme source)
{
    if (m_source == source)
        return;
    m_source = source;

    updatePalette();

    emit sourceChanged();
}

void SystemPalette::setCtx(MainCtx* ctx)
{
    if (ctx == m_ctx)
        return;
    m_ctx = ctx;
    emit ctxChanged();
    updatePalette();
}

void SystemPalette::updatePalette()
{
    m_palettePriv.reset();
    switch(m_source)
    {
    case ColorSchemeModel::System:
        makeSystemPalette();
        break;
    case ColorSchemeModel::Day:
        makeLightPalette();
        break;
    case ColorSchemeModel::Night:
        makeDarkPalette();
        break;
    default:
        break;
    }

    if (m_palettePriv)
    {
        connect(
            m_palettePriv.get(), &ExternalPaletteImpl::paletteChanged,
            this, &SystemPalette::updatePalette);
        connect(
            m_palettePriv.get(), &ExternalPaletteImpl::CSDMetricsChanged,
            this, &SystemPalette::CSDMetricsChanged);
    }
    emit paletteChanged();

    bool hasCSDImage = m_palettePriv && m_palettePriv->hasCSDImages();
    if (m_hasCSDImage != hasCSDImage)
    {
        m_hasCSDImage = hasCSDImage;
        emit hasCSDImageChanged();
    }
}

void SystemPalette::makeLightPalette()
{
    //QColor grey1 = QColor{"#9E9E9E"};
    QColor grey2 = QColor{"#666666"};

    m_isDark = false;

    m_text =  QColor{"#232627"};
    m_textInactive =  QColor{"#7f8c8d"};
    m_textDisabled = "#bdc3c7";

    m_bg =  lightGrey100;
    m_bgInactive =  QColor{"#fcfdfc"};

    m_bgAlt =  lightGrey400;
    m_bgAltInactive =  lightGrey400;

    m_bgHover =  lightGrey400;
    m_bgHoverText =  m_text;
    m_bgHoverInactive =  QColor{"#3daee9"};
    m_bgHoverTextInactive =  m_text;

    m_bgFocus =  Qt::black;

    m_button =  QColor{"#eff0f1"};
    m_buttonText =  m_text;
    m_buttonBorder =  ::blendColors(m_button, m_buttonText, 0.8);

    m_textActiveSource =  QColor{"#ff950d"};

    m_topBanner =  lightGrey400;
    m_lowerBanner =  Qt::white;

    m_accent =  orange800;

    m_alert = QColor{"#d70022"};

    m_separator =  lightGrey400;
    m_playerControlBarFg =  QColor{"#333333"};
    m_expandDelegate =  Qt::white;

    m_tooltipTextColor = Qt::black;
    m_tooltipColor = Qt::white;

    m_border = QColor{"#e0e0e0"};
    m_buttonHover = lightGrey300;
    m_buttonBanner = grey2;
    m_buttonPrimaryHover = QColor{"#e65609"};
    m_buttonPlayer = QColor{"#484848"};
    m_grid = lightGrey400;
    m_gridSelect = lightGrey600;
    m_listHover = lightGrey500;
    m_textField = QColor{"#999999"};
    m_textFieldHover = QColor{"#4c4c4c"};
    m_icon = QColor{"#616161"};
    m_sliderBarMiniplayerBgColor = QColor{"#FFEEEEEE"};
    m_windowCSDButtonBg = QColor{"#80DADADA"};
}

void SystemPalette::makeDarkPalette()
{
    m_isDark = true;

    //QColor grey1 = QColor{"#666666"};
    //QColor grey2 = QColor{"#AAAAAA"};

    m_text = "#eff0f1";
    m_textInactive = "#bdc3c7";
    m_textDisabled = "#bdc3c7";

    m_bg = darkGrey200;
    m_bgInactive = "#232629";

    m_bgAlt = darkGrey400;
    m_bgAltInactive = darkGrey300;

    m_bgHover = darkGrey800;
    m_bgHoverInactive = "#3daee9";

    m_bgHoverText = m_text;
    m_bgHoverTextInactive = m_text;

    m_bgFocus = Qt::white;

    m_button = "#31363b";
    m_buttonText = m_text;
    m_buttonBorder = "#575b5f";

    m_textActiveSource = "#ff950d";

    m_topBanner = darkGrey400;
    m_lowerBanner = Qt::black;

    m_accent = orange500;

    m_alert = QColor{"#d70022"};

    m_separator = darkGrey700;

    m_playerControlBarFg = Qt::white;

    m_expandDelegate = Qt::black;

    m_tooltipTextColor = Qt::white;
    m_tooltipColor = Qt::black;

    m_border = darkGrey800;
    m_buttonHover = darkGrey800;
    m_buttonBanner = QColor("#a6a6a6");
    m_buttonPrimaryHover = QColor{"#e67A00"};
    m_buttonPlayer = lightGrey600;
    m_grid = darkGrey500;
    m_gridSelect = darkGrey800;
    m_listHover = darkGrey500;
    m_textField = QColor{"#6f6f6f"};
    m_textFieldHover = QColor{"#b7b7b7"};
    m_icon = Qt::white;
    m_sliderBarMiniplayerBgColor = QColor{"#FF929292"};
    m_windowCSDButtonBg =  QColor{"#80484848"};
}

void SystemPalette::makeSystemPalette()
{
    if (!m_ctx)
    {
        //can't initialise system palette, fallback to default
        makeLightPalette();
        return;
    }

    auto palette = std::make_unique<ExternalPaletteImpl>(m_ctx);
    if (!palette->init())
    {
        //can't initialise system palette, fallback to default
        makeLightPalette();
        return;
    }

    vlc_qt_palette_t p;
#define BIND_COLOR(name)  p. name = &m_##name;
    VLC_QT_INTF_PUBLIC_COLORS(BIND_COLOR)
#undef BIND_COLOR

    int ret = palette->update(p);
    if (ret != VLC_SUCCESS)
    {
        if (palette->isThemeDark())
            makeDarkPalette();
        else
            makeLightPalette();
    }

    m_palettePriv = std::move(palette);
}
