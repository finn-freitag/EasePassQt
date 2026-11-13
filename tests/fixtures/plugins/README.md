# Plugin test fixtures

WebAssembly components built from the conformance plugins of
[easepassqt-plugin-spec](https://github.com/finn-freitag/easepassqt-plugin-spec)
(commit `1d68f77`, `tools/check.sh`, files from its `dist/` directory). Their behaviour is
specified in `conformance/README.md` of that repository.

| File | Types | Permissions |
| --- | --- | --- |
| `conformance-about.wasm` | about, initializer | - |
| `conformance-importer.wasm` | importer | pick-file |
| `conformance-database-source.wasm` | database-source | storage |
| `conformance-password-generator.wasm` | password-generator | leak-check |
| `conformance-store-feed.wasm` | store-feed | - |
| `negative/*.wasm` | - | must be rejected (forbidden import, unknown namespace, future API version) |

To update: run `nix develop -c tools/check.sh` in the spec repository and copy
`dist/conformance-*.wasm` and `dist/negative/*.wasm` here.
