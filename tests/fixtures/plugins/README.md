# Plugin test fixtures

Built from the conformance plugins of
[easepassqt-plugin-spec](https://github.com/finn-freitag/easepassqt-plugin-spec)
(commit `03dba75`, `tools/check.sh`, files from its `dist/` directory). Their behaviour is
specified in `conformance/README.md` of that repository.

| File | Types | Permissions |
| --- | --- | --- |
| `conformance-importer.wasm` | importer | pick-file |
| `conformance-database-source.wasm` | database-source | storage |
| `conformance-password-generator.wasm` | password-generator | leak-check |
| `conformance-store-feed.wasm` | store-feed | - |
| `conformance-network.wasm` | importer | network |
| `negative/*.wasm` | - | must be rejected (forbidden import, unknown namespace, future API version) |
| `packages/*.epplugin` | | the same components as installable packages (manifest + component) |
| `catalog/valid/*.json`, `catalog/invalid/*.json` | | `samples/catalog` of the spec repository (PluginList.json examples) |

To update: run `nix develop -c tools/check.sh` in the spec repository and copy
`dist/conformance-*.wasm`, `dist/negative/*.wasm` and `dist/*.epplugin` here.
