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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtQuick.Templates as T
import org.videolan.vlc 0.1


import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property Item bgContent: null

    //---------------------------------------------------------------------------------------------
    // Signal
    //---------------------------------------------------------------------------------------------

    signal restoreFocus();

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    Component.onCompleted: if (DialogErrorModel.repeatedMessageCount){
                               hideErrorPopupTimer.restart()
                               errorPopup.state = "visible"
                           }

    Component.onDestruction: {
        if (questionDialog.dialogId !== null) {
            dialogModel.dismiss(questionDialog.dialogId)
            questionDialog.dialogId = null
        } if (loginDialog.dialogId !== null) {
            dialogModel.dismiss(loginDialog.dialogId)
            loginDialog.dialogId = null
        }
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function ask(text, acceptCb, rejectCb, buttons) {
        customDialog.ask(text, acceptCb, rejectCb, buttons)
    }

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections
    {
        target: dialogModel

        function onLogin(dialogId, title, text, defaultUsername, askStore) {
            loginDialog.dialogId = dialogId
            loginDialog.title = title
            loginDialog.defaultUsername = defaultUsername
            loginDialog.open()
        }

        function onQuestion(dialogId, title, text, type, cancel, action1, action2) {
            questionDialog.dialogId = dialogId
            questionDialog.title = title
            questionDialog.text = text
            questionDialog.cancelTxt = cancel
            questionDialog.action1Txt = action1
            questionDialog.action2Txt = action2
            questionDialog.open()
        }

        function onProgress(dialogId, title, text, indeterminate, position, cancel) {
            progressDialog.dialogId = dialogId
            progressDialog.title = title
            progressDialog.text = text
            progressDialog.cancelTxt = cancel
            progressDialog.position = position
            progressDialog.interderminate = indeterminate
            progressDialog.open()
        }

        function onProgressUpdated(dialogId, position, text) {
            if (progressDialog.dialogId !== dialogId) {
                console.warn("progress event on an inexisting dialog")
                return
            }
            progressDialog.text = text
            progressDialog.position = position
        }

        function onCancelled(dialogId) {
            if (questionDialog.dialogId === dialogId) {
                questionDialog.close()
                questionDialog.dialogId = null
                dialogModel.dismiss(dialogId)
            } else if (loginDialog.dialogId === dialogId)  {
                loginDialog.close()
                loginDialog.dialogId = null
                dialogModel.dismiss(dialogId)
            } else if (progressDialog.dialogId === dialogId) {
                progressDialog.close()
                progressDialog.dialogId = null
                dialogModel.dismiss(dialogId)
            } else {
                dialogModel.dismiss(dialogId)
            }
        }
    }

    Connections
    {
        target: DialogErrorModel

        function onCountChanged() {
            hideErrorPopupTimer.restart()
            errorPopup.state = "visible"
        }
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    DialogModel {
        id: dialogModel
        ctx: MainCtx
    }


    Widgets.DrawerExt {
        id: errorPopup

        property string messageText
        property int repeatedMessageCount: 0

        anchors {
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
            bottomMargin: VLCStyle.margin_small
        }

        edge: Widgets.DrawerExt.Edges.Bottom
        width: contentItem.layoutWidth
        height: contentItem.height
        z: 10

        ColorContext {
            id: errorMsgTheme
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        component: Rectangle {
            color: errorMsgTheme.bg.negative

            height: messageText.implicitHeight + VLCStyle.margin_normal
            radius: VLCStyle.fontHeight_normal / 2

            property real layoutWidth: layout.width

            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTr("error popup")

            RowLayout {
                id: layout

                spacing: VLCStyle.margin_xsmall
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                Widgets.IconLabel {
                    text: VLCIcons.info
                    color: errorMsgTheme.fg.negative
                    Layout.leftMargin: VLCStyle.margin_xxsmall
                }

                T.Label {
                    id: messageText

                    Layout.maximumWidth: root.width * 0.5
                    Layout.leftMargin: VLCStyle.margin_xxsmall
                    Layout.rightMargin: VLCStyle.margin_xxsmall

                    text: (DialogErrorModel.repeatedMessageCount > 1 ? '[' + DialogErrorModel.repeatedMessageCount + '] ' : '')
                          + DialogErrorModel.notificationText

                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.bold: true
                    color: errorMsgTheme.fg.negative
                }

                Widgets.TextToolButton {
                    id: detailsBtn

                    text: qsTr("Show Details")

                    colorContext.colorSet: ColorContext.ButtonAccent

                    onClicked: {
                        hideErrorPopupTimer.stop()
                        errorPopup.state = "hidden"
                        DialogErrorModel.resetRepeatedMessageCount()
                        DialogsProvider.messagesDialog(1)
                    }
                }

                Widgets.IconToolButton {
                    id: closeBtn
                    font.pixelSize: VLCStyle.icon_normal
                    text: VLCIcons.clear
                    description: qsTr("Dismiss")
                    Layout.rightMargin: VLCStyle.margin_xxsmall

                    color: closeBtn.colorContext.fg.negative
                    backgroundColor: closeBtn.colorContext.bg.negative

                    onClicked: {
                        hideErrorPopupTimer.stop()
                        errorPopup.state = "hidden"
                        DialogErrorModel.resetRepeatedMessageCount()
                    }
                }
            }
        }

        state: "hidden"

        Timer {
            id: hideErrorPopupTimer
            interval: 5000
            repeat: false
            onTriggered: {
                errorPopup.state = "hidden"
                DialogErrorModel.resetRepeatedMessageCount()
            }
        }
    }

    ModalDialog {
        id: loginDialog

        //use var here as DialogId is a QGadget and passed by value
        property var dialogId: null
        property string defaultUsername: ""

        onAboutToHide: restoreFocus()
        rootWindow: root.bgContent

        contentItem: GridLayout {
            columns: 2

            readonly property ColorContext colorContext: ColorContext {
                id: loginContentTheme
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            Text {
                text: qsTr("User")
                color: loginContentTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.TextFieldExt {
                id: username

                focus: true
                text: loginDialog.defaultUsername

                Layout.fillWidth:true

                Navigation.downItem: password
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
            }

            Text {
                text: qsTr("Password")
                color: loginContentTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.TextFieldExt {
                id: password

                echoMode: TextInput.Password
                font.pixelSize: VLCStyle.fontSize_normal
                Layout.fillWidth:true

                Navigation.upItem: username
                Navigation.downItem: savePassword
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
            }

            Text {
                text: qsTr("Save password")
                color: loginContentTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }
            CheckBox {
                id: savePassword

                Navigation.upItem: password
                Navigation.downItem: loginButtons
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
            }
        }

        footer: FocusScope {
            id: loginButtons
            implicitHeight: VLCStyle.icon_normal

            readonly property ColorContext colorContext: ColorContext {
                id: loginFooterTheme
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            Rectangle {
                color: loginFooterTheme.bg.primary
                anchors.fill: parent

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: VLCStyle.margin_xxsmall
                    anchors.rightMargin: VLCStyle.margin_xxsmall

                    Widgets.TextToolButton {
                        id: loginCancel
                        Layout.fillWidth: true
                        text: qsTr("cancel")

                        Navigation.upItem: savePassword
                        Navigation.rightItem: loginOk
                        Keys.priority: Keys.AfterItem
                        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                        onClicked: {
                            loginDialog.reject()
                            loginDialog.close()
                        }
                    }

                    Widgets.TextToolButton {
                        id: loginOk
                        Layout.fillWidth: true
                        text: qsTr("Ok")
                        focus: true

                        Navigation.upItem: savePassword
                        Navigation.leftItem: loginCancel
                        Keys.priority: Keys.AfterItem
                        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                        onClicked: {
                            loginDialog.accept()
                            loginDialog.close()
                        }
                    }
                }
            }
        }

        onAccepted: {
            if (loginDialog.dialogId !== null) {
                dialogModel.post_login(loginDialog.dialogId, username.text, password.text, savePassword.checked)
                loginDialog.dialogId = null
            }
        }
        onRejected: {
            if (loginDialog.dialogId !== null) {
                dialogModel.dismiss(loginDialog.dialogId)
                loginDialog.dialogId = null
            }
        }
    }

    ModalDialog {
        id: progressDialog

        property var dialogId: null

        property string text : ""
        property string cancelTxt: ""
        property bool interderminate: false
        property real position: 0.0

        onAboutToHide: restoreFocus()
        rootWindow: root.bgContent

        contentItem:  ColumnLayout {

            readonly property ColorContext colorContext: ColorContext {
                id: progressContentTheme
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            Text {
                focus: false
                font.pixelSize: VLCStyle.fontSize_normal
                color: progressContentTheme.fg.primary
                text: progressDialog.text
                wrapMode: Text.WordWrap
            }

            ProgressBar {
                Layout.fillWidth:true

                from: 0.0
                to: 1.0

                indeterminate: progressDialog.interderminate
                value: progressDialog.position
            }
        }


        footer: FocusScope {
            focus: true
            implicitHeight: VLCStyle.icon_normal

            readonly property ColorContext colorContext: ColorContext {
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            Rectangle {
                color: questionDialog.colorContext.bg.primary
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.margin_xxsmall
                anchors.rightMargin: VLCStyle.margin_xxsmall

                Widgets.TextToolButton {
                    anchors.right: parent.right
                    focus: true
                    visible: text !== ""
                    text: progressDialog.cancelTxt

                    onClicked: {
                        dialogModel.dismiss(progressDialog.dialogId)
                        progressDialog.dialogId = null
                        progressDialog.close()
                    }
                }
            }
        }
    }

    ModalDialog {
        id: questionDialog

        //use var here as DialogId is a QGadget and passed by value
        property var dialogId: null
        property alias text: content.text
        property alias cancelTxt: cancel.text
        property alias action1Txt: action1.text
        property alias action2Txt: action2.text

        rootWindow: root.bgContent

        onAboutToHide: restoreFocus()

        contentItem: Text {
            id: content
            focus: false
            font.pixelSize: VLCStyle.fontSize_normal
            color: questionDialog.colorContext.fg.primary
            wrapMode: Text.WordWrap
        }

        footer: FocusScope {
            focus: true
            id: questionButtons
            implicitHeight: VLCStyle.icon_normal

            readonly property ColorContext colorContext: ColorContext {
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            Rectangle {
                color: questionDialog.colorContext.bg.primary
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.margin_xxsmall
                anchors.rightMargin: VLCStyle.margin_xxsmall

                RowLayout {
                    anchors.fill: parent

                    Widgets.TextToolButton {
                        id: cancel

                        focus: true
                        visible: cancel.text !== ""

                        Layout.fillWidth: true

                        Navigation.rightItem: action1
                        Keys.priority: Keys.AfterItem
                        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                        onClicked: {
                            dialogModel.dismiss(questionDialog.dialogId)
                            questionDialog.dialogId = null
                            questionDialog.close()
                        }
                    }

                    Widgets.TextToolButton {
                        id: action1

                        visible: action1.text !== ""

                        Layout.fillWidth: true

                        Navigation.leftItem: cancel
                        Navigation.rightItem: action2
                        Keys.priority: Keys.AfterItem
                        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                        onClicked: {
                            dialogModel.post_action1(questionDialog.dialogId)
                            questionDialog.dialogId = null
                            questionDialog.close()
                        }
                    }

                    Widgets.TextToolButton {
                        id: action2
                        visible: action2.text !== ""

                        Layout.fillWidth: true

                        Navigation.leftItem: action1
                        Keys.priority: Keys.AfterItem
                        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                        onClicked: {
                            dialogModel.post_action2(questionDialog.dialogId)
                            questionDialog.dialogId = null
                            questionDialog.close()
                        }
                    }
                }
            }
        }
    }

    CustomDialog {
        id: customDialog
        rootWindow: root.bgContent
        onAboutToHide: restoreFocus()
    }

    Loader {
        id: toolbarEditorDialogLoader
        active: false
        source: "qrc:///dialogs/ToolbarEditorDialog.qml"

        Connections {
            target: toolbarEditorDialogLoader.item

            function onUnload() {
                toolbarEditorDialogLoader.active = false
            }
        }

        Connections {
            target: DialogsProvider

            function onShowToolbarEditorDialog() {
                toolbarEditorDialogLoader.active = true
                toolbarEditorDialogLoader.item.open()
            }
        }
    }
}
