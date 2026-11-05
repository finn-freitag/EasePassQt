import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 2FA parameters of an item (C#: Add2FAPage / AddItemPage 2FA expander): secret, digits,
// interval and algorithm. Writes every edit into the ItemViewModel buffer; call sync() after
// the buffer was changed from outside (load, QR scan).
ColumnLayout {
    id: root
    required property var editor          // App::ItemViewModel
    readonly property var algorithms: ["SHA1", "SHA256", "SHA512"]
    signal copyRequested(string text)

    function sync() {
        if (!editor) return
        secretBox.text = editor.secret
        digitsBox.text = editor.digits
        intervalBox.text = editor.interval
        const i = algorithms.indexOf(editor.algorithm.trim().toUpperCase())
        algorithmBox.currentIndex = i >= 0 ? i : 0
    }
    function focusSecret() { secretBox.forceActiveFocus() }

    spacing: Theme.spacingS

    Label { text: qsTr("Secret"); color: Theme.textPrimary }
    EditPasswordBox {
        id: secretBox
        objectName: "secretBox"
        placeholderText: qsTr("Secret")
        Layout.fillWidth: true
        onTextEdited: root.editor.secret = text
        onCopyRequested: (t) => root.copyRequested(t)
    }
    Label { text: qsTr("Digits"); color: Theme.textPrimary }
    TextField {
        id: digitsBox
        objectName: "digitsBox"
        Accessible.name: qsTr("Digits")
        validator: RegularExpressionValidator { regularExpression: /[0-9]{0,2}/ } // C#: digits only
        inputMethodHints: Qt.ImhDigitsOnly
        Layout.fillWidth: true
        onTextEdited: root.editor.digits = text
    }
    Label { text: qsTr("Interval (sec)"); color: Theme.textPrimary }
    TextField {
        id: intervalBox
        objectName: "intervalBox"
        Accessible.name: qsTr("Interval (sec)")
        validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
        inputMethodHints: Qt.ImhDigitsOnly
        Layout.fillWidth: true
        onTextEdited: root.editor.interval = text
    }
    Label { text: qsTr("Algorithm"); color: Theme.textPrimary }
    ComboBox {
        id: algorithmBox
        objectName: "algorithmBox"
        model: root.algorithms
        Accessible.name: qsTr("Algorithm")
        Layout.fillWidth: true
        onActivated: (index) => root.editor.algorithm = root.algorithms[index]
    }
}
