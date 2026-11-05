# Security notes

This document describes how Ease Pass Qt handles secrets on the local machine. It records
the results of the security review (steps.txt T23, 2026-11-05) and the limits that remain.
The file format and cryptography (Argon2id + AES-256-CBC, epdb v1.4) are unchanged from the
C# app and are described in the code (`src/core/CryptoHelper`, `src/core/DatabaseFile`).

Report vulnerabilities privately to the maintainers. Please do not open a public issue.

## Threat model

Protected:
- **A stolen or copied database file.** The file is only as strong as the master password,
  plus the second factor if one is set. Argon2id makes guessing passwords expensive.
- **Other local users on a shared machine.** Database, backup, export and log files are
  owner-only, and so is the single-instance socket.
- **Leftovers after locking.** Locking, either by hand or after inactivity, drops the
  decrypted items, the master password and the second factor, and the generated password.
  It also clears a copied secret from the clipboard.
- **Accidental disclosure.** Secrets never go into logs. Clipboard managers and clipboard
  history are asked not to store copied secrets. Favicons and the leak check stay off unless
  the user turns them on.

Not protected:
- **Malware running as the same user, or as root, while the database is unlocked.** It can
  read process memory (unless ptrace is restricted, see below), log keystrokes and take
  screenshots.
- **A compromised operating system, a compromised Qt or OpenSSL, or physical attacks on RAM**
  (cold boot, DMA).

## Secrets in memory

| Secret | Held in | Wiped |
|---|---|---|
| Master password, second factor | `SecretString` / `SecureBuffer` (locked with mlock or VirtualLock, cleared with `OPENSSL_cleanse`) | on lock, close and destruction (`DatabaseFile::wipeSecrets`) |
| Argon2 / AES keys | `SecureBuffer` | at the end of each operation |
| Decrypted item JSON while loading | `SecureBuffer` (since T23) | on every return path |
| Item JSON while saving | `QByteArray` | `OPENSSL_cleanse` right after encryption |
| Items (passwords, TOTP secrets, notes) | `QString` inside `PasswordItem` | released on lock; see limitations |
| Typed text in QML fields | QML `TextField` (`QString`, JS heap) | fields are emptied after login, on dialog close and on lock |

Limitations:
- **Item fields are plain `QString`s.** Qt, QML and the JS engine copy strings freely, for
  example in bindings, models, the undo history of text fields and the accessibility tree.
  Those copies cannot be wiped reliably. After a lock they are freed, but not overwritten.
  The same holds for the `QJsonDocument` that is built while loading.
- **Locking memory is best effort.** `mlock` is limited by `RLIMIT_MEMLOCK`, usually 8 MiB
  on current Linux and sometimes only 64 KiB. Very large databases may not fit, and failures
  are ignored. Argon2's 250 MB working memory is not locked; libargon2 clears it after use.
- **Swap and hibernation.** Memory that is not locked can reach swap, and a hibernation image
  contains all of RAM. Use encrypted swap and full-disk encryption. Neither can be turned off
  per process.

## Crash dumps and debugging

- **Linux:** `PR_SET_DUMPABLE=0`. This means no core file (also not through systemd-coredump
  or a `core_pattern` pipe), and other processes of the same user cannot `ptrace` the app or
  read its memory. Since T23 the core size limit is also set to 0 (`RLIMIT_CORE`), on every
  Unix system.
- **Windows:** Windows Error Reporting writes a dump only if the user or an administrator
  turned on `LocalDumps`. The app does not change that machine setting. TODO for the Windows
  port: decide whether to call `WerAddExcludedApplication` per user.
- **Both:** debugging a running instance still needs root, or a debugger the user starts
  explicitly.

## Clipboard

Copying a password or TOTP code goes through `ClipboardManager`. It remembers only a SHA-256
of the text. After `clipboardClearSeconds` (default 15 s, 0 = never) it clears the clipboard,
but only if the clipboard still holds our text. Something the user copied in the meantime is
left alone. The clipboard is also cleared:

- right away when the database is locked (since T23), and
- when the app quits.

| Platform | Hint for history and managers | Notes |
|---|---|---|
| KDE Plasma (X11 and Wayland) | `x-kde-passwordManagerHint: secret`, respected by Klipper | |
| Other X11 / Wayland | none (no standard exists) | Clipboard managers such as CopyQ may keep the text. Exclude Ease Pass in their settings. |
| Windows | `ExcludeClipboardContentFromMonitorProcessing`, `CanIncludeInClipboardHistory=0`, `CanUploadToCloudClipboard=0` | Not yet tested on Windows. |
| macOS | TODO: `org.nspasteboard.ConcealedType` | macOS is not a target yet. |

Wayland: a client may only change the selection while it has keyboard focus. If the window is
in the background when the timer runs out, the compositor may ignore the clear, and the text
stays until something else is copied. KeePassXC has the same limit. (verify per compositor;
not covered by automated tests)

