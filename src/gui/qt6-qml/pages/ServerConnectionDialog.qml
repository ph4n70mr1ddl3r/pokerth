import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import "../config" as Config


Window {
    id: serverConnectDialogWindow
    width: 200
    height: 150
    visible: true
    title: qsTr("Server Connection")

    Rectangle {
        color: Config.StaticData.palette.secondary.col700
        anchors.fill: parent

        StackLayout {
            id: mainStack
            anchors.fill: parent
            currentIndex: 0 // Start with the initial choices view

            // View 0: Initial choices
            ColumnLayout {
                id: initialChoicesView
                spacing: 15
                Layout.fillHeight: true
                Layout.fillWidth: true

                Button {
                    text: qsTr("Login as User")
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 16
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    onClicked: {
                        serverConnectDialogWindow.width = 500
                        serverConnectDialogWindow.height = 280
                        mainStack.currentIndex = 1 // Switch to login form view
                    }
                }

                Button {
                    text: qsTr("Register")
                    Layout.alignment: Qt.AlignHCenter
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 16
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    onClicked: {
                        Qt.openUrlExternally("https://www.pokerth.net/ucp.php?mode=register")
                    }
                }

                Button {
                    text: qsTr("Continue as Guest")
                    Layout.alignment: Qt.AlignHCenter
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 16
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    onClicked: {
                        serverConnectDialogWindow.width = 300
                        serverConnectDialogWindow.height = 135

                        usernameLabel.text = "Guest" + Math.floor(Math.random() * 10000)
                        mainStack.currentIndex = 2 // Switch to connecting
                    }
                }
            }

            // View 1: Login as User form
            ColumnLayout {
                id: loginFormView
                spacing: 15
                Layout.fillHeight: true
                width: parent.width * 0.8

                Label {
                    text: qsTr("User Login")
                    Layout.alignment: Qt.AlignHCenter
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 20
                    color: Config.StaticData.palette.secondary.col200
                }

                TextField {
                    id: usernameInput
                    placeholderText: qsTr("Username")
                    Layout.fillWidth: true
                    font.family: Config.StaticData.loadedFont.font.family
                    color: Config.StaticData.palette.secondary.col200
                    background: Rectangle {
                        color: Qt.darker(Config.StaticData.palette.secondary.col700, 1.5)
                        radius: 3
                    }
                    placeholderTextColor: Qt.lighter(Config.StaticData.palette.secondary.col200, 1.5)
                }

                TextField {
                    id: passwordInput
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password
                    Layout.fillWidth: true
                    font.family: Config.StaticData.loadedFont.font.family
                    color: Config.StaticData.palette.secondary.col200
                     background: Rectangle {
                        color: Qt.darker(Config.StaticData.palette.secondary.col700, 1.5)
                        radius: 3
                    }
                    placeholderTextColor: Qt.lighter(Config.StaticData.palette.secondary.col200, 1.5)
                }

                CheckBox {
                    id: rememberMeCheckbox
                    text: qsTr("Remember me")
                    Layout.alignment: Qt.AlignLeft
                    font.family: Config.StaticData.loadedFont.font.family
                    contentItem: Text {
                        text: rememberMeCheckbox.text
                        font: rememberMeCheckbox.font
                        color: Config.StaticData.palette.secondary.col200
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: rememberMeCheckbox.indicator.width + rememberMeCheckbox.spacing
                    }
                }

                Button {
                    text: qsTr("Login")
                    Layout.alignment: Qt.AlignHCenter
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 16
                    onClicked: {
                        console.log("Login clicked. Username:", usernameInput.text, "Password:", passwordInput.text, "Remember me:", rememberMeCheckbox.checked)
                        usernameLabel.text = usernameInput.text

                        serverConnectDialogWindow.width = 300
                        serverConnectDialogWindow.height = 135
                        mainStack.currentIndex = 2 // Go to login section
                        // Login Logic
                    }
                }

                Button {
                    text: qsTr("Back")
                    Layout.alignment: Qt.AlignHCenter
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 14
                    onClicked: {
                        serverConnectDialogWindow.width = 200
                        serverConnectDialogWindow.height = 150
                        mainStack.currentIndex = 0 // Go back to initial choices
                    }
                }
            }

            // View 2: Connecting
            ColumnLayout {
                id: guestLoginView
                Layout.minimumWidth: 0
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 15

                Text {
                    text: qsTr("Connecting as...")
                    font.pixelSize: 14
                    Layout.fillWidth: false
                    font.family: Config.StaticData.loadedFont.font.family
                    color: Config.StaticData.palette.secondary.col200
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    id: usernameLabel
                    text: qsTr("Username/Guest")
                    font.pixelSize: 20
                    Layout.fillWidth: false
                    font.family: Config.StaticData.loadedFont.font.family
                    color: Config.StaticData.palette.secondary.col200
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: qsTr("STATUS INDICATOR ??%")
                    font.pixelSize: 16
                    Layout.fillWidth: false
                    font.family: Config.StaticData.loadedFont.font.family
                    color: Config.StaticData.palette.secondary.col200
                    Layout.alignment: Qt.AlignHCenter
                }
                 Button {
                    text: qsTr("Cancel")
                    Layout.alignment: Qt.AlignHCenter
                    font.family: Config.StaticData.loadedFont.font.family
                    font.pixelSize: 14
                    Layout.fillWidth: true
                    onClicked: {
                        serverConnectDialogWindow.width = 200
                        serverConnectDialogWindow.height = 150

                        mainStack.currentIndex = 0 // Go back to initial choices
                    }
                }
            }
        }
    }
}