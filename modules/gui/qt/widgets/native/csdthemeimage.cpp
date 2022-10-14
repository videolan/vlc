#include <QQuickWindow>
#include <QSGImageNode>

#include "csdthemeimage.hpp"
#include <maininterface/mainctx.hpp>


CSDThemeImage::CSDThemeImage(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_imageSet.resize(VLC_QT_THEME_BUTTON_STATE_COUNT);
    connect(this, &CSDThemeImage::requestContentUpdate, this, &CSDThemeImage::updateContent, Qt::QueuedConnection);
}

void CSDThemeImage::updateContent()
{
    for (int i = 0; i < VLC_QT_THEME_BUTTON_STATE_COUNT; i++)
    {
        m_imageSet[i] = m_theme->getCSDImage(
                    static_cast<vlc_qt_theme_csd_button_type>(m_buttonType),
                    static_cast<vlc_qt_theme_csd_button_state>(i),
                    m_maximized, m_active, m_bannerHeight);
    }
    m_needContentUpdate = false;
    m_stateUpdated = true;
    update();
}

QSGNode* CSDThemeImage::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    QSGImageNode *imageNode = nullptr;;
    auto w = window();
    if (!m_theme || m_bannerHeight <= 0)
    {
        if (oldNode)
            delete oldNode;
        return nullptr;
    }

    if (!oldNode)
    {
        imageNode = w->createImageNode();
        imageNode->setOwnsTexture(true);
        m_stateUpdated = true;
    }
    else
    {
        imageNode = static_cast<QSGImageNode*>(oldNode);
    }

    if (m_stateUpdated)
    {
        const QImage& image = m_imageSet[m_buttonState];
        if (image.isNull())
        {
            delete imageNode;
            return nullptr;
        }

        setImplicitSize(image.width(), image.height());

        QQuickWindow::CreateTextureOptions flags = image.hasAlphaChannel()
                ? QQuickWindow::TextureHasAlphaChannel
                : QQuickWindow::TextureIsOpaque;
        flags |= QQuickWindow::TextureCanUseAtlas;

        imageNode->setTexture(window()->createTextureFromImage(image, flags));

        m_stateUpdated = false;
    }

    if (!imageNode->texture())
    {
        delete imageNode;
        return nullptr;
    }

    imageNode->setRect(boundingRect());
    imageNode->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    return imageNode;
}

void CSDThemeImage::setWindowActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit windowActiveChanged();
    onConfigChanged();
}

void CSDThemeImage::setWindowMaximized(bool maximized)
{
    if (m_maximized == maximized)
        return;
    m_maximized = maximized;
    emit windowMaximizedChanged();
    onConfigChanged();
}

void CSDThemeImage::setTheme(SystemPalette* val)
{
    if (val == m_theme)
        return;
    if (m_theme)
        disconnect(m_theme, &SystemPalette::hasCSDImageChanged, this, &CSDThemeImage::onConfigChanged);
    m_theme = val;
    if (m_theme)
        connect(m_theme, &SystemPalette::hasCSDImageChanged, this, &CSDThemeImage::onConfigChanged);
    emit themeChanged();
    onConfigChanged();
}
void CSDThemeImage::setButtonType(ButtonType val)
{
    if (val == m_buttonType)
        return;
    m_buttonType = val;
    emit buttonTypeChanged();
    onConfigChanged();
}
void CSDThemeImage::setButtonState(ButtonState val)
{
    if (val == m_buttonState)
        return;
    m_buttonState = val;
    m_stateUpdated = true;
    emit buttonStateChanged();
    update();
}
void CSDThemeImage::setBannerHeight(int val)
{
    if (val == m_bannerHeight)
        return;
    m_bannerHeight = val;
    emit bannerHeightChanged();
    onConfigChanged();
}

void CSDThemeImage::onConfigChanged()
{
    if (!m_needContentUpdate)
    {
        m_needContentUpdate = true;
        emit requestContentUpdate();
    }
}
