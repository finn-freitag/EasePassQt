# Plugin runtime decision (T26)

Date: 2026-11-13. Question 4 of steps.txt section 7: do plugins use the WebAssembly
**Component Model with WIT**, or core modules with a hand-written message ABI?

## Decision

**Components + WIT**, hosted with the **Wasmtime C API**. No core-module fallback.

The interface is the WIT package `easepass:plugin@1.0.0` of
[easepassqt-plugin-spec](https://github.com/finn-freitag/easepassqt-plugin-spec). The fallback
of section 4.4 ("core modules with a small versioned message ABI") is not needed, because
both hosts can load components.

## What was tested

The same file, `tests/fixtures/plugins/conformance-importer.wasm` (33 944 bytes, built once
from the Rust conformance plugin), was run in two hosts:

| Check | Qt host: Wasmtime 48.0.1, C API (`tests/plugin`) | .NET host: wasmtime-dotnet 50.0.1 (`tools/plugin-poc-dotnet`) |
| --- | --- | --- |
| Read imports/exports without instantiating | yes (`wasmtime_component_type_*`) | yes, but only by calling the same C functions of the bundled library (no managed wrapper) |
| Forbidden import (`wasi:filesystem`) detected | yes, rejected | yes, visible |
| Import not defined in the linker: instantiation fails | yes | yes |
| `sources()` and `import-passwords("fixed")` incl. Unicode, TOTP, tags | yes | yes |
| Host function `log` | yes | yes |
| Host function `pick-file` (bytes into the plugin), cancel | yes | yes |
| Plugin returns an error value | yes | yes |
| Plugin panics (trap): host continues | yes | yes |
| Infinite loop interrupted (200 ms limit) | yes, after about 200 ms | yes, after about 200 ms |
| Memory cap (16 MiB): allocation loop stopped | yes | yes |
| A hanging plugin does not disturb another one | yes | not tested |

Qt host: 23 QtTest cases in `test_plugin`, all passing. .NET host: 14 checks, all passing.
The .NET host ran on Linux (.NET 8); it was **not** run on Windows or inside the Windows
application.

## Why components

- One typed interface definition (WIT) that both applications and plugin authors share.
  Bindings for plugins are generated (`wit-bindgen`); nobody writes serialization code.
- Permissions fall out of the format: a component lists the interfaces it imports by name
  and version, which is exactly what the permission table maps (steps.txt 4.2).
- A component built for `wasm32-unknown-unknown` imports nothing but what its code calls.
  The importer imports `log`, `types` and `file-picker` and nothing else.
- A message ABI would have to be designed, versioned and implemented twice (C++ and C#),
  plus once per plugin language.

## What it costs

1. **`list<u8>` is expensive in the dynamic value API.** A component value is a 32-byte
   tagged union in the C API, and a byte list is one value per byte. Measured with the
   Qt host passing a file through `pick-file`:

   | File size | Time for the whole import call | Note |
   | --- | --- | --- |
   | 1 MiB | 0.2 s | |
   | 8 MiB | 1.6 s | |
   | 32 MiB | failed | the plugin hit its 256 MiB memory cap; host peak memory 2.8 GiB |

   Consequence: the spec now caps byte lists at 8 MiB per call (about 256 MiB of host
   memory for a moment). Database files are far below that. Larger transfers need several
   calls. A core-module ABI would not have this cost; it was judged less important than
   the points above.
2. **wasmtime-dotnet is behind the C API.** It has no managed API for import/export
   inspection (works through P/Invoke, shown in the .NET host) and no support for resource
   and map values. Consequence: the spec no longer uses a WIT resource for TCP
   connections; `network` uses `u32` handles.
3. **Size.** `libwasmtime.so` is about 50 MB in nixpkgs (29 MB in the NuGet package).
4. **Inspection compiles the component.** `wasmtime_component_new` runs the compiler
   before the imports can be read (about 10 ms for the importer). No plugin code runs, but
   untrusted input reaches the compiler before the allow-list check. T27 should cap the
   file size first and may replace this with a small parser of the import section.

## Findings for the host implementation (T27)

- The linker must define an empty instance for `easepass:plugin/types`; components import
  it although it contains only types.
- Timeouts: epoch interruption. One ticker thread advances the engine epoch every 20 ms
  while a call runs; each store checks its **own** deadline in the epoch callback, so a
  slow plugin does not interrupt others. Time spent in a host dialog is added back to the
  deadline.
- Memory: `wasmtime_store_limiter`. A Rust plugin that cannot grow its memory aborts, which
  the host sees as a trap (`unreachable`), not as a distinct "out of memory" error.
- After a trap or timeout the instance must not be entered again (`PluginError::Unusable`);
  create a new store and instance.
- Plugins must be built for `wasm32-unknown-unknown` and turned into a component with
  `wasm-tools component new`. Components for `wasm32-wasip2` are rejected by the
  allow-list. (That the WASI targets add such imports was not measured here.)
- Fuel was not used. Epochs are cheaper and the limit that matters is wall-clock time.

## Code

The proof of concept grew into the plugin host of T27; see [PLUGIN_HOST.md](PLUGIN_HOST.md).
The .NET comparison host is in `tools/plugin-poc-dotnet/`.

## Open

- **Windows build of the Qt app.** vcpkg has no Wasmtime port; the Windows CI job therefore
  builds without the plugin host for now. The C API release archive from the Wasmtime
  GitHub releases works with `-DWasmtime_ROOT=...`; wiring that into CI is left for T27.
- **Running inside the real Windows application** (WinUI, .NET 8 on Windows) is up to the
  Windows app; only the library was exercised here.
