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
import QtQuick.Templates as T
import QtQuick.Layouts

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

import org.videolan.vlc 0.1

WindowDialog {
    id: root

    width: minimumWidth
    height: 600

    minimumWidth: 825
    minimumHeight: 400

    modal: true
    title: qsTr("Toolbar Editor")

    signal unload()

    Component.onCompleted: {
        // Save first, in case the dialog is rejected.
        MainCtx.controlbarProfileModel.save(false)
    }

    onAccepted: {
        MainCtx.controlbarProfileModel.save()
        unload()
    }

    onRejected: {
        // Load saved to discard the changes
        MainCtx.controlbarProfileModel.reload()
        unload()
    }

    function _markDirty(text) {
        return (text += " *")
    }

    contentComponent: Item {
        MouseArea {
            anchors.fill: parent

            cursorShape: toolbarEditor.dragActive ? Qt.ForbiddenCursor : Qt.ArrowCursor
        }

        ColumnLayout {
            anchors.fill: parent

            spacing: VLCStyle.margin_small

            RowLayout {
                Layout.fillHeight: false

                spacing: VLCStyle.margin_xsmall

                Widgets.MenuLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: implicitWidth

                    color: root.colorContext.fg.primary
                    text: qsTr("Select profile:")
                }

                Widgets.ComboBoxExt {
                    id: comboBox

                    Layout.fillWidth: (implicitWidth > Layout.minimumWidth)
                    Layout.minimumWidth: VLCStyle.combobox_width_large
                    Layout.minimumHeight: VLCStyle.combobox_height_normal

                    // this is not proper way to do it,
                    // but ComboBoxExt does not provide
                    // correct implicit width:
                    implicitWidth: implicitContentWidth
                    font.pixelSize: VLCStyle.fontSize_normal

                    delegate: ItemDelegate {
                        width: comboBox.width
                        leftPadding: comboBox.leftPadding
                        background: Item {}
                        contentItem: Text {
                            text: model.dirty ? _markDirty(model.name)
                                              : model.name
                            color: comboBox.color
                            font: comboBox.font
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        highlighted: (comboBox.highlightedIndex === index)
                    }

                    displayText: {
                        let text

                        if (!!MainCtx.controlbarProfileModel.currentModel)
                            text = MainCtx.controlbarProfileModel.currentModel.name
                        else {
                            text = "N/A"
                            return text
                        }

                        if (MainCtx.controlbarProfileModel.currentModel.dirty)
                            return _markDirty(text)
                        else
                            return text
                    }

                    model: MainCtx.controlbarProfileModel

                    currentIndex: MainCtx.controlbarProfileModel.selectedProfile

                    onCurrentIndexChanged: {
                        MainCtx.controlbarProfileModel.selectedProfile = currentIndex
                    }

                    Accessible.name: qsTr("Profiles")
                }

                Widgets.IconToolButton {
                    description: qsTr("New Profile")
                    text: VLCIcons.profile_new

                    onClicked: {
                        const npDialog = DialogsProvider.getTextDialog(null,
                                                                     qsTr("Profile Name"),
                                                                     qsTr("Please enter the new profile name:"),
                                                                     qsTr("Profile %1").arg(comboBox.count + 1))
                        if (!npDialog.ok)
                            return

                        MainCtx.controlbarProfileModel.cloneSelectedProfile(npDialog.text)
                        MainCtx.controlbarProfileModel.selectedProfile = (MainCtx.controlbarProfileModel.rowCount() - 1)
                    }
                }

                Widgets.IconToolButton {
                    id: useDefaultButton

                    description: qsTr("Use Default")
                    text: VLCIcons.history

                    onClicked: {
                        MainCtx.controlbarProfileModel.currentModel.injectDefaults(false)
                    }
                }

                Widgets.IconToolButton {
                    description: qsTr("Delete the current profile")
                    text: VLCIcons.del

                    onClicked: {
                          MainCtx.controlbarProfileModel.deleteSelectedProfile()
                    }
                }
            }

            // The main context of the toolbareditor dialog:
            ToolbarEditor {
                id: toolbarEditor

                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
