# QML module "EasePass" (steps.txt T11): Theme singleton, control library, icons.
# Built as a static library with a static plugin; consumers link easepass_qml_module and
# easepass_qml_moduleplugin and call Q_IMPORT_QML_PLUGIN(EasePassPlugin).

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

file(GLOB EASEPASS_QML_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/*.svg)

qt_add_library(easepass_qml_module STATIC)
qt_add_qml_module(easepass_qml_module
    URI EasePass
    VERSION 1.0
    PLUGIN_TARGET easepass_qml_moduleplugin
    QML_FILES
        Theme.qml
        Icon.qml
        IconButton.qml
        SettingsControl.qml
        SettingsItemSeparator.qml
        LabeledTextBox.qml
        CopyTextbox.qml
        CopyPasswordbox.qml
        CopyTokenBox.qml
        EditPasswordBox.qml
        TagRenderControl.qml
        SuggestionBox.qml
        SearchPasswordsBox.qml
        TextWithSuggestionBox.qml
        PasswordSafetyChart.qml
        InfoBar.qml
        ContentDialog.qml
        FlyoutMenu.qml
        CommandBar.qml
        Splitter.qml
        Gallery.qml
        DailyTipBox.qml
        ImportDatabaseFlow.qml
        CreateDatabaseDialog.qml
        OneTimeTokenDialog.qml
        LoginPage.qml
        RegisterPage.qml
        PlaceholderPage.qml
        PasswordRow.qml
        PasswordsPage.qml
        TwoFactorFields.qml
        ItemEditDialog.qml
        Add2FADialog.qml
        WebcamScanDialog.qml
        PasswordGeneratorDialog.qml
        ChangePasswordDialog.qml
        ManageSecondFactorDialog.qml
        DeleteDatabaseDialog.qml
        RenameDatabaseDialog.qml
        SelectItemsDialog.qml
        PasswordPromptDialog.qml
        PrintDatabaseDialog.qml
        DatabasePage.qml
        SettingsPage.qml
        Expander.qml
        AboutPage.qml
        AutoLogoutDialog.qml
        Main.qml
    RESOURCES
        ${EASEPASS_QML_ICONS}
)
target_link_libraries(easepass_qml_module PRIVATE
    Qt6::Quick Qt6::QuickControls2 easepass_warnings)
