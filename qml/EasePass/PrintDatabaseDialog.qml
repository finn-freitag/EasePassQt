import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: ManageDatabasePage printer selection + Print_DatabaseItem_Click (Helper/PrinterHelper).
// The printer is chosen in the system print dialog; passwords are only printed after the
// plaintext warning was acknowledged by ticking "Include passwords".
ContentDialog {
    id: root
    required property var printer
    signal pdfRequested(bool includePasswords)

    title: qsTr("Print database")
    primaryButtonText: qsTr("Print")
    secondaryButtonText: qsTr("Export PDF")
    closeButtonText: qsTr("Cancel")
    primaryButtonEnabled: printer ? printer.canPrint : false
    secondaryButtonEnabled: primaryButtonEnabled

    onAboutToShow: includePasswords.checked = false
    onPrimaryClicked: printer.print(includePasswords.checked)
    onSecondaryClicked: pdfRequested(includePasswords.checked)

    CheckBox {
        id: includePasswords
        objectName: "includePasswords"
        text: qsTr("Include passwords")
    }
    InfoBar {
        objectName: "plaintextWarning"
        visible: includePasswords.checked
        open: true
        closable: false
        severity: Theme.Severity.Warning
        message: root.printer ? root.printer.plaintextWarning : ""
        Layout.fillWidth: true
    }
    Button {
        text: qsTr("Preview")
        enabled: root.primaryButtonEnabled
        onClicked: root.printer.preview(includePasswords.checked)
    }
}
