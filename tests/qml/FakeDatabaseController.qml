import QtQuick

// Test double for App::DatabaseController (same properties, signals and invokables the pages
// use). open() resolves asynchronously like the real Argon2 unlock.
QtObject {
    id: fake
    property var knownDatabases: ["/home/u/Personal.epdb", "/home/u/Work.epdb"]
    property string lastDatabase: "/home/u/Work.epdb"
    property bool busy: false
    property bool isOpen: false
    property bool needsSecondFactor: false
    property string errorMessage: ""
    property string correctPassword: "secret"
    property string correctSecondFactor: ""   // non-empty -> database needs it
    property int openCalls: 0
    property var lastOpen: []
    property var lastCreate: []
    property var lastImport: []
    property bool createAndOpenUsed: false

    signal openFailed(string message)
    signal created(string path)
    signal imported(string path)
    signal oneTimeTokenIssued(string token)

    function displayNameForPath(p) {
        const f = p.substring(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1)
        const dot = f.lastIndexOf(".")
        return dot > 0 ? f.substring(0, dot) : f
    }
    function toLocalPath(p) { return p.startsWith("file://") ? p.substring(7) : p }
    function defaultDatabasePath() { return "/home/u/.local/share/EasePass/easepass.epdb" }
    function buildDatabasePath(folder, name) { return toLocalPath(folder) + "/" + name + ".epdb" }
    function clearError() { errorMessage = "" }

    function open(path, password, secondFactor) {
        if (busy) return false
        openCalls++
        lastOpen = [path, password, secondFactor]
        busy = true
        errorMessage = ""
        resolve.start()
        return true
    }
    property Timer resolve: Timer {
        interval: 30
        onTriggered: {
            fake.busy = false
            if (fake.lastOpen[1] !== fake.correctPassword) {
                fake.errorMessage = "Wrong password"
                fake.openFailed(fake.errorMessage)
            } else if (fake.correctSecondFactor.length > 0 && fake.lastOpen[2] !== fake.correctSecondFactor) {
                fake.needsSecondFactor = true
                fake.errorMessage = "Second factor required"
                fake.openFailed(fake.errorMessage)
            } else {
                fake.needsSecondFactor = false
                fake.isOpen = true
            }
        }
    }

    function createDatabase(path, pw, repeat) {
        lastCreate = [path, pw, repeat]
        if (pw.length < 4) { errorMessage = "Password too short"; return }
        if (pw !== repeat) { errorMessage = "Passwords do not match"; return }
        knownDatabases = knownDatabases.concat([path])
        lastDatabase = path
        created(path)
    }
    function createAndOpen(path, pw, repeat) {
        createAndOpenUsed = true
        createDatabase(path, pw, repeat)
        if (errorMessage.length === 0) isOpen = true
    }
    function importDatabase(path, pw, second) {
        lastImport = [path, pw, second]
        if (pw !== correctPassword) { errorMessage = "Wrong password"; return }
        knownDatabases = knownDatabases.concat([path])
        lastDatabase = path
        imported(path)
    }
}
