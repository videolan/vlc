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
#ifndef VLC_COLOR_CONTEXT_H
#define VLC_COLOR_CONTEXT_H

#include <memory>

#include <QObject>
#include <QColor>
#include <QQuickItem>
#include <QPointer>
#include <QQmlParserStatus>

#include "qtthemeprovider.hpp"

Q_MOC_INCLUDE("style/systempalette.hpp")

class SystemPalette;
class ColorContext;
class ColorContextState;

class ColorProperty {
    Q_GADGET

    Q_PROPERTY(QColor primary READ primary CONSTANT FINAL)
    Q_PROPERTY(QColor secondary READ secondary CONSTANT FINAL)
    Q_PROPERTY(QColor highlight READ highlight CONSTANT FINAL)
    Q_PROPERTY(QColor link READ link CONSTANT FINAL)

    Q_PROPERTY(QColor positive READ positive CONSTANT FINAL)
    Q_PROPERTY(QColor neutral READ neutral CONSTANT FINAL)
    Q_PROPERTY(QColor negative READ negative CONSTANT FINAL)

public:
    ColorProperty() = default;
    ColorProperty(const ColorContext* context, int section);

    QColor primary() const;
    QColor secondary() const;
    QColor highlight() const;
    QColor link() const;
    QColor positive() const;
    QColor neutral() const;
    QColor negative() const;

private:
    const ColorContext* m_context;
    int m_section;
};

Q_DECLARE_METATYPE(ColorProperty)

class ColorContext : public QObject, public QQmlParserStatus {
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

public:
    enum ColorSet {
        View  = VQTC_SET_VIEW ,
        Window = VQTC_SET_WINDOW,
        Item = VQTC_SET_ITEM,
        Badge = VQTC_SET_BADGE,
        ButtonStandard = VQTC_SET_BUTTON_STANDARD,
        ButtonAccent= VQTC_SET_BUTTON_ACCENT,
        ToolButton = VQTC_SET_TOOL_BUTTON,
        TabButton = VQTC_SET_TAB_BUTTON,
        MenuBar = VQTC_SET_MENUBAR,
        Tooltip = VQTC_SET_TOOLTIP,
        Slider = VQTC_SET_SLIDER,
        ComboBox = VQTC_SET_COMBOBOX,
        SpinBox = VQTC_SET_SPINBOX,
        TextField = VQTC_SET_TEXTFIELD,
        Switch = VQTC_SET_SWITCH,
        ColorSetUndefined = -1
    };
    Q_ENUM(ColorSet)

    enum ColorState {
        Normal = VQTC_STATE_NORMAL,
        Disabled = VQTC_STATE_DISABLED,
        Pressed = VQTC_STATE_PRESSED,
        Hovered = VQTC_STATE_HOVERED,
        Focused =  VQTC_STATE_FOCUSED
    };

    enum ColorSection {
        Fg = VQTC_SECTION_FG,
        Bg = VQTC_SECTION_BG,
        Decoration = VQTC_SECTION_DECORATION
    };

    enum ColorName {
        Primary = VQTC_NAME_PRIMARY,
        Secondary = VQTC_NAME_SECONDARY,
        Highlight = VQTC_NAME_HIGHLIGHT,
        Link = VQTC_NAME_LINK,
        Positive = VQTC_NAME_POSITIVE,
        Neutral = VQTC_NAME_NEUTRAL,
        Negative = VQTC_NAME_NEGATIVE,

        //roles for decorations
        VisualFocus = VQTC_NAME_VISUAL_FOCUS,
        Border = VQTC_NAME_BORDER,
        Accent = VQTC_NAME_ACCENT,
        Shadow = VQTC_NAME_SHADOW,
        Separator = VQTC_NAME_SEPARATOR,
        Indicator = VQTC_NAME_INDICATOR
    };

    Q_PROPERTY(SystemPalette* palette READ palette WRITE setPalette NOTIFY paletteChanged FINAL)
    Q_PROPERTY(ColorSet colorSet READ colorSet WRITE setColorSet NOTIFY colorSetChanged FINAL)

    // enabled/disabled buttons for instance, true by default
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    // mouse hover
    Q_PROPERTY(bool hovered READ hovered WRITE setHovered NOTIFY hoveredChanged FINAL)
    // keyboard focused
    Q_PROPERTY(bool focused READ focused WRITE setFocused NOTIFY focusedChanged FINAL)
    // intermediate state when user, false by default
    Q_PROPERTY(bool pressed READ pressed WRITE setPressed NOTIFY pressedChanged FINAL)

    //exposed colors
    Q_PROPERTY(ColorProperty bg READ bg NOTIFY colorsChanged FINAL)
    Q_PROPERTY(ColorProperty fg READ fg NOTIFY colorsChanged FINAL)

    //decoration colors
    Q_PROPERTY(QColor visualFocus READ visualFocus NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor border READ border NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor separator READ separator NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor indicator READ indicator NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor accent READ accent NOTIFY colorsChanged FINAL)


    Q_PROPERTY(bool initialized READ initialized NOTIFY initializedChanged FINAL)
public:
    explicit ColorContext(QObject* parent = nullptr);

    QColor getColor(ColorSection section, ColorName color) const;

    SystemPalette* palette() { return m_palette; }
    ColorSet colorSet() { return m_colorSet; }
    bool enabled() const;
    bool hovered() const;
    bool focused() const;
    bool pressed() const;
    inline ColorProperty bg() const { return ColorProperty{ this, Bg }; }
    inline ColorProperty fg() const { return ColorProperty{ this, Fg }; }
    QColor visualFocus() const;
    QColor border() const;
    QColor separator() const;
    QColor indicator() const;
    QColor shadow() const;
    QColor accent() const;

    void setPalette(SystemPalette*);
    void setColorSet(ColorSet);

    void setEnabled(bool);
    void setHovered(bool);
    void setFocused(bool);
    void setPressed(bool);

    bool hasExplicitPalette() const { return m_hasExplicitPalette; }
    bool hasExplicitColorSet() const { return m_hasExplicitColorSet; }
    bool hasExplicitState() const { return m_hasExplicitColorSet; }
    bool initialized() const { return m_initialized; }

signals:
    void colorsChanged();
    void paletteChanged(SystemPalette*);
    void colorSetChanged(ColorSet);
    void enabledChanged();
    void hoveredChanged();
    void focusedChanged();
    void pressedChanged();
    void initializedChanged();

    void sharedStateChanged(QPrivateSignal);

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    void onParentChanged();
    void setParentContext(ColorContext* parentContext);

    void ensureExplicitState();
    void onInheritedStateChanged();

    bool setInheritedPalette(SystemPalette*);
    bool setInheritedColorSet(ColorSet);
    bool setInheritedState(std::shared_ptr<ColorContextState>& state);

    SystemPalette* m_palette = nullptr;
    std::shared_ptr<ColorContextState> m_state;

    QPointer<ColorContext> m_parentContext;

    ColorSet m_colorSet = ColorSet::ColorSetUndefined;

    bool m_initialized = false;
    bool m_hasExplicitPalette = false;
    bool m_hasExplicitColorSet = false;
    bool m_hasExplicitState = false;
};

class ColorContextState : public QObject
{
    Q_OBJECT

signals:
    void stateChanged();
    void colorsChanged();
    void enabledChanged();
    void hoveredChanged();
    void focusedChanged();
    void pressedChanged();

public:
    void updateState();
    ColorContext::ColorState m_state = ColorContext::Normal;
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_focused = false;
};

#endif // VLC_COLOR_CONTEXT_H
