# Test fixtures

Rules: steps.txt section 3.3. Fixtures must never be edited to make a failing
compatibility test pass; any change needs a documented reason in this file.

| File | Password | Second factor | Provenance | Expected content |
|------|----------|---------------|------------|------------------|
| `test_db.epdb` | `SecretPassword123!` | none | Written by a standalone C# re-implementation of the EasePass epdb v1.4 writer (.NET + Konscious.Security.Cryptography Argon2id, same parameters as EasePass). **Not** produced by the real EasePass app. | 1 item: DisplayName `Google Account`, Username `john_doe`, Password `myP@ssw0rd!`, Email `john@example.com`, Website `https://accounts.google.com`, Notes `Some notes`, Secret `JBSWY3DPEHPK3PXP`, Digits `6`, Interval `30`, Algorithm `SHA1`, Clicks `["1.10.2026"]`, Tags `["tech","mail"]` |
| `test.epdb` | `123456` | none | Produced by the official C# EasePass application (supplied by repository owner in commit 10681a3). | 4 items: Item 0: DisplayName `Test123`, Username `Julius`, Password `sooSicherOder?`, Email `mail@ph.de`, Notes `Baum.dummy`, Tags `["123124"]`; Item 1: DisplayName `Nudel`, Username `Kuchen`, Password `123123`, Email `Keks@mail.com`, Website `http://baum.dummy.com`, Notes `NOTITTEN`; Item 2: DisplayName `Peters Futterecken`, Username `Peter`, Password `umdieeckefuttern`, Email `futter@ecke.uk`, Website `http://nee.com`, Secret `BBJVUWUYKNKUOBIOYMPN`, Clicks `["14.10.2026"]`; Item 3: Empty item (all empty strings). |
| `qr_valid.png` | - | - | Generated for unit tests | QR code encoding `otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example` |
| `qr_rotated.png` | - | - | Generated for unit tests | Same QR code rotated 90 degrees |
| `qr_inverted.png` | - | - | Generated for unit tests | Same QR code with inverted pixels (white on black) |
| `qr_noisy.png` | - | - | Generated for unit tests | Same QR code with noise dots added |
| `non_qr.png` | - | - | Generated for unit tests | Solid color image containing no QR code |

sha256 `test_db.epdb`: `d887abb81a2d538ef7c1ed6c36ae4f0718c1b308ad23b31a17955bbd8dc3f6c7`  
sha256 `test.epdb`: `9b82bd88be531c1d0c89d33b1cb41b5a05acb7143ad35a12171a795077d2c859`  
sha256 `qr_valid.png`: `615776dcbd61956f8a4b0cf7eb018c2f669c4f4e887a86e96852065f328e84cb`  
sha256 `qr_rotated.png`: `e76ea2612e6a76194b6d404ce769f20fabd65e08369e4692f266a1924e44b97c`  
sha256 `qr_inverted.png`: `ff04609a1d775671355b4b0f25aa1507e954dd3505130ad03f1b3d65dba33a13`  
sha256 `qr_noisy.png`: `e17a1ff5fb3819a095afc9d9b86e88c27a49cc27773d9f5b70f1e378b0f2d2ed`  
sha256 `non_qr.png`: `f96cea77a99fd1941614d1ab3e53945860241aedba084e485e13d5b52329a74b`  

