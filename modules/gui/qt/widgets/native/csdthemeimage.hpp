#ifndef CSDTHEMEIMAGE_H
#define CSDTHEMEIMAGE_H

#include <QQuickItem>
#include "style/systempalette.hpp"

class CSDThemeImage : public QQuickItem
{
    Q_OBJECT

public:
    enum ButtonType {
        MAXIMIZE = VLC_QT_THEME_BUTTON_MAXIMIZE,
        MINIMIZE = VLC_QT_THEME_BUTTON_MINIMIZE,
        RESTORE = VLC_QT_THEME_BUTTON_RESTORE,
        CLOSE = VLC_QT_THEME_BUTTON_CLOSE,
    };
    Q_ENUM(ButtonType)

    enum ButtonState {
        DISABLED  = VLC_QT_THEME_BUTTON_STATE_DISABLED ,
        HOVERED = VLC_QT_THEME_BUTTON_STATE_HOVERED,
        NORMAL = VLC_QT_THEME_BUTTON_STATE_NORMAL,
        PRESSED = VLC_QT_THEME_BUTTON_STATE_PRESSED,
    };
    Q_ENUM(ButtonState)

    Q_PROPERTY(SystemPalette* theme READ  getTheme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(ButtonType buttonType READ  getButtonType WRITE setButtonType NOTIFY buttonTypeChanged)
    Q_PROPERTY(ButtonState buttonState READ  getButtonState WRITE setButtonState NOTIFY buttonStateChanged)
    Q_PROPERTY(int bannerHeight READ getBannerHeight WRITE setBannerHeight NOTIFY bannerHeightChanged)
    Q_PROPERTY(bool windowMaximized READ getWindowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)
    Q_PROPERTY(bool windowActive READ getWindowActive WRITE setWindowActive NOTIFY windowActiveChanged)


public:
    CSDThemeImage(QQuickItem *parent = nullptr);

protected:

    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

signals:
    void requestContentUpdate();

    void themeChanged();
    void buttonTypeChanged();
    void buttonStateChanged();
    void bannerHeightChanged();
    void windowMaximizedChanged();
    void windowActiveChanged();

private slots:
    void updateContent();

public:
    inline SystemPalette* getTheme() const { return  m_theme; }
    inline ButtonType getButtonType() const { return  m_buttonType; }
    inline ButtonState getButtonState() const { return  m_buttonState; }
    inline int getBannerHeight() const { return  m_bannerHeight; }
    inline bool getWindowMaximized() const {  return m_maximized;  }
    inline bool getWindowActive() const {  return m_active;  }

    void setTheme(SystemPalette*);
    void setButtonType(ButtonType);
    void setButtonState(ButtonState);
    void setBannerHeight(int);
    void setWindowMaximized(bool);
    void setWindowActive(bool);

private:
    void onConfigChanged();

    std::vector<QImage> m_imageSet;

    SystemPalette* m_theme = nullptr;
    ButtonType m_buttonType = CLOSE;
    ButtonState m_buttonState = NORMAL;
    bool m_maximized = true;
    bool m_active = true;
    int m_bannerHeight = -1;

    bool m_needContentUpdate = false;
    bool m_stateUpdated = false;
};

#endif // CSDTHEMEIMAGE_H
