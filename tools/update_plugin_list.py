#!/usr/bin/env python3
"""Writes the SHA-256 of every package in plugins/ into plugins/PluginList.json.

Run it after replacing a package (plugins/<id>.epplugin, built by the plugin's own
repository with tools/build.sh). Version, name and description are taken from the
package's manifest. The test plugins.list_matches_packages fails if this was forgotten.
"""
import hashlib
import json
import zipfile
from datetime import date
from pathlib import Path

PLUGINS = Path(__file__).resolve().parent.parent / "plugins"


def main() -> None:
    path = PLUGINS / "PluginList.json"
    catalog = json.loads(path.read_text(encoding="utf-8"))
    changed = False
    for entry in catalog["plugins"]:
        package = PLUGINS / f"{entry['id']}.epplugin"
        with zipfile.ZipFile(package) as archive:
            manifest = json.loads(archive.read("manifest.json"))
        new = dict(entry, sha256=hashlib.sha256(package.read_bytes()).hexdigest(), version=manifest["version"],
                   name=manifest["name"])
        # The download address carries the version: releases/download/v<version>/<id>.epplugin
        base = entry["download_url"].split("/releases/download/")[0]
        new["download_url"] = f"{base}/releases/download/v{manifest['version']}/{entry['id']}.epplugin"
        if new != entry:
            entry.update(new)
            changed = True
            print(f"updated {entry['id']} {entry['version']} {entry['sha256']}")
    if changed:
        catalog["updated"] = f"{date.today().isoformat()}T00:00:00Z"
        path.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    else:
        print("nothing to update")


if __name__ == "__main__":
    main()
