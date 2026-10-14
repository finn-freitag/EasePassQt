# Test fixtures

Rules: steps.txt section 3.3. Fixtures must never be edited to make a failing
compatibility test pass; any change needs a documented reason in this file.

| File | Password | Second factor | Provenance | Expected content |
|------|----------|---------------|------------|------------------|
| `test_db.epdb` | `SecretPassword123!` | none | Written by a standalone C# re-implementation of the EasePass epdb v1.4 writer (.NET + Konscious.Security.Cryptography Argon2id, same parameters as EasePass). **Not** produced by the real EasePass app. | 1 item: DisplayName `Google Account`, Username `john_doe`, Password `myP@ssw0rd!`, Email `john@example.com`, Website `https://accounts.google.com`, Notes `Some notes`, Secret `JBSWY3DPEHPK3PXP`, Digits `6`, Interval `30`, Algorithm `SHA1`, Clicks `["1.10.2026"]`, Tags `["tech","mail"]` |

sha256 `test_db.epdb`: `d887abb81a2d538ef7c1ed6c36ae4f0718c1b308ad23b31a17955bbd8dc3f6c7`

Moved here in T0 (2026-10-15) from a machine-specific absolute path that the
existing `testDecryptCSharpDatabase` test pointed to; the file is unchanged.

Fixtures produced by the real C# EasePass app (with/without second factor, unicode
password, 0/1/many items, comma associated-data variant) are added in T1.

`../../test.epdb` (password `123456`) in the repository root is a separate file for manual
testing; it must not be modified either.
