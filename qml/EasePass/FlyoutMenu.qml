import QtQuick
import QtQuick.Controls

// WinUI MenuFlyout replacement. Use MenuItem / Action children; openAt() positions the menu
// below an anchor item (like Flyout placement "Bottom").
Menu {
    id: root
    function openAt(anchorItem) {
        if (anchorItem) {
            const p = anchorItem.mapToItem(parent, 0, anchorItem.height)
            x = p.x
            y = p.y
        }
        open()
    }
}
