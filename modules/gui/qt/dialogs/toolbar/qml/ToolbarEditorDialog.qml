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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

import org.videolan.vlc 0.1

WindowDialog {
    id: root

    width: 800
    height: 600

    modal: true
    title: i18n.qtr("Toolbar Editor")

    signal unload()

    Component.onCompleted: {
        // Save first, in case the dialog is rejected.
        mainInterface.controlbarProfileModel.save(false)
    }

    onAccepted: {
        mainInterface.controlbarProfileModel.save()
        unload()
    }

    onRejected: {
        // Load saved to discard the changes
        mainInterface.controlbarProfileModel.reload()
        unload()
    }

    function _markDirty(text) {
        return (text += " *")
    }

    contentComponent: Item {
        ColumnLayout {
            anchors.fill: parent

            RowLayout {
                Widgets.MenuLabel {
                    Layout.fillWidth: true

                    text: i18n.qtr("Select profile:")
                }

                Widgets.ComboBoxExt {
                    id: comboBox
                    font.pixelSize: VLCStyle.fontSize_normal

                    width: VLCStyle.combobox_width_large
                    height: VLCStyle.combobox_height_normal

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
                        var text

                        if (!!mainInterface.controlbarProfileModel.currentModel)
                            text = mainInterface.controlbarProfileModel.currentModel.name
                        else {
                            text = "N/A"
                            return text
                        }

                        if (mainInterface.controlbarProfileModel.currentModel.dirty)
                            return _markDirty(text)
                        else
                            return text
                    }

                    model: mainInterface.controlbarProfileModel

                    currentIndex: mainInterface.controlbarProfileModel.selectedProfile

                    onCurrentIndexChanged: {
                        mainInterface.controlbarProfileModel.selectedProfile = currentIndex
                    }

                    Accessible.name: i18n.qtr("Profiles")
                }

                Widgets.IconToolButton {
                    text: i18n.qtr("New Profile")
                    iconText: VLCIcons.profile_new

                    onClicked: {
                        var npDialog = dialogProvider.getTextDialog(null,
                                                                    i18n.qtr("Profile Name"),
                                                                    i18n.qtr("Please enter the new profile name:"),
                                                                    i18n.qtr("Profile %1").arg(comboBox.count + 1))
                        if (!npDialog.ok)
                            return

                        mainInterface.controlbarProfileModel.cloneSelectedProfile(npDialog.text)
                        mainInterface.controlbarProfileModel.selectedProfile = (mainInterface.controlbarProfileModel.rowCount() - 1)
                    }

                    ToolTip.visible: hovered
                }

                Widgets.IconToolButton {
                    id: useDefaultButton

                    text: i18n.qtr("Use Default")
                    iconText: VLCIcons.history

                    onClicked: {
                        mainInterface.controlbarProfileModel.currentModel.injectDefaults(false)
                    }

                    ToolTip.visible: hovered
                }

                Widgets.IconToolButton {
                    text: i18n.qtr("Delete the current profile")
                    iconText: VLCIcons.del

                    onClicked: {
                          mainInterface.controlbarProfileModel.deleteSelectedProfile()
                    }

                    ToolTip.visible: hovered
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
