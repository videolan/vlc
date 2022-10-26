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

import org.videolan.vlc 0.1

Item {
    id: colors_id

    /* required*/ property var palette

    //"alias" ColorHelper functions
    property var blendColors: palette.blendColors
    property var setColorAlpha: palette.setColorAlpha

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

    readonly property bool isThemeDark: palette.isDark

    readonly property color text: palette.text;
    readonly property color textInactive: palette.textInactive;
    readonly property color textDisabled: palette.textDisabled;

    readonly property color caption: setColorAlpha(text, .4)
    readonly property color menuCaption: setColorAlpha(text, .6)

    readonly property color bg: palette.bg;
    readonly property color bgInactive: palette.bgInactive;

    //for alternate rows
    readonly property color bgAlt: palette.bgAlt;
    readonly property color bgAltInactive: palette.bgAltInactive;

    readonly property color bgHover: palette.bgHover;
    readonly property color bgHoverText: palette.bgHoverText;
    readonly property color bgHoverInactive: palette.bgHoverInactive;
    readonly property color bgHoverTextInactive: palette.bgHoverTextInactive;

    readonly property color bgFocus: palette.bgFocus

    // Banner

    readonly property color border: palette.border

    // Button

    readonly property color button: palette.button

    readonly property color buttonHover: palette.buttonHover

    readonly property color buttonText: palette.buttonText
    readonly property color buttonTextHover: bgFocus

    readonly property color buttonBorder: blendColors(palette.button, palette.buttonText, 0.8)

    // ButtonBanner (BannerTabButton)

    readonly property color buttonBannerDark: "#a6a6a6"

    readonly property color buttonBanner: palette.buttonBanner

    // ButtonPrimary (ActionButtonPrimary)

    readonly property color buttonPrimaryHover: palette.buttonPrimaryHover

    // ButtonPlayer (IconControlButton)

    readonly property color buttonPlayer: palette.buttonPlayer

    // ButtonPlay (ControlButtons)

    readonly property color buttonPlayA: "#f89a06"
    readonly property color buttonPlayB: "#e25b01"

    readonly property color buttonPlayIcon: "#333333"

    // GridItem

    // NOTE: This needs to contrast with the background because we have no border.
    readonly property color grid: palette.grid

    readonly property color gridSelect: palette.gridSelect

    // ListItem

    readonly property color listHover: palette.listHover

    // TrackItem (CheckedDelegate)

    readonly property color trackItem: palette.darkGrey800
    readonly property color trackItemHover: palette.darkGrey600

    // TextField

    readonly property color textField: palette.textField
    readonly property color textFieldHover: palette.textFieldHover

    readonly property color icon: palette.icon

    readonly property color textActiveSource: "red";

    readonly property color topBanner: palette.topBanner

    readonly property color lowerBanner: palette.lowerBanner

    readonly property color volsliderbg: "#bdbebf"
    readonly property color volbelowmid: "#99d299"
    readonly property color volabovemid: "#14d214"
    readonly property color volhigh: "#ffc70f"
    readonly property color volmax: "#f5271d"

    readonly property color playerFg: text
    readonly property color playerFgInactive: textInactive
    readonly property color playerControlBarFg: playerFg
    readonly property color playerBg: bg
    readonly property color playerSeekBar: Qt.lighter(playerBg, 1.6180)
    readonly property color playerBorder: buttonText

    readonly property color separator: blendColors(bg, text, .95)

    readonly property color roundPlayCoverBorder: "#979797"

    // basic color definitions for color blending:
    readonly property color black: "black"
    readonly property color white: "white"

    // glow colors:
    readonly property color glowColor: setColorAlpha(blendColors(bg, black, 0.8), 0.35)
    readonly property color glowColorBanner: setColorAlpha(blendColors(topBanner, black, isThemeDark ? 0.25 : 0.35), 0.25)

    readonly property color sliderBarMiniplayerBgColor: palette.sliderBarMiniplayerBgColor

    readonly property color tooltipTextColor: palette.tooltipTextColor
    readonly property color tooltipColor: palette.tooltipColor

    //vlc orange
    readonly property color accent: palette.accent

    readonly property color alert: palette.alert;

    readonly property color buffer: "#696969";

    readonly property color seekpoint: "red";
    readonly property color record: "red";

    readonly property color windowCSDButtonBg: palette.windowCSDButtonBg

    readonly property color expandDelegate: palette.expandDelegate
}
