import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config
import "../components"

Rectangle {
    id: startPage
    width: mainWindow.width
    height: mainWindow.height
    color: "transparent"

    property var internetDialog: null

    function dialogClosing(closeEvent) {
        internetDialog.closing.disconnect(dialogClosing)
        mainWindow.show()
        internetDialog.destroy()
        internetDialog = null;
    }

    Image {
        id: preLoaderBackground
        anchors.fill: parent
        source: "../resources/startWindowBackground.png"
        fillMode: Image.PreserveAspectCrop
    }

    ColumnLayout {
        id: startPageRows
        anchors.fill: parent

        ColumnLayout {
            id: startPageContentLayout
            Layout.alignment: Qt.AlignCenter
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: startPageMainButtonsBox
                Layout.preferredWidth: 287
                Layout.preferredHeight: 300
                color: "transparent"

                Rectangle {
                    anchors.fill: parent
                    color: Config.StaticData.palette.secondary.col700
                    opacity: 0.8
                    radius: 5
                }

                ColumnLayout {
                    id: startPageMainButtons
                    anchors.centerIn: parent
                    spacing: 18

                    CustomButton {
                        text: qsTr("Internetspiel")
                        onClicked: {
                            var component = Qt.createComponent("ServerConnectionDialog.qml")
                            internetDialog = component.createObject(mainWindow)

                            internetDialog.closing.connect(dialogClosing)

                            internetDialog.show()
                            mainWindow.hide()
                        }
                    }

                    CustomButton {
                        text: qsTr("Lokales Spiel starten")
                        onClicked: {
                            mainStackView.push("LocalGamePage.qml");
                        }
                    }

                    CustomButton {
                        text: qsTr("Netzwerkspiel erstellen")
                        onClicked: {
                            mainStackView.push("NetworkGameCreatePage.qml");
                        }
                    }

                    CustomButton {
                        text: qsTr("Netzwerkspiel beitreten")
                        onClicked: {
                            mainStackView.push("NetworkGameEnterPage.qml");
                        }
                    }

                    CustomButton {
                        text: qsTr("Logs")
                        onClicked: {
                            mainStackView.push("LogsPage.qml");
                        }
                    }
                }
            }
        }
    }
}
