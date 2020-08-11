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

    property color text: systemPalette.text;
    property color textInactive: systemPalette.textInactive;
    property color textDisabled: systemPalette.textDisabled;

    property color caption: setColorAlpha(text, .4)
    property color menuCaption: setColorAlpha(text, .6)

    property color bg: systemPalette.base;
    property color bgInactive: systemPalette.baseInactive;

    //for alternate rows
    property color bgAlt: systemPalette.alternateBase;
    property color bgAltInactive: systemPalette.alternateBaseInactive;

    property color bgHover: systemPalette.highlight;
    property color bgHoverInactive: systemPalette.highlightInactive;

    property color button: systemPalette.button;
    property color buttonText: systemPalette.buttonText;
    property color buttonBorder: blendColors(systemPalette.button, systemPalette.buttonText, 0.8);

    property color textActiveSource: "red";

    property color banner: systemPalette.window;
    property color bannerHover: systemPalette.highlight;
    property color volsliderbg: "#bdbebf"
    property color volbelowmid: "#99d299"
    property color volabovemid: "#14d214"
    property color volhigh: "#ffc70f"
    property color volmax: "#f5271d"

    property color playerFg: "white"
    property color playerFgInactive: "#888888"
    property color playerBg: "black"
    property color playerBorder: "#222222"

    property color separator: blendColors(bg, text, .95)

    property color playlistSeparator: colors_id.white

    // basic color definitions for color blending:
    property color black: "black"
    property color white: "white"

    // glow colors:
    property color glowColor: setColorAlpha(blendColors(bg, black, 0.8), 0.35)
    property color glowColorBanner: setColorAlpha(blendColors(banner, black, isThemeDark ? 0.25 : 0.35), 0.25)

    //vlc orange
    property color accent: "#FFFF950D";

    property color alert: "red";

    property color buffer: "#696969";

    property color seekpoint: "red";

    property var colorSchemes: ["system", "day", "night"]

    state: settings.VLCStyle_colors_state
    states: [
        //other styles are provided for testing purpose
        State {
            name: "day"
            PropertyChanges {
                target: colors_id

                text: "#232627"
                textInactive: "#7f8c8d"

                bg: "#fcfdfc"
                bgInactive: "#fcfdfc"

                bgAlt: "#eff0f1"
                bgAltInactive: "#eff0f1"

                bgHover: "#ededed"
                bgHoverInactive: "#3daee9"

                button: "#eff0f1";
                buttonText: "#232627";
                buttonBorder: blendColors(button, buttonText, 0.8);

                textActiveSource: "#ff950d";

                banner: "#eff0f1";
                bannerHover: "#3daee9";

                accent: "#ff950d";
                alert: "#ff0000";
                separator: "#ededed";

                isThemeDark: false;
            }
        },
        State {
            name: "night"
            PropertyChanges {
                target: colors_id

                text: "#eff0f1"
                textInactive: "#bdc3c7"
                bg: "#232629"
                bgInactive: "#232629"
                bgAlt: "#31363b"
                bgAltInactive: "#31363b"
                bgHover: "#2d2d2d"
                bgHoverInactive: "#3daee9"
                button: "#31363b"
                buttonText: "#eff0f1"
                buttonBorder: "#575b5f"
                textActiveSource: "#ff950d"
                banner: "#31363b"
                bannerHover: "#3daee9"
                accent: "#ff950d"
                alert: "#ff0000"
                separator: "#2d2d2d"
                isThemeDark: true
            }
        },
        State {
            name: "system"
            PropertyChanges {
                target: colors_id

                bg: systemPalette.base
                bgInactive: systemPalette.baseInactive

                bgAlt: systemPalette.alternateBase
                bgAltInactive: systemPalette.alternateBaseInactive

                bgHover: systemPalette.highlight
                bgHoverInactive: systemPalette.highlightInactive

                text: systemPalette.text
                textDisabled: systemPalette.textDisabled
                textInactive: systemPalette.textInactive

                button: systemPalette.button
                buttonText: systemPalette.buttonText
                buttonBorder: blendColors(button, buttonText, 0.8)

                textActiveSource: accent
                banner: systemPalette.window
                bannerHover: systemPalette.highlight

                separator: blendColors(bg, text, .95)

                isThemeDark: systemPalette.isDark
            }
        }
    ]
}
