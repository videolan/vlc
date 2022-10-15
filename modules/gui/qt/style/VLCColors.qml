/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick 2.11

import org.videolan.vlc 0.1 as VLC

Item {
    id: colors_id

    function blendColors( a, b, blend ) {
        return Qt.rgba( a.r * blend + b.r * (1. - blend),
                       a.g * blend + b.g * (1. - blend),
                       a.b * blend + b.b * (1. - blend),
                       a.a * blend + b.a * (1. - blend))
    }

    function setColorAlpha( c, alpha )
    {
        return Qt.rgba(c.r, c.g, c.b, alpha)
    }

    function getBgColor(selected, hovered, focus)
    {
        if (focus)
            return accent
        if ( selected )
            return bgHoverInactive
        else if (hovered)
            return bgHoverInactive
        else
            return "transparent"
    }

    property bool  isThemeDark: false

    property color text: VLC.SystemPalette.text;
    property color textInactive: VLC.SystemPalette.textInactive;
    property color textDisabled: VLC.SystemPalette.textDisabled;

    property color caption: setColorAlpha(text, .4)
    property color menuCaption: setColorAlpha(text, .6)

    property color bg: VLC.SystemPalette.base;
    property color bgInactive: VLC.SystemPalette.baseInactive;

    //for alternate rows
    property color bgAlt: VLC.SystemPalette.alternateBase;
    property color bgAltInactive: VLC.SystemPalette.alternateBaseInactive;

    property color bgHover: VLC.SystemPalette.highlight;
    property color bgHoverText: VLC.SystemPalette.highlightText;
    property color bgHoverInactive: VLC.SystemPalette.highlightInactive;
    property color bgHoverTextInactive: VLC.SystemPalette.highlightTextInactive;

    property color bgFocus: (isThemeDark) ? white : black

    // Banner

    property color border: (isThemeDark) ? "#303030" : "#e0e0e0"

    // Button

    property color button: VLC.SystemPalette.button

    property color buttonHover: (isThemeDark) ? "#303030" : "#f2f2f2"

    property color buttonText: VLC.SystemPalette.buttonText
    property color buttonTextHover: bgFocus

    property color buttonBorder: blendColors(VLC.SystemPalette.button, VLC.SystemPalette.buttonText, 0.8)

    // ButtonBanner (BannerTabButton)

    property color buttonBannerDark: "#a6a6a6"

    property color buttonBanner: (isThemeDark) ? buttonBannerDark : "#666666"

    // ButtonPrimary (ActionButtonPrimary)

    property color buttonPrimaryHover: (isThemeDark) ? "#e67A00" : "#e65609"

    // ButtonPlayer (IconControlButton)

    property color buttonPlayer: (isThemeDark) ? "#e5e5e5" : "#484848"

    // ButtonPlay (ControlButtons)

    property color buttonPlayA: "#f89a06"
    property color buttonPlayB: "#e25b01"

    property color buttonPlayIcon: "#333333"

    // GridItem

    // NOTE: This needs to contrast with the background because we have no border.
    property color grid: (isThemeDark) ? "#272727" : "#ededed"

    property color gridSelect: (isThemeDark) ? "#303030" : "#e5e5e5"

    // ListItem

    property color listHover: (isThemeDark) ? "#272727" : "#e9e9e9"

    // TrackItem (CheckedDelegate)

    property color trackItem: "#303030"
    property color trackItemHover: "#2a2a2a"

    // TextField

    property color textField: (isThemeDark) ? "#6f6f6f" : "#999999"
    property color textFieldHover: (isThemeDark) ? "#b7b7b7" : "#4c4c4c"

    property color icon: isThemeDark ? white : "#616161"

    property color textActiveSource: "red";

    property color topBanner: VLC.SystemPalette.window
    property color topBannerHover: VLC.SystemPalette.highlight

    property color lowerBanner: bg

    property color volsliderbg: "#bdbebf"
    property color volbelowmid: "#99d299"
    property color volabovemid: "#14d214"
    property color volhigh: "#ffc70f"
    property color volmax: "#f5271d"

    property color playerFg: text
    property color playerFgInactive: textInactive
    property color playerControlBarFg: playerFg
    property color playerBg: bg
    property color playerSeekBar: Qt.lighter(playerBg, 1.6180)
    property color playerBorder: buttonText

    property color separator: blendColors(bg, text, .95)
    
    property color roundPlayCoverBorder: "#979797"

    // playlist
    property color plItemHovered:  topBannerHover
    property color plItemSelected: blendColors(plItemHovered, plItemFocused, 0.5)
    property color plItemFocused: isThemeDark ? "#1E1E1E" : "#EDEDED"

    // basic color definitions for color blending:
    property color black: "black"
    property color white: "white"

    // glow colors:
    property color glowColor: setColorAlpha(blendColors(bg, black, 0.8), 0.35)
    property color glowColorBanner: setColorAlpha(blendColors(topBanner, black, isThemeDark ? 0.25 : 0.35), 0.25)

    property color sliderBarMiniplayerBgColor: isThemeDark ? "#FF929292" : "#FFEEEEEE"

    property color tooltipTextColor: isThemeDark ? white : black
    property color tooltipColor: isThemeDark ? black : white

    //vlc orange
    property color accent: (isThemeDark) ? "#ff8800" : "#ff610a"

    property color accentText: "#ffffff";

    property color alert: "#d70022";

    property color buffer: "#696969";

    property color seekpoint: "red";
    property color record: "red";

    property color windowCSDButtonDarkBg:  "#80484848"
    property color windowCSDButtonLightBg: "#80DADADA"
    property color windowCSDButtonBg: isThemeDark ? windowCSDButtonDarkBg : windowCSDButtonLightBg

    property color expandDelegate

    state: {
        switch (VLC.MainCtx.colorScheme.scheme) {
        case VLC.ColorSchemeModel.System:
            return "system"
        case VLC.ColorSchemeModel.Day:
            return "day"
        case VLC.ColorSchemeModel.Night:
            return "night"
        default:
            console.assert(false, "Unknown color scheme")
        }
    }

    states: [
        //other styles are provided for testing purpose
        State {
            name: "day"
            PropertyChanges {
                target: colors_id

                text: "#232627"
                textInactive: "#7f8c8d"

                bg: "#fafafa"
                bgInactive: "#fcfdfc"

                bgAlt: "#ededed"
                bgAltInactive: "#ededed"

                bgHover: "#ededed"
                bgHoverText: text
                bgHoverInactive: "#3daee9"
                bgHoverTextInactive: text
                bgFocus: "black"

                button: "#eff0f1";
                buttonText: "#232627";
                buttonBorder: blendColors(button, buttonText, 0.8);

                textActiveSource: "#ff950d";

                topBanner: "#ededed"
                topBannerHover: "#f2f2f2"
                lowerBanner: "#ffffff"

                accent: "#ff610a";
                separator: "#ededed";

                playerControlBarFg: "#333333"

                expandDelegate: "#ffffff"

                isThemeDark: false;
            }
        },
        State {
            name: "night"
            PropertyChanges {
                target: colors_id

                text: "#eff0f1"
                textInactive: "#bdc3c7"
                bg: "#1e1e1e"
                bgInactive: "#232629"
                bgAlt: "#242424"
                bgAltInactive: "#212121"
                bgHover: "#303030"
                bgHoverText: text
                bgHoverInactive: "#3daee9"
                bgHoverTextInactive: text
                bgFocus: "white"
                button: "#31363b"
                buttonText: "#eff0f1"
                buttonBorder: "#575b5f"
                textActiveSource: "#ff950d"
                topBanner: "#242424"
                topBannerHover: "#272727"
                lowerBanner: "#000000"
                accent: "#ff8800"
                separator: "#2d2d2d"
                playerControlBarFg: "#ffffff"

                expandDelegate: "#000000"

                isThemeDark: true
            }
        },
        State {
            name: "system"
            PropertyChanges {
                target: colors_id

                bg: VLC.SystemPalette.base
                bgInactive: VLC.SystemPalette.baseInactive

                bgAlt: VLC.SystemPalette.alternateBase
                bgAltInactive: VLC.SystemPalette.alternateBaseInactive

                bgHover: VLC.SystemPalette.highlight
                bgHoverText: VLC.SystemPalette.highlightText
                bgHoverInactive: VLC.SystemPalette.highlightInactive
                bgHoverTextInactive: VLC.SystemPalette.highlightTextInactive

                text: VLC.SystemPalette.text
                textDisabled: VLC.SystemPalette.textDisabled
                textInactive: VLC.SystemPalette.textInactive

                button: VLC.SystemPalette.button
                buttonText: VLC.SystemPalette.buttonText
                buttonBorder: blendColors(button, buttonText, 0.8)

                textActiveSource: accent

                topBanner: VLC.SystemPalette.window
                topBannerHover: VLC.SystemPalette.highlight
                lowerBanner: VLC.SystemPalette.base

                separator: blendColors(bg, text, .95)

                playerControlBarFg: VLC.SystemPalette.text

                expandDelegate: bg

                isThemeDark: VLC.SystemPalette.isDark

                tooltipColor: VLC.SystemPalette.tooltip
                tooltipTextColor: VLC.SystemPalette.tooltipText
            }
        }
    ]
}
