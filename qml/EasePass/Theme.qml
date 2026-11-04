pragma Singleton
import QtQuick

/*
 * Central design tokens for the QML UI (steps.txt T11). Components must take colours,
 * sizes and fonts from here instead of hard-coding them.
 *
 * Light/dark follows the system colour scheme (Qt.styleHints.colorScheme) unless
 * themeMode forces one (C# setting "Theme": system/light/dark). The colours mirror the
 * WinUI 3 default palette used by the C# app.
 */
QtObject {
    id: theme

    // --- Mode -------------------------------------------------------------------------
    enum Mode { System, Light, Dark }
    property int themeMode: Theme.Mode.System
    readonly property bool systemDark: Qt.styleHints.colorScheme === Qt.ColorScheme.Dark
    readonly property bool dark: themeMode === Theme.Mode.Dark
                                 || (themeMode === Theme.Mode.System && systemDark)

    // --- Severity (InfoBar etc.) -----------------------------------------------------
    enum Severity { Info, Success, Warning, Error }

    // --- Accent --------------------------------------------------------------------
    readonly property SystemPalette _sys: SystemPalette { colorGroup: SystemPalette.Active }
    // Accent from the system palette when the platform provides one, else WinUI blue.
    property color accent: _sys.accent.valid && _sys.accent.hslSaturation > 0.05
                           ? _sys.accent : (dark ? "#60CDFF" : "#005FB8")
    readonly property color accentText: dark ? "#000000" : "#FFFFFF"

    // --- Surfaces --------------------------------------------------------------------
    readonly property color background: dark ? "#202020" : "#F3F3F3"
    readonly property color card: dark ? "#2B2B2B" : "#FBFBFB"
    readonly property color cardHover: dark ? "#323232" : "#F6F6F6"
    readonly property color cardPressed: dark ? "#272727" : "#F0F0F0"
    readonly property color cardBorder: dark ? "#1D1D1D" : "#E5E5E5"
    readonly property color divider: dark ? "#3A3A3A" : "#E0E0E0"
    readonly property color overlay: dark ? "#99000000" : "#4D000000"

    // --- Text ------------------------------------------------------------------------
    readonly property color textPrimary: dark ? "#FFFFFF" : "#1B1B1B"
    readonly property color textSecondary: dark ? "#C5C5C5" : "#616161"
    readonly property color textDisabled: dark ? "#787878" : "#A0A0A0"

    // --- Status ----------------------------------------------------------------------
    readonly property color infoBackground: dark ? "#272727" : "#F6F6F6"
    readonly property color successBackground: dark ? "#393D1B" : "#DFF6DD"
    readonly property color warningBackground: dark ? "#433519" : "#FFF4CE"
    readonly property color errorBackground: dark ? "#442726" : "#FDE7E9"
    readonly property color infoForeground: accent
    readonly property color successForeground: dark ? "#6CCB5F" : "#0F7B0F"
    readonly property color warningForeground: dark ? "#FCE100" : "#9D5D00"
    readonly property color errorForeground: dark ? "#FF99A4" : "#C42B1C"

    function severityBackground(s) {
        switch (s) {
        case Theme.Severity.Success: return successBackground
        case Theme.Severity.Warning: return warningBackground
        case Theme.Severity.Error: return errorBackground
        default: return infoBackground
        }
    }
    function severityForeground(s) {
        switch (s) {
        case Theme.Severity.Success: return successForeground
        case Theme.Severity.Warning: return warningForeground
        case Theme.Severity.Error: return errorForeground
        default: return infoForeground
        }
    }
    function severityIcon(s) {
        switch (s) {
        case Theme.Severity.Success: return icon("success")
        case Theme.Severity.Warning: return icon("warning")
        case Theme.Severity.Error: return icon("error")
        default: return icon("info")
        }
    }

    // PasswordSafetyChart segment colours (C#: Controls/PasswordSafetyChart.xaml.cs SetChartEntry)
    readonly property color chartUnknown: "#646464"
    readonly property color chartGood: "#00FF00"
    readonly property color chartBad: "#FF0000"

    // --- Geometry --------------------------------------------------------------------
    readonly property int radiusSmall: 4
    readonly property int radius: 8
    readonly property int spacingXS: 2
    readonly property int spacingS: 4
    readonly property int spacing: 8
    readonly property int spacingM: 12
    readonly property int spacingL: 16
    readonly property int spacingXL: 24
    readonly property int controlHeight: 32
    readonly property int iconSize: 16
    readonly property int iconSizeLarge: 20
    readonly property int settingsCardHeight: 64

    // --- Typography (WinUI type ramp) -------------------------------------------------
    readonly property string fontFamily: Qt.application.font.family
    readonly property int fontCaption: 12
    readonly property int fontBody: 14
    readonly property int fontSubtitle: 20
    readonly property int fontTitle: 28
    readonly property int fontTitleLarge: 40

    // --- Animation -------------------------------------------------------------------
    readonly property int animationFast: 83
    readonly property int animationNormal: 167

    // --- Icons -----------------------------------------------------------------------
    // Monochrome SVGs in icons/, tinted by the consumer (icon.color / Icon.color).
    function icon(name) {
        return Qt.resolvedUrl("icons/" + name + ".svg")
    }
}
