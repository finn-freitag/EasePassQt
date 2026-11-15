# Plugin host (T27), plugin UI (T28) and official plugins (T29)

How Ease Pass runs plugins. The interface itself is specified in
[easepassqt-plugin-spec](https://github.com/finn-freitag/easepassqt-plugin-spec); why
WebAssembly components were chosen is in [PLUGIN_DECISION.md](PLUGIN_DECISION.md).

The host is the library `easepass_plugin` (`src/plugin`). It is built when Wasmtime is
found; the application then gets its plugin UI (`App::PluginsViewModel`, see
[Catalog and UI](#catalog-and-ui-t28)). Without Wasmtime the application builds without
any plugin UI.

## Pieces

| Class | Job |
| --- | --- |
| `PluginPackage`, `PluginManifest` | Reads a `.epplugin` file: SHA-256 first, then a small ZIP reader that only reads `manifest.json`, `plugin.wasm` and `icon.png`, then manifest validation |
| `PluginInspector` | Computes types and permissions from the component's exports and imports, rejects anything outside the allow-list. Runs no plugin code |
| `PluginInstance` | One instantiated component: explicit linker, memory cap, call timeout, errors instead of crashes. Converts between Wasmtime values and `Value` |
| `PluginHostFunctions` | The host side of the interface. The interface of a permission is only built when that permission is approved |
| `PluginStorage` | Private key/value settings and files of one plugin, with quotas |
| `PluginNetwork`, `NetworkAllowList` | Every connection of a plugin: manifest allow-list, private-address block, redirects by hand, caps |
| `PluginWorker` | One thread per plugin. Calls are asynchronous; a crashed instance is replaced on the next call |
| `PluginManager` | Install, verify, enable, load, uninstall. Keeps `installed.json` |
| `PluginAdapters` | Typed front ends: `PluginImporter`, `PluginDatabaseProvider` / `PluginDatabaseSource` (a `Core::IDatabaseSource`), `PluginPasswordGenerator`, `PluginStoreFeed`, `PluginAbout` |

Files on disk, below `<AppDataLocation>/plugins`:

```
installed.json                 records: hash, computed types and permissions, approved permissions, switches
packages/<id>.epplugin         the installed package, byte for byte
storage/<id>/settings.json     the plugin's key/value settings
storage/<id>/files/<name>      the plugin's files
```

## What is checked, and when

**Install** (`PluginManager::prepareInstall`, then `install` after the user confirmed):

1. Size cap, then the SHA-256 of the file. For an install from the store it must equal the
   catalog entry; otherwise the install stops before anything is parsed.
2. Catalog lookup by hash: verified, unverified, offline or revoked (blocked).
3. Package and manifest are parsed and validated.
4. API version, operating system and minimum application version.
5. The component is inspected. Forbidden imports or exports reject the package. If the
   manifest lists permissions, they must equal the computed ones.
6. `install` repeats steps 1 to 5 on the bytes it is about to store and requires that the
   user approved every computed permission.

**Load** (`PluginManager::load`, every time):

1. The stored file must still have the hash recorded at install time.
2. The catalog must not have revoked it (being offline does not block an installed plugin).
3. Package, manifest and component are parsed and inspected again; the permissions
   computed now must all have been approved. `installed.json` is never trusted for what a
   plugin may do.
4. Only the host interfaces of the computed permissions are linked.

## Rules the host enforces at run time

- **Time:** 30 s per call by default (120 s for database transfers). Time the user spends
  in a file dialog does not count. A timeout or crash is returned as `PluginError`; the
  instance is dropped and a new one is created for the next call.
- **Memory:** 64 MiB of linear memory per instance.
- **Byte lists:** 8 MiB per call in each direction that the host controls. In practice
  Wasmtime allows less, see "Data per call" under Known limits; a database handled by a
  `database-source` plugin is limited to 2 MiB (`PluginLimits::MaxDatabaseBytes`).
- **Storage:** 64 MiB, 1024 keys, 1024 files per plugin. File names are plain names;
  nothing outside `storage/<id>/` can be named.
- **Network:** host and port must match `network_hosts` of the manifest before the name
  is resolved. The resolved address must be public unless the user set "allow local
  network" for that plugin, and the connection goes to exactly the address that was
  checked. Redirects are followed by the host and each hop passes the same checks. The
  host sets `Host`, `Connection`, `Content-Length` and similar headers itself; a plugin
  cannot override them. TLS certificates are always verified.
- **Log:** control characters removed, 4096 bytes per message, 200 messages per 10 s.

## Catalog and UI (T28)

**Catalog** (`PluginCatalog.{h,cpp}`): `PluginCatalogData::parse` validates a
`PluginList.json` like the JSON schema of the spec (one invalid entry makes the whole file
invalid). `CatalogClient` fetches it over HTTPS only, with a timeout and a size cap, and
implements `IPluginCatalog`:

| Catalog state | Hash in the official list | Hash not in the list |
| --- | --- | --- |
| fetched by the latest `refresh()` | verified (or revoked) | unverified |
| only the disk cache, or nothing | offline (or revoked) | offline |

The cache (`<plugins>/PluginList.json`) is for *showing* the store while the list cannot
be reached; it never makes a package "verified". The one thing taken from it is a
revocation. Catalogs named by `store-feed` plugins are read with the same rules and never
verify anything. The official list is
`https://raw.githubusercontent.com/finn-freitag/EasePassQt/main/plugins/PluginList.json`
(`plugins/PluginList.json` of this repository, see "Official plugins"). Nothing is fetched at application start: the list is
loaded when the plugin page opens or a file is installed.

**View model** (`src/app/PluginsViewModel`, QML singleton `Plugins`, owns the
`PluginManager` and the `CatalogClient`):

- Install from a file or from the store: hash, catalog verification and inspection first
  (`PluginManager::prepareInstall`), then `pendingInstall` for the dialogs. Unverified and
  offline packages get the extra window first (`installStep`), revoked ones are refused.
  The texts of the permission dialog are built from the **computed** types and
  permissions and the allow-list of servers; the plugin's own description is not part of
  them.
- Store list with the operating-system filter (`showIncompatible`), update detection and
  revocation: a plugin whose hash is revoked in the catalog is disabled and cannot be
  enabled again.
- `pick-file`: the plugin thread waits while the UI thread shows the file dialog
  (`filePickRequested` / `resolveFilePick`); the dialog title names the plugin.
- Settings > Plugins: import passwords through `importer` plugins (the items are
  candidates until the user selected them; they are dropped when the database is
  locked), and the settings form of `database-source` plugins, drawn from the plugin's
  JSON Schema (`configFieldsFromSchema`: flat object with string, password, choice,
  integer, number and boolean fields). The fields are ordered by the properties'
  `propertyOrder` numbers (a JSON object has no order of its own), then by key. A property
  listed in `required` must not be left empty in the form.

### Plugins at work in the application

- **Databases of `database-source` plugins.** `PluginsViewModel::remoteDatabases()` holds
  the databases of all enabled plugins of this type. They are asked at start, after
  every change of the installed plugins, after a provider's settings were saved, and by
  the "look again" button of the login page (shown only while such a plugin is
  installed). `AppContext` hands the list to `DatabaseController` and
  `DatabaseManagementViewModel` as `RemoteDatabase` values (`src/app/RemoteDatabase.h`).
  The pages address a database by a string: a file path, or for these the key
  `plugin://<plugin id>/<database id>`. That key is also what is stored as last
  database, so a plugin's database is preselected at the next start.
  - The name in lists is "<database name> (<plugin name>)", shortened to one line; the
    plugin cannot make its database look like a local file.
  - Opening, saving, read-only and "locked by another user" go through
    `Core::DatabaseService` like for a local file; the plugin only sees encrypted bytes.
  - On the database page such a database can be loaded, but not renamed, deleted or
    copied as a path.
  - A plugin whose database is open cannot be uninstalled, disabled or updated (the open
    database would keep running the old code). If the plugin is disabled from outside
    (revocation), the database is closed.
  - Only local files decide whether the Register page is shown at the first start.
- **`password-generator` plugins.** `PluginsViewModel::passwordGenerators()` feeds
  `PasswordGeneratorViewModel::setExternalGenerators`. The generator dialog shows a box
  to choose the generator while at least one such plugin is enabled; the choice is
  stored in `AppSettings::passwordGeneratorPlugin` and also applies to the generate
  button of the item dialog. Installing a plugin does not select it. The plugin gets
  length, characters and "verify not leaked"; what it returns is refused if it is empty,
  longer than 4096 characters or contains control characters, and is otherwise checked
  for leaks and rated by the host like any password. A failing plugin is shown as an
  error; the built-in generator never silently takes over.

**QML:** `PluginsPage.qml`, `PluginDialogs.qml` (the two install dialogs and the file
dialogs), `PluginImportDialog.qml`, `PluginConfigDialog.qml`, and the section "Plugins"
of `SettingsPage.qml`.

## Known limits

- **A returned byte list is converted before the host sees its size.** The 64 MiB memory
  cap bounds it, but a hostile plugin can still make the host allocate about 2 GiB for a
  moment. Avoiding that needs a change of the interface (bytes as text) or a typed call
  API in Wasmtime.
- **Data per call: about 2.6 MiB.** Wasmtime stops a call when more than 128 MiB of its
  value type were copied between host and plugin, and a byte costs about 24 bytes there.
  The C API of Wasmtime 48 cannot raise that. A database passes the boundary twice
  (network to plugin, plugin to Ease Pass), so databases of plugins are limited to 2 MiB
  and larger ones are refused with a message. Found with the FTP plugin and a 3 MB file.
- **TLS is only covered by a test that needs servers.** Certificate verification is
  switched on in code and a test checks that TLS against a plain server fails. A real TLS
  server is only used by `OfficialPluginsTest::testFtpPluginAgainstServers` (FTPS with a
  test certificate trusted through `PluginEnvironment::extraCaCertificates`), which is
  skipped unless the servers of the FTP plugin's repository are running.
- **No TLS session reuse** between connections of a plugin. FTPS servers that demand it
  for the data connection refuse the transfer.
- **Inspection compiles the component** (`wasmtime_component_new`) before the import list
  can be read. The package size is capped first.
- **Shutdown** interrupts running plugin code, but a host function that is blocking
  (a network read, an open file dialog) finishes first.
- **Windows:** the Windows CI job builds without the plugin host (no vcpkg port of
  Wasmtime). Nothing in `src/plugin` was compiled or run on Windows.
- **A new database cannot be created at a plugin.** The app opens what a
  `database-source` plugin offers; "Create database" and "Import database" only work
  with local files. A plugin's storage must already hold a database.
- **Plugin databases are asked for at start.** With a `database-source` plugin installed
  and enabled, its code runs when the application starts (and may use the network it was
  allowed). Nothing changes in the list while the plugin is not asked again.
- **The catalog is not signed** (spec version 1): "verified" means "the hash is in the
  list served by GitHub over HTTPS".
- **Installing blocks the UI briefly:** hashing and inspecting a package runs on the UI
  thread.
- Plugin log lines go to the logging category `easepass.plugin.log` (debug level, so not
  into the log file by default; warnings and errors of a plugin are logged).

## Official plugins (T29)

`plugins/` holds the plugin list the application fetches (`PluginList.json`) and, next to
it, the exact packages it lists. The packages are built in their own repositories (Rust,
`tools/build.sh` there) and published as release assets; `download_url` points at those.

| Plugin | Repository | What it does |
| --- | --- | --- |
| `com.finnfreitag.easepass.ftp-database` | [EasePassQTDatabaseSourcePlugin](https://github.com/finn-freitag/EasePassQTDatabaseSourcePlugin) | one database on an FTP, FTPS or SFTP server |
| `com.finnfreitag.easepass.cloud` | [EasePassQTCloudPlugin](https://github.com/finn-freitag/EasePassQTCloudPlugin) | databases on an Ease Pass Cloud server |
| `com.finnfreitag.easepass.url-database` | [EasePassQTRemoteDatabaseSourcePlugin](https://github.com/finn-freitag/EasePassQTRemoteDatabaseSourcePlugin) | a database file from a web address, read-only |

All three are of type `database-source` with the permissions `storage` and `network`, and
name `*:*` as servers because the user chooses the server (the install dialog warns about
that).

To publish a new version: build it in the plugin's repository, create a release there
with the package, copy the package to `plugins/`, run `tools/update_plugin_list.py`, and
commit. The test `plugins.list_matches_packages` (`tests/CheckPluginList.cmake`, runs in
every build including CI) fails if a package and the hash in the list differ.

A user sees a plugin as "verified" only when the pushed `PluginList.json` on the `main`
branch contains the hash of the package they install.

## Tests

`test_plugin` (`tests/plugin`): runtime, package, storage, network, manager, adapter and
catalog tests against the conformance plugins in `tests/fixtures/plugins`. The plugin UI
is tested in `test_app` (`tests/app/PluginsViewModelTest.cpp`, fake network) and
`test_qml` (`tests/qml/tst_plugins.qml`, with a fake view model and with the real one). The network tests use
a local server; names ending in `.conformance.test` are mapped to `127.0.0.1` by a test
resolver.

`OfficialPluginsTest` runs the packages of `plugins/` in the real host: list against
packages, computed types and permissions, the URL plugin and the Ease Pass Cloud plugin
against the local test server (which imitates the cloud's plugin API), and the FTP
plugin's settings. `testFtpPluginAgainstServers` runs the FTP plugin against real FTP,
FTPS and SFTP servers; it is skipped unless `EASEPASS_TEST_FTP_SERVERS` and
`EASEPASS_TEST_FTP_ROOT` are set (see the comment above the test). The protocol code of the
plugins is tested in their own repositories.
