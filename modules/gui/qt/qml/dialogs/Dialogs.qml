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
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
import org.videolan.vlc 0.1


import "qrc:///utils/" as Utils
import "qrc:///style/"

Item {
    id: root
    signal restoreFocus();
    property var bgContent: undefined

    Utils.DrawerExt {
        id: errorPopup
        anchors {
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }
        edge: Utils.DrawerExt.Edges.Bottom
        width: parent.width * 0.8
        z: 10

        property alias messageModel: messages
        ListModel {
            id: messages
        }

        component: Rectangle {
            color: "gray"
            opacity: 0.7
            width: errorPopup.width
            height: VLCStyle.fontHeight_normal * 5
            radius: VLCStyle.fontHeight_normal / 2

            Flickable {
                anchors.fill: parent
                anchors.margins: VLCStyle.fontHeight_normal / 2
                ScrollBar.vertical: ScrollBar{}
                contentY: VLCStyle.fontHeight_normal * ((messages.count * 2) - 4)
                clip: true

                ListView {
                    width: parent.width
                    height: VLCStyle.fontHeight_normal * messages.count * 2
                    model: messages
                    delegate: Column {
                        Text {
                            text: model.title
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.bold: true
                            color: "red"
                        }
                        Text {
                            text: model.text
                            font.pixelSize: VLCStyle.fontSize_normal
                        }
                    }
                }
            }
        }

        state: "hidden"
        onStateChanged: {
            hideErrorPopupTimer.restart()
        }

        Timer {
            id: hideErrorPopupTimer
            interval: 5000
            repeat: false
            onTriggered: {
                errorPopup.state = "hidden"
            }
        }
    }

    ModalDialog {
        id: loginDialog
        property var dialogId: undefined
        property string defaultUsername: ""

        onAboutToHide: restoreFocus()
        rootWindow: root.bgContent

        contentItem: GridLayout {
            columns: 2

            Text {
                text: qsTr("User")
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_normal
            }

            TextField {
                Layout.fillWidth:true
                id: username
                focus: true
                text: loginDialog.defaultUsername
                font.pixelSize: VLCStyle.fontSize_normal
                KeyNavigation.down: password
            }

            Text {
                text: qsTr("Password")
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_normal
            }

            TextField {
                Layout.fillWidth:true
                id: password
                echoMode: TextInput.Password
                font.pixelSize: VLCStyle.fontSize_normal
                KeyNavigation.down: savePassword
            }

            Text {
                text: qsTr("Save password")
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_normal
            }
            CheckBox {
                id: savePassword
                KeyNavigation.down: loginButtons
            }
        }

        footer: FocusScope {
            id: loginButtons
            implicitHeight: VLCStyle.icon_normal

            Rectangle {
                color: VLCStyle.colors.banner
                anchors.fill: parent

                RowLayout {
                    anchors.fill: parent

                    Utils.TextToolButton {
                        id: loginCancel
                        Layout.fillWidth: true
                        text: qsTr("cancel")
                        KeyNavigation.up: savePassword
                        KeyNavigation.right: loginOk
                        onClicked: {
                            dialogModel.dismiss(loginDialog.dialogId)
                            loginDialog.dialogId = undefined
                            loginDialog.close()
                        }
                    }

                    Utils.TextToolButton {
                        id: loginOk
                        Layout.fillWidth: true
                        text: qsTr("Ok")
                        focus: true
                        KeyNavigation.up: savePassword
                        onClicked: {
                            dialogModel.post_login(loginDialog.dialogId, username.text, password.text, savePassword.checked)
                            loginDialog.dialogId = undefined
                            loginDialog.close()
                        }
                    }
                }
            }
        }

        onAccepted: {
            dialogModel.post_login(dialogId, username.text, password.text, savePassword.checked)
        }
        onRejected: {
            dialogModel.dismiss(dialogId)
        }
    }

    ModalDialog {
        id: questionDialog

        property var dialogId: undefined
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
            color: VLCStyle.colors.text
            wrapMode: Text.WordWrap
        }

        footer: FocusScope {
            focus: true
            id: questionButtons
            implicitHeight: VLCStyle.icon_normal

            Rectangle {
                color: VLCStyle.colors.banner
                anchors.fill: parent

                RowLayout {
                    anchors.fill: parent

                    Utils.TextToolButton {
                        Layout.fillWidth: true
                        id: cancel
                        focus: true
                        visible: cancel.text !== ""
                        KeyNavigation.right: action1
                        onClicked: {
                            dialogModel.dismiss(questionDialog.dialogId)
                            questionDialog.dialogId = undefined
                            questionDialog.close()
                        }
                    }

                    Utils.TextToolButton {
                        id: action1
                        Layout.fillWidth: true
                        visible: action1.text !== ""
                        KeyNavigation.right: action2
                        onClicked: {
                            dialogModel.post_action1(questionDialog.dialogId)
                            questionDialog.dialogId = undefined
                            questionDialog.close()
                        }
                    }

                    Utils.TextToolButton {
                        id: action2
                        Layout.fillWidth: true
                        visible: action2.text !== ""
                        onClicked: {
                            dialogModel.post_action2(questionDialog.dialogId)
                            questionDialog.dialogId = undefined
                            questionDialog.close()
                        }
                    }
                }
            }
        }
    }

    DialogModel {
        id: dialogModel
        mainCtx: mainctx
        onLoginDisplayed: {
            loginDialog.dialogId = dialogId
            loginDialog.title = title
            loginDialog.defaultUsername = defaultUsername
            loginDialog.open()
        }

        onErrorDisplayed: {
            errorPopup.messageModel.append({title: title, text: text })
            errorPopup.state = "visible"
        }

        onProgressDisplayed: {
            console.warn("onProgressUpdated is not implemented")
        }

        onProgressUpdated: {
            console.warn("onProgressUpdated is not implemented")
        }

        onQuestionDisplayed: {
            questionDialog.dialogId = dialogId
            questionDialog.title = title
            questionDialog.text = text
            questionDialog.cancelTxt = cancel
            questionDialog.action1Txt = action1
            questionDialog.action2Txt = action2
            questionDialog.open()
        }

        onCancelled: {
            if (questionDialog.dialogId === dialogId) {
                questionDialog.close()
                dialogModel.dismiss(dialogId)
            } else if (loginDialog.dialogId === dialogId)  {
                loginDialog.close()
                dialogModel.dismiss(dialogId)
            } else {
                dialogModel.dismiss(dialogId)
            }
        }

        Component.onDestruction: {
            if (questionDialog.dialogId) {
                dialogModel.dismiss(questionDialog.dialogId)
            } if (loginDialog.dialogId) {
                dialogModel.dismiss(loginDialog.dialogId)
            }
        }
    }
}
