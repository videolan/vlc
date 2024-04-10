/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers

ColumnLayout {
    id: root

    property alias slider: slider

    // Private

    property var _model: [{ "value": 0.25 },
                          { "value": 0.5 },
                          { "value": 0.75 },
                          { "value": 1, "title": qsTr("Normal") },
                          { "value": 1.25 },
                          { "value": 1.5 },
                          { "value": 1.75 },
                          { "value": 2 }]

    // NOTE: 0.96 and 1.04 are useful for video enthusiasts.
    property var _values: [ 0.25, 0.5, 0.75, 0.96, 1.0, 1.04, 1.25, 1.5, 1.75, 2 ]

    property bool _update: true

    // FIXME: Currently we are not updating the ShiftModifier status while dragging. This could be
    //        fixed with a custom Slider based on a MouseArea.
    property bool _shiftPressed: false

    property real _value: 1.0

    property real _minimum: 0.25
    property real _maximum: 4.0

    property real _threshold: 0.03

    property color _color: (slider.visualFocus || slider.pressed) ? theme.accent
                                                                  : theme.fg.primary

    // Settings

    Layout.fillWidth: true
    Layout.fillHeight: true

    // Events

    Component.onCompleted: _updateValue(Player.rate)

    // Function

    function _updateComboBox(value) {
        // NOTE: We want a rounded 1.xx value.
        value = Math.round(value * 100) / 100

        for (let i = 0; i < _model.length; i++) {
            if (Helpers.compareFloat(_model[i].value, value) === false)
                continue

            comboBox.currentIndex = i

            _value = value

            return;
        }

        comboBox.currentIndex = -1

        _value = value
    }

    function _updateValue(value) {
        _update = false

        _applySlider(value)

        _updateComboBox(value)

        _update = true
    }

    function _applySlider(value) {
        value = 17 * Math.log(value) / Math.log(2)

        if (value <= 1.0)
            slider.value = Math.max(slider.from, value)
        else
            slider.value = Math.min(value, slider.to)
    }

    function sliderToSpeed(value) {
        return Math.pow(2, value / 17)
    }

    function _applyPlayer(value) {
        if (_update === false)
            return

        value = sliderToSpeed(value)

        if (_shiftPressed === false) {
            for (let i = 0; i < _values.length; i++) {
                const clamp = _values[i]

                if (_testClamp(value, clamp)) {
                    value = clamp

                    break
                }
            }
        }

        _update = false

        Player.rate = value

        _updateComboBox(value)

        _update = true
    }

    function _testClamp(value, target) {
        return (Math.abs(value - target) < _threshold)
    }

    // Connections

    Connections {
        target: Player

        function onRateChanged() {
            _updateValue(Player.rate)
        }
    }

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Widgets.SubtitleLabel {
        id: label

        Layout.fillWidth: true

        Layout.alignment: Qt.AlignTop

        text: qsTr("Playback Speed")

        color: theme.fg.primary
    }

    Item {
        id: rowA

        Layout.fillWidth: true
        Layout.topMargin: VLCStyle.margin_xsmall

        Layout.alignment: Qt.AlignTop

        Navigation.parentItem: root
        Navigation.downItem: slider

        Widgets.CaptionLabel {
            anchors.verticalCenter: parent.verticalCenter

            text: qsTr("0.25")

            color: theme.fg.primary

            font.pixelSize: VLCStyle.fontSize_normal
        }

        Widgets.CaptionLabel {
            anchors.right: parent.right

            anchors.verticalCenter: parent.verticalCenter

            text: qsTr("4.00")

            color: theme.fg.primary

            font.pixelSize: VLCStyle.fontSize_normal
        }
    }

    Widgets.SliderExt {
        id: slider

        Layout.fillWidth: true
        Layout.topMargin: VLCStyle.margin_xsmall
        topPadding: 0

        // NOTE: These values come from the VLC 3.x implementation.
        from: -34
        to: 34

        stepSize: 1

        wheelEnabled: true

        toolTipTextProvider: function (value) {
            return sliderToSpeed(value).toFixed(2)
        }

        toolTipFollowsMouse: true

        Navigation.parentItem: root
        Navigation.downItem: comboBox

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

        onValueChanged: root._applyPlayer(value)

        MouseArea {
            anchors.fill: parent

            acceptedButtons: Qt.LeftButton

            onPressed: (mouse) => {
                mouse.accepted = false

                root._shiftPressed = (mouse.modifiers === Qt.ShiftModifier)
            }
        }

        Rectangle {
            id: tickmark

            parent: slider.background
            x: parent.width * .5
            width: VLCStyle.dp(1, VLCStyle.scale)
            height: parent.height
            visible: root.sliderToSpeed(slider.value) !== 1
            color: {
                const theme = slider.colorContext
                return root.sliderToSpeed(slider.value) > 1 ? theme.fg.negative : theme.fg.primary
            }
        }
    }

    RowLayout {
        id: rowB

        Layout.fillWidth: true
        Layout.topMargin: VLCStyle.margin_xsmall

        Navigation.parentItem: root
        Navigation.upItem: slider

        Widgets.ListLabel {
            text: qsTr("Presets")
            color: colorContext.fg.primary
            Layout.fillWidth: true
        }

        Widgets.ComboBoxExt {
            id: comboBox

            width: VLCStyle.combobox_width_normal
            height: VLCStyle.combobox_height_normal

            model: ListModel {}

            // NOTE: We display the 'Normal' string when the Slider is centered.
            displayText: (currentIndex === 3) ? currentText
                                              : root._value

            Navigation.parentItem: rowB

            // NOTE: This makes the navigation possible since 'up' is changing the comboBox value.
            Navigation.leftItem: slider

            Component.onCompleted: {
                for (let i = 0; i < _model.length; i++) {
                    const item = _model[i]

                    const title = item.title

                    if (title)
                        model.append({ "title": title })
                    else
                        model.append({ "title": String(item.value) })
                }
            }

            onCurrentIndexChanged: {
                if (root._update === false || currentIndex === -1)
                    return

                root._applySlider(_model[currentIndex].value)
            }
        }
    }
}
