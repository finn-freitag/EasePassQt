import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/GenPasswordDialog + Views/DialogPages/GenPasswordPage. Generates with the stored
// generator settings (length, characters, leak check); "New" draws another one.
ContentDialog {
    id: root
    required property var generator       // App::PasswordGeneratorViewModel
    signal copyRequested(string text)

    title: qsTr("Password generator")
    primaryButtonText: qsTr("New")
    primaryButtonEnabled: generator ? !generator.busy : false
    closeButtonText: qsTr("Done")
    closeOnPrimary: false
    onPrimaryClicked: generator.generate()
    onAboutToShow: {
        passwordBox.text = ""
        generator.generate()
    }
    onClosed: generator.password = "" // do not keep the password around

    Connections {
        target: root.generator
        function onPasswordChanged() {
            if (passwordBox.text !== root.generator.password)
                passwordBox.text = root.generator.password
        }
    }

    RowLayout {
        spacing: Theme.spacing
        Layout.fillWidth: true
        EditPasswordBox {
            id: passwordBox
            objectName: "generatedPassword"
            placeholderText: qsTr("Password")
            revealed: true
            Layout.fillWidth: true
            onTextEdited: root.generator.password = text
            onCopyRequested: (t) => root.copyRequested(t)
        }
        IconButton {
            objectName: "copyGeneratedButton"
            iconName: "copy"
            toolTip: qsTr("Copy")
            enabled: passwordBox.text.length > 0
            onClicked: root.copyRequested(passwordBox.text)
        }
    }
    ProgressBar {
        objectName: "generatorProgress"
        indeterminate: true
        visible: root.generator ? root.generator.busy : false
        Layout.fillWidth: true
    }
    PasswordSafetyChart {
        objectName: "generatorSafetyChart"
        checks: root.generator ? root.generator.safetyChecks : []
        Layout.alignment: Qt.AlignHCenter
    }
}
