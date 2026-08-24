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
import QtQuick.Templates as T
import QtQml.Models

import VLC.Style
import VLC.Widgets as Widgets

import VLC.MainInterface
import VLC.Dialogs
import VLC.Util

T.Pane {
    id: root

    signal itemClicked(uri : var, item : Item)

    //safe area margins for delegate content,
    //so the background of the delegates fills the available space
    //but the content remains within the safe area
    property int safeAreaLeftMargin: 0
    property int safeAreaRightMargin: 0

    property bool useAcrylic: true

    property bool delegateShowText: true

    property alias model: navigationModel

    property alias view: listView

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

    property alias loaderUpdateButton: loaderUpdateButton

    Accessible.name: qsTr("Sidebar")

    NavigationModel {
        id: navigationModel
        hasMedialib: MainCtx.mediaLibraryAvailable
    }

    Connections {
        target: History

        // NOTE: handles cases where the path changes not by clicking in the sidebar
        function onViewPathChanged(viewPath) {
            for (let i = 0; i < listView.count; ++i) {
                const rowIndex = navigationModel.index(i, 0)
                const uri = navigationModel.data(rowIndex, NavigationModel.URI)

                if (uri && History.match(viewPath, uri)) {
                    // handle back button
                    listView.currentIndex = i

                    // handle keyboard navigation
                    if (!navigationModel.data(rowIndex, NavigationModel.EXPANDED))
                        navigationModel.setData(rowIndex, true, NavigationModel.EXPANDED)

                    return
                }
            }
        }
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

                Binding {
                    target: listView.T.ScrollBar.vertical
                    property: "policy"
                    when: listView.T.ScrollBar.vertical?.size > (1.0 - Number.EPSILON)
                    value: T.ScrollBar.AlwaysOff
                }

                colorContext.colorSet: ColorContext.Window

                fadingEdge.backgroundColor:  (root.background && root.background.visible && (root.background.color.a >= 1.0 || fadingEdge.uniformBackdrop)) ? root.background.color
                                                                                                                                                            : "transparent"

                component DelegateAnimation : NumberAnimation {
                    easing.type: Easing.InOutSine
                    duration: VLCStyle.duration_long
                }

                reuseItems: false // Not relevant here, we disable it because of QTBUG-131106 when transition is involved.

                // As the docs note, `ListView` transitions do not seem to handle height animation. For that reason,
                // we animate the height directly in the delegate (Qt's recommended workaround). Because of this, we
                // do not need to have `addDisplaced` transition here, but we can have `removeDisplaced` here:
                removeDisplaced: Transition {
                    DelegateAnimation { property: "y" }
                }

                onCurrentIndexChanged: {
                    const rowIndex = navigationModel.index(currentIndex, 0)

                    if (rowIndex.valid && !navigationModel.data(rowIndex, NavigationModel.EXPANDED)
                            && History.match(History.viewPath, navigationModel.data(rowIndex, NavigationModel.URI)))
                        navigationModel.setData(rowIndex, true, NavigationModel.EXPANDED)
                }

                property bool readyForAnimations: false

                onModelChanged: {
                    if (!listView.readyForAnimations)
                        Qt.callLater(() => { listView.readyForAnimations = true })
                }

                Navigation.parentItem: root
                Navigation.downItem: loaderUpdateButton

                delegate: SideNavigationDelegate {
                    id: delegate

                    width: ListView.view.contentWidth
                    height: preferredHeight

                    property real preferredHeight: shouldShow ? VLCStyle.buttonHeightNavigationPane : 0.0

                    leftPadding: root.safeAreaLeftMargin + VLCStyle.margin_xsmall
                    rightPadding: root.safeAreaRightMargin

                    leftInset: root.safeAreaLeftMargin + implicitLeftInset
                    rightInset: root.safeAreaRightMargin + implicitRightInset

                    iconTxt: icon_alias
                    text: title

                    showText: root.delegateShowText

                    required property string title
                    required property var uri
                    required property int depth
                    // TODO: Qt 6.2 has property `icon` marked final, so we can't override it:
                    required property string icon_alias
                    required property bool expandable
                    required property bool expanded

                    required property int index

                    visible: (height > 0.0)

                    property bool shouldShow: showText || (iconTxt.length > 0)

                    onShouldShowChanged: {
                        if (listView.readyForAnimations)
                            heightBehavior.enabled = true // At this point, it can keep being enabled for good.
                    }

                    Binding on highlighted {
                        when: !!uri && History.match(History.viewPath, uri)
                        value: true
                    }
                    checked: (!showText || !expanded) && highlighted

                    onClicked: {
                        itemClicked(uri, this)
                        listView.currentIndex = index
                        listView.forceActiveFocus(focusReason)
                    }

                    Behavior on height {
                        id: heightBehavior

                        enabled: false

                        DelegateAnimation {
                            onRunningChanged: {
                                if (running) {
                                    delegate.clip = true
                                } else {
                                    delegate.clip = false
                                }
                            }
                        }
                    }

                    ListView.onAdd: {
                        if (listView.readyForAnimations && delegate.showText) {
                            heightBehavior.enabled = false
                            delegate.height = 0.0
                            heightBehavior.enabled = true
                            delegate.height = Qt.binding(() => delegate.preferredHeight)
                            heightBehavior.enabled = false
                        }
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
                    if (item.background) {
                        item.background.opacity = 0.6
                        item.background.radius = Qt.binding(() => VLCStyle.dp(4, VLCStyle.scale))
                    }

                    item.leftPadding = Qt.binding(function() { return root.safeAreaLeftMargin + VLCStyle.margin_small })
                    item.rightPadding = Qt.binding(function() { return root.safeAreaRightMargin + VLCStyle.margin_small })

                    item.leftInset = Qt.binding(() => { return VLCStyle.margin_xxsmall + root.safeAreaLeftMargin })
                    item.rightInset = Qt.binding(() => { return VLCStyle.margin_xxsmall + root.safeAreaRightMargin })
                }
            }

            Loader {
                id: loaderUpdateButton

                // WARNING: Object name is used from C++ side to acknowledge the modern update pane is loaded,
                //          if C++ side does not get this acknowledgement, the old update dialog is shown as
                //          fallback.
                objectName: "updateButtonLoader"

                visible: active

                Layout.fillWidth: true

                readonly property bool updateModelIsAvailable: (typeof UpdateModel !== 'undefined')

                opacity: 0.0

                Behavior on opacity {
                    OpacityAnimator {
                        duration: VLCStyle.duration_short
                    }
                }

                active: updateModelIsAvailable && (shouldShow || opacity > 0.0)

                Binding {
                    target: MainCtx.navigationPanel
                    property: "badge"
                    when: loaderUpdateButton.shouldShow &&
                          !loaderUpdateButton.visible &&
                          !loaderUpdateButton.suppressed
                    value: {
                       if (!loaderUpdateButton.updateModelIsAvailable)
                           return SidePanelCtx.BadgeNone

                        switch (UpdateModel.updateStatus) {
                        case UpdateModel.NeedUpdate:
                            return SidePanelCtx.BadgePositive
                        case UpdateModel.CheckFailed:
                            return SidePanelCtx.BadgeNegative
                        default:
                            return SidePanelCtx.BadgeNeutral
                        }
                    }
                }

                readonly property bool shouldShow: {
                    if (!updateModelIsAvailable)
                        return false

                    if (DialogsProvider.updateDialogVisible)
                        return false
                        
                    if (UpdateModel.explicitCheck) {
                        return false // This button is only meant for non-explicit update checks (unlike update pane).
                    } else {
                        if (UpdateModel.updateStatus === UpdateModel.updateStatusOnAcknowledgment)
                            return false // already dismissed with the same status

                        switch (UpdateModel.updateStatus) {
                            case UpdateModel.NeedUpdate:
                            case UpdateModel.CheckFailed:
                                return true
                            default:
                                return false
                        }
                    }
                }

                onShouldShowChanged: {
                    if (shouldShow) {
                        opacity = 1.0
                        suppressed = false
                    }
                }

                property bool suppressed: false

                function dismiss(suppress /* : bool */ = false) {
                    // This could be also be an assertion:
                    if (updateModelIsAvailable) {
                        if (!suppress)
                            UpdateModel.updateStatusOnAcknowledgment = UpdateModel.updateStatus
                    }

                    if (suppress)
                        suppressed = true

                    opacity = 0.0
                }

                Navigation.parentItem: root
                Navigation.upItem: listView
                Navigation.downItem: preferenceButton

                sourceComponent: Widgets.ButtonExt {
                    id: updateButton

                    focus: true

                    radius: VLCStyle.button_radius

                    verticalPadding: VLCStyle.margin_xxsmall

                    leftPadding: root.safeAreaLeftMargin + VLCStyle.margin_xsmall
                    rightPadding: root.safeAreaRightMargin + VLCStyle.margin_xsmall

                    leftInset: root.safeAreaLeftMargin + VLCStyle.margin_xxsmall
                    rightInset: root.safeAreaRightMargin + VLCStyle.margin_xxsmall

                    topInset: VLCStyle.margin_xxxsmall
                    bottomInset: VLCStyle.margin_xxxsmall

                    topPadding: VLCStyle.margin_xxsmall
                    bottomPadding: VLCStyle.margin_xxsmall

                    spacing: VLCStyle.margin_xsmall

                    showText: root.delegateShowText

                    iconSize: VLCStyle.icon_banner

                    font.pixelSize: VLCStyle.fontSize_normal

                    text: {
                        switch (UpdateModel.updateStatus) {
                        case UpdateModel.NeedUpdate:
                            return qsTr("Update found!")
                        case UpdateModel.CheckFailed:
                            return qsTr("Update error!")
                        default:
                            return qsTr("N/A")
                        }
                    }

                    Navigation.parentItem: loaderUpdateButton

                    Keys.onCancelPressed: {
                        loaderUpdateButton.dismiss()
                    }

                    Keys.onEscapePressed: {
                        loaderUpdateButton.dismiss()
                    }

                    onClicked: {
                        loaderUpdateButton.dismiss(true)
                    }

                    background: Widgets.AnimatedBackground {
                        enabled: updateButton.colorContext.initialized

                        color: {
                            switch (UpdateModel.updateStatus) {
                            case UpdateModel.NeedUpdate:
                                return updateButton.colorContext.bg.positive
                            case UpdateModel.CheckFailed:
                                return updateButton.colorContext.bg.negative
                            default:
                                return updateButton.colorContext.bg.neutral
                            }
                        }

                        border.color: updateButton.visualFocus ? updateButton.colorFocus
                                                               : (updateButton.colorContext.border.a > 0.0 ? updateButton.colorContext.border
                                                                                                           : color)

                        radius: updateButton.radius

                        opacity: 0.8
                    }

                    contentItem: RowLayout {
                        spacing: updateButton.spacing

                        property alias label: contentText

                        Widgets.IconLabel {
                            Layout.fillHeight: true

                            font.pixelSize: updateButton.iconSize

                            text: VLCIcons.ic_fluent_arrow_sync_24_regular

                            color: updateButton.colorContext.fg.primary
                        }

                        Widgets.LabelExt {
                            id: contentText

                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            font: updateButton.font

                            visible: updateButton.showText

                            elide: Text.ElideRight

                            verticalAlignment: Text.AlignVCenter

                            text: updateButton.text

                            color: updateButton.colorContext.fg.primary

                            Accessible.ignored: true
                        }

                        Widgets.IconToolButton {
                            id: closeButton

                            Layout.fillHeight: true
                            Layout.maximumHeight: width

                            implicitHeight: contentText.implicitHeight

                            description: qsTr("Dismiss the update button")
                            text: VLCIcons.close

                            visible: contentText.visible

                            // We handle escape key press in `updateButton`:
                            activeFocusOnTab: false

                            onClicked: {
                                loaderUpdateButton.dismiss()
                            }

                            // This blocks propagating hover to the root button (updateButton):
                            HoverHandler {
                                id: hoverHandler

                                Component.onCompleted: {
                                    // TODO: Directly set with Qt 6.3
                                    if (hoverHandler.blocking !== undefined)
                                        hoverHandler.blocking = true
                                }
                            }
                        }
                    }
                }
            }

            SideNavigationDelegate {
                id: preferenceButton

                AccessibleCompat.id: "preferencesButton"

                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.buttonHeightNavigationPane

                leftPadding: root.safeAreaLeftMargin + VLCStyle.margin_xsmall
                rightPadding: root.safeAreaRightMargin

                leftInset: root.safeAreaLeftMargin + implicitLeftInset
                rightInset: root.safeAreaRightMargin + implicitRightInset

                iconTxt: VLCIcons.settings
                text: qsTr("Preferences")

                checked: DialogsProvider.prefsDialogVisible

                showText: root.delegateShowText

                onClicked: DialogsProvider.prefsDialog()

                //SideNavigationDelegate is an ItemDelegate and has NoFocus by default
                focusPolicy: Qt.StrongFocus

                Navigation.parentItem: root
                Navigation.upItem: loaderUpdateButton

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (e) => Navigation.defaultKeyAction(e)
                Keys.onReleased: (e) => Navigation.defaultKeyReleaseAction(e)
            }

        }
    }
}
