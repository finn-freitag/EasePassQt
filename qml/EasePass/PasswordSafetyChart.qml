import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// C#: Controls/PasswordSafetyChart.xaml(.cs) - 8-segment circle, one segment per check:
//  0 lower case, 1 upper case, 2 length, 3 not leaked, 4 punctuation, 5 digits,
//  6 low predictability, 7 not seen before.
// checks[i] is true (green), false (red) or null/undefined (grey, unknown). The evaluation
// itself is done in C++ (Core::PasswordStrength, LeakCheckService) by the view model.
Item {
    id: root
    property var checks: [null, null, null, null, null, null, null, null]
    property bool showInfo: true
    readonly property int hoveredSegment: hover.hovered ? segmentAt(hover.point.position) : -1

    readonly property var messages: [
        qsTr("Lower case letters"), qsTr("Upper case letters"), qsTr("Password length"),
        qsTr("Leaked or exploited"), qsTr("Punctuation"), qsTr("Digits"),
        qsTr("Predictability"), qsTr("Seen before")
    ]
    readonly property var _infos: [
        qsTr("Contains lower case:"), qsTr("Contains upper case:"), qsTr("Password length:"),
        qsTr("Leaked or exploited:"), qsTr("Contains punctuation:"), qsTr("Contains digits:"),
        qsTr("Predictability:"), qsTr("Seen before:")
    ]
    // [positive, negative] per check (C# suffixes)
    readonly property var _suffixes: [
        [qsTr("True"), qsTr("False")], [qsTr("True"), qsTr("False")],
        [qsTr("Meets minimum"), qsTr("Doesn't meet minimum")], [qsTr("False"), qsTr("True")],
        [qsTr("True"), qsTr("False")], [qsTr("True"), qsTr("False")],
        [qsTr("Low"), qsTr("High")], [qsTr("False"), qsTr("True")]
    ]
    // C#: PasswordSafetyChart.ToString()
    readonly property string infoText: {
        const lines = []
        for (let i = 0; i < 8; ++i) {
            const c = checks ? checks[i] : undefined
            const s = c === true ? _suffixes[i][0] : c === false ? _suffixes[i][1] : qsTr("Unknown")
            lines.push(_infos[i] + " " + s)
        }
        return lines.join("\n")
    }

    function segmentColor(i) {
        const c = checks ? checks[i] : undefined
        return c === true ? Theme.chartGood : c === false ? Theme.chartBad : Theme.chartUnknown
    }
    // Maps a point to the check index of the segment under it (clockwise from 12 o'clock).
    function segmentAt(p) {
        const dx = p.x - width / 2, dy = p.y - height / 2
        let a = Math.atan2(dx, -dy) * 180 / Math.PI
        if (a < 0) a += 360
        const order = [0, 1, 2, 3, 7, 6, 5, 4]
        return order[Math.min(7, Math.floor(a / 45))]
    }

    implicitWidth: 100
    implicitHeight: 100
    Accessible.role: Accessible.Chart
    Accessible.name: qsTr("Password safety")
    Accessible.description: infoText

    // Path data copied from the C# XAML (10x10 coordinate space).
    readonly property var _paths: [
        "M 5.1251355,0 V 4.6972437 L 8.4469209,1.3754593 C 7.5776978,0.54862633 6.4118825,0.03168344 5.1251355,0 Z",
        "M 8.6247999,1.5533377 5.3030145,4.8751226 H 10 C 9.9683501,3.5883582 9.4516218,2.4225824 8.6247999,1.5533377 Z",
        "M 5.3030145,5.1251355 8.6247999,8.4469209 C 9.4515943,7.5777063 9.9683161,6.4118474 10,5.1251355 Z",
        "M 5.1251355,5.3030145 V 10 C 6.4118474,9.9683151 7.5777058,9.4515943 8.4469209,8.6247999 Z",
        "M 4.8751226,0 C 3.5883234,0.03164992 2.4225901,0.54859781 1.5533377,1.3754593 l 3.3217849,3.3217844 z",
        "M 1.3754589,1.5533377 C 0.54859771,2.4225904 0.03165073,3.5883237 0,4.8751226 h 4.6972437 z",
        "m 0,5.1251355 c 0.03168316,1.286747 0.54862613,2.4525628 1.3754589,3.3217854 L 4.6972437,5.1251355 Z",
        "M 4.8751226,5.3030145 1.5533377,8.6247999 C 2.4225822,9.4516223 3.5883582,9.9683491 4.8751226,10 Z"
    ]

    Shape {
        id: shape
        width: 10
        height: 10
        preferredRendererType: Shape.CurveRenderer
        transform: Scale { xScale: root.width / 10; yScale: root.height / 10 }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(0); PathSvg { path: root._paths[0] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(1); PathSvg { path: root._paths[1] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(2); PathSvg { path: root._paths[2] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(3); PathSvg { path: root._paths[3] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(4); PathSvg { path: root._paths[4] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(5); PathSvg { path: root._paths[5] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(6); PathSvg { path: root._paths[6] } }
        ShapePath { strokeWidth: -1; fillColor: root.segmentColor(7); PathSvg { path: root._paths[7] } }
    }

    HoverHandler { id: hover }
    ToolTip.visible: showInfo && hover.hovered
    ToolTip.delay: 300
    ToolTip.text: hoveredSegment >= 0 ? messages[hoveredSegment] + "\n\n" + infoText : infoText
}