X11: our process owns the clipboard. When it quits, the text disappears unless a clipboard
manager has taken a copy. Selecting a revealed password with the mouse also puts it into the
PRIMARY selection. Qt does not do that for hidden (password-mode) fields.

## Files on disk

| File | Location | Permissions |
|---|---|---|
| Database (`*.epdb`) | chosen by the user | new files 0600 (since T23); existing files keep theirs |
| Backup (`*.epdb.bak`) | next to the database | copied from the database |
| Atomic-save temp file | next to the database (`QSaveFile`) | same as the database; renamed on commit, removed on failure |
| Database export | chosen by the user | new files 0600 |
| PDF export (may contain plaintext passwords) | chosen by the user | new files 0600; the dialog warns before including passwords |
| Log files | `<AppData>/logs` | folder 0700, files 0600, created with these permissions (since T23) |
| Settings | `QSettings` (`~/.config/EasePass/…`) | user default; no secrets (paths, window state, options) |
| Favicon cache | `~/.cache/EasePass/icons` | user default; see below |
| Screenshot for QR scanning (Wayland portal) | portal temp file | deleted right after loading |

On Windows the Unix permission bits have no effect. There the ACLs of the user profile
protect the files.

## Logs

- **No secrets are logged on purpose.** As a second line of defence, every message passes
  `FileLogger::redact()`, which masks `otpauth://` URIs and `password=` / `secret=` /
  `token=` style values.
- **What logs can contain:** database file names and paths, and errors.
- **Retention:** at most 10 files and 28 days, each file at most 1 MiB.
- **Test:** `test_app.testFileLoggerRedaction` runs a real session (login, copy, failed open,
  lock) with the logger installed. It asserts that neither the master password, an item
  password nor a TOTP secret reaches the file.

## Network and privacy

The app makes no network requests by default.

- **Favicons** (`fetchFavicons`, default off). When on, the app requests
  `https://<domain>/favicon.ico` for every website in the database, so each site learns that
  you use it. If that request fails, the app asks Google's favicon service
  (`www.google.com/s2/favicons?domain=…`), which tells Google about these domains too.
  Downloaded icons are cached unencrypted under file names that are an MD5 of the domain, so
  someone with access to your home folder can tell which sites are in the database.
  Possible follow-ups: drop the Google fallback, encrypt the cache or keep it in memory only.
- **Leak check** (`verifyNotLeaked`). Uses the Have I Been Pwned range API with
  k-anonymity: only the first 5 hex characters of the SHA-1 are sent, with response padding.
  The password never leaves the machine.
- **Plugins (T25+).** Network access is a permission that is granted per plugin and limited
  by the host to a list of allowed hosts (steps.txt D11).

## Single instance

The lock file and the local socket live in `$XDG_RUNTIME_DIR` (owner-only). The socket uses
`QLocalServer::UserAccessOption`. A second instance can only ask the first one to select a
database on the login screen or to raise its window. It cannot unlock anything.

## Random numbers

Passwords, the 12-digit second-factor tokens and the AES IVs come from the OS CSPRNG
(`QRandomGenerator::system()` and OpenSSL `RAND_bytes`).

## Review log (T23, 2026-11-05)

| # | Finding | Fix | Regression test (`test_app.…`) |
|---|---|---|---|
| 1 | New database files, their `.bak` copy and exported databases were created with `0666 & ~umask`, which is world-readable with the usual umask 022 | new files are owner-only (`FilePermissions.h`); files the user already shares keep their permissions | `testNewDatabaseFilesOwnerOnly` |
| 1b | PDF exports, which can contain plaintext passwords, were world-readable | same | `testPdfExportOwnerOnly` |
| 2 | The decrypted item JSON was a `QByteArray`. With a UTF-8 BOM it was copied and the original freed without wiping, and the `WrongFormat` path returned before the wipe | decrypt into a `SecureBuffer` (new `CryptoHelper::decryptAes` overload); BOM skipped without copying | `testDecryptedPlaintextWiped` |
| 3 | Locking left a copied password on the clipboard until the timer ran out | clear on `DatabaseService::closed` (only if it is still ours) | `testLockClearsClipboard` |
| 4 | The password generator kept its last password after logout, and the export password field kept its text after the dialog closed | cleared on lock / on dialog close | `testLockForgetsGeneratedPassword` |
| 5 | Core dumps were only blocked with Linux `PR_SET_DUMPABLE` | also `RLIMIT_CORE = 0` (all Unix) | `testCoreDumpLimitZero` |
| 6 | Log files were created with umask permissions and only then changed, and the log folder was world-listable | created 0600, folder 0700 | `testLogFilesOwnerOnly` |

All seven tests failed on the code before the fixes and pass after them.

Checked and found fine: random number sources, the leak-check protocol, permissions of the
single-instance socket, deletion of the portal screenshot, log redaction (existing test), and
that favicons are off by default (P8, T7).

Still open (documented above, not fixed in T23):
- item fields as `QString`
- the Google favicon fallback and the plaintext favicon cache
- clearing the clipboard while unfocused on Wayland
- Windows Error Reporting dumps
- clipboard hint on macOS
