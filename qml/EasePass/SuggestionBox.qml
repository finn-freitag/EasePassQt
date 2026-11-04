import QtQuick
import QtQuick.Controls

// Shared AutoSuggestBox replacement used by SearchPasswordsBox and TextWithSuggestionBox.
// suggestions: array of strings or objects with a `text` property.
// userTextChanged(isUserChange, text): isUserChange is false when the text was set
// programmatically or by choosing a suggestion (C#: _isUserTextChange logic).
TextField {
    id: root
    property var suggestions: []
    property int highlightedIndex: -1
    property string accessibleName: placeholderText
    property bool _programmatic: false
    readonly property bool suggestionsOpen: popup.opened
    signal userTextChanged(bool isUserChange, string text)
    signal suggestionChosen(var item, int index)
    signal keyPressed(var event)

    function setTextProgrammatically(t) {
        _programmatic = true
        text = t
        _programmatic = false
    }
    function suggestionText(item) {
        return (item !== null && typeof item === "object") ? String(item.text) : String(item)
    }
    function chooseSuggestion(index) {
        if (index < 0 || !suggestions || index >= suggestions.length)
            return
        const item = suggestions[index]
        popup.close()
        setTextProgrammatically(suggestionText(item))
        suggestionChosen(item, index)
    }
    function openSuggestions() {
        if (suggestions && suggestions.length > 0 && activeFocus)
            popup.open()
    }

    selectByMouse: true
    Accessible.name: accessibleName
    Accessible.role: Accessible.EditableText

    onTextChanged: {
        userTextChanged(!_programmatic, text)
        highlightedIndex = -1
        if (!_programmatic) openSuggestions()
    }
    onSuggestionsChanged: {
        highlightedIndex = -1
        if (suggestions && suggestions.length > 0) openSuggestions()
        else popup.close()
    }
    onActiveFocusChanged: if (!activeFocus) popup.close()

    Keys.onPressed: (event) => {
        root.keyPressed(event)
        if (event.accepted)
            return
        const n = suggestions ? suggestions.length : 0
        if (event.key === Qt.Key_Down && n > 0) {
            if (!popup.opened) popup.open()
            highlightedIndex = Math.min(n - 1, highlightedIndex + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up && n > 0 && popup.opened) {
            highlightedIndex = Math.max(0, highlightedIndex - 1)
            event.accepted = true
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                   && popup.opened && highlightedIndex >= 0) {
            chooseSuggestion(highlightedIndex)
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && popup.opened) {
            popup.close()
            event.accepted = true
        }
    }

    Popup {
        id: popup
        objectName: "suggestionPopup"
        y: root.height + Theme.spacingXS
        width: root.width
        padding: Theme.spacingXS
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        implicitHeight: Math.min(list.contentHeight, 300) + 2 * padding

        background: Rectangle {
            color: Theme.card
            border.color: Theme.cardBorder
            radius: Theme.radius
        }

        contentItem: ListView {
            id: list
            objectName: "suggestionList"
            clip: true
            model: root.suggestions ? root.suggestions : []
            currentIndex: root.highlightedIndex
            Accessible.role: Accessible.List
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: ListView.view.width
                text: root.suggestionText(modelData)
                highlighted: index === root.highlightedIndex
                focusPolicy: Qt.NoFocus
                onClicked: root.chooseSuggestion(index)
            }
        }
    }
}
