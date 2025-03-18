/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Controls as T
import QtQml.Models

import VLC.Style
import VLC.Widgets as Widgets

import VLC.MainInterface
import VLC.Dialogs

T.Pane {
    id: root

    signal itemClicked(uri : var)

    //safe area margins for delegate content,
    //so the background of the delegates fills the available space
    //but the content remains within the safe area
    property int safeAreaLeftMargin: 0
    property int safeAreaRightMargin: 0

    property bool useAcrylic: false

    readonly property int minimumWidth: VLCStyle.expandNavigationPaneWidth + safeAreaLeftMargin + safeAreaRightMargin

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_normal

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Accessible.name: qsTr("Sidebar")

    NavigationModel {
        id: navigationModel
        hasMedialib: MainCtx.mediaLibraryAvailable
    }

    background: Widgets.AcrylicBackground {
        enabled: root.useAcrylic
        tintColor: theme.bg.primary
    }

    contentItem:  Item {
        id: contentItemId

        //don't use Layout as top item as items put as children of the Control
        //would be parented (and layouted) to the Layout
        ColumnLayout {
            anchors.fill: parent
            spacing: VLCStyle.margin_xsmall

            Widgets.ListViewExt {
                id: listView

                Layout.fillWidth: true
                Layout.fillHeight: true

                clip: !fadingEdge.implicitClipping && (height < contentHeight)
                focus: true

                model: navigationModel

                colorContext.colorSet: ColorContext.Window

                fadingEdge.backgroundColor:  (root.background && (root.background.color.a >= 1.0)) ? root.background.color
                                                                                                   : "transparent"

                Navigation.parentItem: root
                Navigation.downItem: preferenceButton

                delegate: SideNavigationDelegate {

                    width: ListView.view.contentWidth
                    height: VLCStyle.buttonHeightNavigationPane

                    leftPadding: root.safeAreaLeftMargin + VLCStyle.margin_xsmall
                    rightPadding: root.safeAreaRightMargin

                    iconTxt: model.icon
                    text: model.title

                    checked: !model.expanded && !!model.uri && History.match(History.viewPath, model.uri)

                    onClicked: {
                        itemClicked(model.uri)
                        listView.currentIndex = index
                        listView.forceActiveFocus(focusReason)
                        model.expanded = !model.expanded
                    }
                }
            }

            Loader {
                id: loaderProgress

                Layout.fillWidth: true

                active: (MainCtx.mediaLibraryAvailable && MainCtx.mediaLibrary.idle === false)
                visible: active

                source: "qrc:///qt/qml/VLC/MediaLibrary/ScanProgressBar.qml"

                onLoaded: {
                    item.leftPadding = Qt.binding(function() { return root.safeAreaLeftMargin + VLCStyle.margin_small })
                    item.rightPadding = Qt.binding(function() { return root.safeAreaRightMargin + VLCStyle.margin_small })
                }
            }

            SideNavigationDelegate {
                id: preferenceButton

                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.buttonHeightNavigationPane

                leftPadding: root.safeAreaLeftMargin + VLCStyle.margin_xsmall
                rightPadding: root.safeAreaRightMargin

                iconTxt: VLCIcons.settings
                text: qsTr("Preferences")

                checked: activeFocus || DialogsProvider.prefsDialogVisible

                onClicked: DialogsProvider.prefsDialog()

                //SideNavigationDelegate is an ItemDelegate and has NoFocus by default
                focusPolicy: Qt.StrongFocus

                Navigation.parentItem: root
                Navigation.upItem: listView

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (e) => Navigation.defaultKeyAction(e)
                Keys.onReleased: (e) => Navigation.defaultKeyReleaseAction(e)
            }

        }
    }
}
