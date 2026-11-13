# Plugin proof of concept: wasmtime-dotnet host

Part of T26 (see [docs/PLUGIN_DECISION.md](../../docs/PLUGIN_DECISION.md)). Loads the same
conformance importer component as `tests/plugin` in the .NET binding of Wasmtime, which is
what the Windows application would use. Not built by CMake or CI.

```bash
dotnet run --project tools/plugin-poc-dotnet -- tests/fixtures/plugins
```

Without a .NET SDK on Nix:

```bash
nix shell nixpkgs#dotnet-sdk -c dotnet run --project tools/plugin-poc-dotnet -- tests/fixtures/plugins
```

It prints one PASS/FAIL line per check and exits with the number of failures.
