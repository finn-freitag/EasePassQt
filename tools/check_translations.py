#!/usr/bin/env python3
"""CTest check (steps.txt T20): the committed .ts files are up to date with the sources.

Runs lupdate (same options as the update_translations target) on temporary copies of
i18n/*.ts and compares the set of (context, source, numerus) messages with the committed files.
Fails when a qsTr()/tr() text is missing (lupdate not run after a UI change) or a stale one is
still present, and when a translation in a non-English file is unfinished.

Usage: check_translations.py --lupdate <exe> --cmake <exe> --generator <GenerateLUpdateProject.cmake>
                             --project-in <.lupdate/<name>_lupdate_project.cmake> --i18n <dir>
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


def messages(ts: Path):
    root = ET.parse(ts).getroot()
    out = {}
    for ctx in root.iter("context"):
        name = ctx.findtext("name")
        for m in ctx.iter("message"):
            tr = m.find("translation")
            key = (name, m.findtext("source"), m.get("numerus") == "yes")
            out[key] = None if tr is None else tr.get("type")
    return out


def retarget(node, mapping):
    if isinstance(node, dict):
        if "translations" in node:
            node["translations"] = [mapping.get(Path(p).name, p) for p in node["translations"]]
        for v in node.values():
            retarget(v, mapping)
    elif isinstance(node, list):
        for v in node:
            retarget(v, mapping)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--lupdate", required=True)
    ap.add_argument("--lconvert", required=True)
    ap.add_argument("--cmake", required=True)
    ap.add_argument("--generator", required=True)
    ap.add_argument("--project-in", required=True)
    ap.add_argument("--i18n", required=True, type=Path)
    args = ap.parse_args()

    committed = sorted(args.i18n.glob("*.ts"))
    if not committed:
        print("no .ts files found", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        mapping = {}
        for ts in committed:
            shutil.copy(ts, tmp / ts.name)
            mapping[ts.name] = str(tmp / ts.name)
        project = tmp / "project.json"
        subprocess.run([args.cmake, f"-DIN_FILE={args.project_in}", f"-DOUT_FILE={project}", "-P", args.generator],
                       check=True)
        data = json.loads(project.read_text())
        retarget(data, mapping)
        project.write_text(json.dumps(data))
        subprocess.run([args.lupdate, "-project", str(project), "-no-obsolete", "-locations", "none", "-silent"],
                       check=True)
        failed = False
        for ts in committed:
            fresh = tmp / ts.name
            if ts.name == "easepass_en.ts":  # plurals only, like the update target
                subprocess.run([args.lconvert, "-pluralonly", "-i", str(fresh), "-o", str(fresh)], check=True)
            old, new = messages(ts), messages(fresh)
            missing = sorted(set(new) - set(old))
            stale = sorted(set(old) - set(new))
            for ctx, src, _ in missing:
                print(f"{ts.name}: missing [{ctx}] {src!r} (run the update_translations target)")
            for ctx, src, _ in stale:
                print(f"{ts.name}: obsolete [{ctx}] {src!r} (run the update_translations target)")
            unfinished = [k for k, t in old.items() if t in ("unfinished",) and k in new]
            for ctx, src, _ in unfinished:
                print(f"{ts.name}: unfinished [{ctx}] {src!r}")
            failed = failed or bool(missing or stale or unfinished)
        if not failed:
            print(f"{len(committed)} translation files up to date")
        return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
