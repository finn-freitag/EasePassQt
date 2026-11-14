# Plugin host (T27)

How Ease Pass runs plugins. The interface itself is specified in
[easepassqt-plugin-spec](https://github.com/finn-freitag/easepassqt-plugin-spec); why
WebAssembly components were chosen is in [PLUGIN_DECISION.md](PLUGIN_DECISION.md).

The host is the library `easepass_plugin` (`src/plugin`). It is built when Wasmtime is
found and is **not linked into the application yet**: the UI that installs plugins and
uses them comes with T28.

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
- **Byte lists:** 8 MiB per call in each direction that the host controls.
- **Storage:** 64 MiB, 1024 keys, 1024 files per plugin. File names are plain names;
  nothing outside `storage/<id>/` can be named.
- **Network:** host and port must match `network_hosts` of the manifest before the name
  is resolved. The resolved address must be public unless the user set "allow local
  network" for that plugin, and the connection goes to exactly the address that was
  checked. Redirects are followed by the host and each hop passes the same checks. The
  host sets `Host`, `Connection`, `Content-Length` and similar headers itself; a plugin
  cannot override them. TLS certificates are always verified.
- **Log:** control characters removed, 4096 bytes per message, 200 messages per 10 s.

## What the application has to provide (T28)

`PluginEnvironment` carries the hooks; all of them are called on plugin threads:

- `pickFile`: show a file dialog on the UI thread and return name and content.
- `leakCheck`: `PluginManager::leakCheckWith(LeakCheckService *)` is ready to use.
- `removeStorageOnUninstall`: `AppSettings::removePluginSettingsOnUninstall`.
- `log`: where plugin log lines go (default: `qInfo`, category `easepass.plugin`).
- `setCatalog`: an `IPluginCatalog` (the catalog client of T28).

## Known limits

- **A returned byte list is converted before the host sees its size.** The 64 MiB memory
  cap bounds it, but a hostile plugin can still make the host allocate about 2 GiB for a
  moment. Avoiding that needs a change of the interface (bytes as text) or a typed call
  API in Wasmtime.
- **TLS is not covered by tests.** Certificate verification is switched on in code, and a
  test checks that TLS against a plain server fails, but there is no test with a real TLS
  server.
- **Inspection compiles the component** (`wasmtime_component_new`) before the import list
  can be read. The package size is capped first.
- **Shutdown** interrupts running plugin code, but a host function that is blocking
  (a network read, an open file dialog) finishes first.
- **Windows:** the Windows CI job builds without the plugin host (no vcpkg port of
  Wasmtime). Nothing in `src/plugin` was compiled or run on Windows.
- Reading a catalog, the install dialogs and the plugin pages are T28.

## Tests

`test_plugin` (`tests/plugin`): runtime, package, storage, network, manager and adapter
tests against the conformance plugins in `tests/fixtures/plugins`. The network tests use
a local server; names ending in `.conformance.test` are mapped to `127.0.0.1` by a test
resolver.
