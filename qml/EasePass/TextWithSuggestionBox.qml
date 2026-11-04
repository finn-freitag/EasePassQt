import QtQuick

// C#: Controls/TextWithSuggestionBox.xaml.cs - text field that suggests from a fixed list
// (suggestionItems) while the user types; matching is a case-insensitive "contains" and the
// exact current text is not suggested again.
SuggestionBox {
    id: root
    property var suggestionItems: []
    property int maxSuggestions: 10
    property bool _hasTyped: false

    function filterSuggestions(t) {
        if (!suggestionItems || t.length === 0)
            return []
        const q = t.toLowerCase()
        const out = []
        for (let i = 0; i < suggestionItems.length && out.length < maxSuggestions; ++i) {
            const s = String(suggestionItems[i])
            const l = s.toLowerCase()
            if (l !== q && l.indexOf(q) >= 0)
                out.push(s)
        }
        return out
    }

    onUserTextChanged: (isUserChange, t) => {
        if (isUserChange) suggestions = filterSuggestions(t)
        else suggestions = []
    }
}
