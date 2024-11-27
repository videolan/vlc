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

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs


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

    onRejected: (byButton) => {
        // Load saved to discard the changes
        MainCtx.controlbarProfileModel.reload()
        unload()
    }

    function newProfile(profileCreatorFunction) {
        console.assert(typeof profileCreatorFunction === 'function')

        const count = MainCtx.controlbarProfileModel.rowCount()
        const npDialog = DialogsProvider.getTextDialog(null,
                                                       qsTr("Profile Name"),
                                                       qsTr("Please enter the new profile name:"),
                                                       qsTr("Profile %1").arg(count + 1))
        if (!npDialog.ok)
            return

        profileCreatorFunction(npDialog.text)
        MainCtx.controlbarProfileModel.selectedProfile = count
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
                    description: qsTr("Clone the selected profile")
                    text: VLCIcons.ic_fluent_document_copy_24_regular

                    onClicked: {
                        root.newProfile(MainCtx.controlbarProfileModel.cloneSelectedProfile)
                    }
                }

                Widgets.IconToolButton {
                    description: qsTr("New profile")
                    text: VLCIcons.ic_fluent_document_add_24_regular

                    onClicked: {
                        root.newProfile(MainCtx.controlbarProfileModel.newProfile)
                    }
                }

                Widgets.IconToolButton {
                    description: qsTr("Delete the current profile.\nDeleting all profiles brings back the default styles.")
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
