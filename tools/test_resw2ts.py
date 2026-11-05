#!/usr/bin/env python3
"""Unit tests for resw2ts.py (steps.txt T20): matching, manual entries kept, idempotence."""

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import resw2ts  # noqa: E402

RESW = """<?xml version="1.0" encoding="utf-8"?>
<root>
  <data name="View_Settings_Headline.Text" xml:space="preserve"><value>{settings}</value></data>
  <data name="View_Login_Password.PlaceholderText" xml:space="preserve"><value>{password}</value></data>
  <data name="Dialog_Name.Text" xml:space="preserve"><value>{name}</value></data>
  <data name="Placeholder.Text" xml:space="preserve"><value>{placeholder}</value></data>
</root>
"""

TS = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de">
<context>
    <name>Page</name>
    <message>
        <source>Settings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Password:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Mein Name</translation>
    </message>
    <message>
        <source>Brand new text</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Hello %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <source>%n item(s)</source>
        <translation type="unfinished"><numerusform></numerusform><numerusform></numerusform></translation>
    </message>
</context>
</TS>
"""


class Resw2TsTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        for lang, values in {
            "en-US": dict(settings="Settings", password="Password", name="Name", placeholder="Hello @@@Name@@@"),
            "de-DE": dict(settings="Einstellungen", password="Passwort", name="Name (C#)", placeholder="Hallo @@@Name@@@"),
        }.items():
            (self.dir / lang).mkdir()
            (self.dir / lang / "Resources.resw").write_text(RESW.format(**values), encoding="utf-8")
        self.ts = self.dir / "easepass_de.ts"
        self.ts.write_text(TS, encoding="utf-8")

    def tearDown(self):
        shutil.rmtree(self.dir)

    def translations(self):
        import xml.etree.ElementTree as ET
        out = {}
        for m in ET.parse(self.ts).getroot().iter("message"):
            tr = m.find("translation")
            out[m.findtext("source")] = (tr.text, tr.get("type"), m.findtext("translatorcomment"))
        return out

    def test_fill_and_idempotent(self):
        counts = resw2ts.process(self.ts, self.dir, stats_only=False)
        self.assertEqual(counts["matched"], 2)
        t = self.translations()
        self.assertEqual(t["Settings"], ("Einstellungen", None, "resw: View_Settings_Headline.Text"))
        # Case/punctuation-insensitive match keeps the source's trailing colon.
        self.assertEqual(t["Password:"][0], "Passwort:")
        # Manual translation kept, unmatched and @@@ placeholders stay unfinished.
        self.assertEqual(t["Name"], ("Mein Name", None, None))
        self.assertEqual(t["Brand new text"][1], "unfinished")
        self.assertEqual(t["Hello %1"][1], "unfinished")
        self.assertEqual(t["%n item(s)"][1], "unfinished")

        first = self.ts.read_bytes()
        again = resw2ts.process(self.ts, self.dir, stats_only=False)
        self.assertEqual(again["changed"], 0)
        self.assertEqual(self.ts.read_bytes(), first)

    def test_upstream_change_refreshes_linked(self):
        resw2ts.process(self.ts, self.dir, stats_only=False)
        de = self.dir / "de-DE" / "Resources.resw"
        de.write_text(de.read_text(encoding="utf-8").replace("Einstellungen", "Optionen"), encoding="utf-8")
        resw2ts.process(self.ts, self.dir, stats_only=False)
        self.assertEqual(self.translations()["Settings"][0], "Optionen")

    def test_stats_only_writes_nothing(self):
        before = self.ts.read_bytes()
        resw2ts.process(self.ts, self.dir, stats_only=True)
        self.assertEqual(self.ts.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
