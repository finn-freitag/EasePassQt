#!/usr/bin/env python3
"""Fill Qt Linguist .ts files from the C# Ease Pass .resw resources (steps.txt T20).

The Qt port has its own English source texts (qsTr/tr). This script matches each source text
against the en-US values of the C# Resources.resw (case, surrounding whitespace and trailing
punctuation ignored) and copies the de-DE / pt-BR value of the same key into the .ts file.
Matched messages get a translator comment "resw: <Key>" so the link to the upstream key stays
visible and later runs can refresh them when upstream changes.

Rules (idempotent: a second run changes nothing):
  * finished translations without a "resw:" comment are manual and never touched;
  * messages linked to a resw key are refreshed from the .resw value;
  * unmatched, untranslated messages stay "unfinished" (the app falls back to English);
  * plural (numerus) messages are never filled from .resw (C# has no plural forms).

Usage:
  tools/resw2ts.py --resw-dir <C# repo>/EasePass/Strings i18n/easepass_de.ts i18n/easepass_pt_BR.ts
  tools/resw2ts.py --stats ...   (report only, write nothing)
"""

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

RESW_PREFIX = "resw: "
# .ts file language -> .resw folder
LANG_DIRS = {"de": "de-DE", "de_DE": "de-DE", "pt_BR": "pt-BR", "pt": "pt-BR", "en": "en-US"}


def normalize(text: str) -> str:
    t = text.replace("&", "").replace("…", "...")
    t = re.sub(r"\s+", " ", t).strip().lower()
    t = t.rstrip(" .:!?")
    return t


def read_resw(path: Path) -> dict:
    """key -> value (keys with '/' like 'X/Text' are XAML properties; the part before is the uid)."""
    root = ET.parse(path).getroot()
    out = {}
    for data in root.findall("data"):
        value = data.find("value")
        if value is None or value.text is None:
            continue
        out[data.get("name")] = value.text
    return out


def build_index(english: dict) -> dict:
    """normalized English text -> sorted list of keys (several keys can share a text)."""
    index = {}
    for key, value in english.items():
        if "@@@" in value:  # C# placeholders do not match Qt's %1
            continue
        index.setdefault(normalize(value), []).append(key)
    for keys in index.values():
        keys.sort()
    return index


def pick_translation(keys, target: dict):
    """First key (sorted) that has a non-empty translation."""
    for key in keys:
        value = target.get(key)
        if value and value.strip():
            return key, value
    return None, None


def process(ts_path: Path, resw_dir: Path, stats_only: bool) -> dict:
    tree = ET.parse(ts_path)
    root = tree.getroot()
    lang = root.get("language", "")
    folder = LANG_DIRS.get(lang)
    if folder is None:
        raise SystemExit(f"{ts_path}: unknown language '{lang}'")
    english = read_resw(resw_dir / "en-US" / "Resources.resw")
    target = read_resw(resw_dir / folder / "Resources.resw")
    index = build_index(english)
    counts = {"messages": 0, "matched": 0, "manual": 0, "unfinished": 0, "changed": 0}

    for message in root.iter("message"):
        counts["messages"] += 1
        source = message.findtext("source") or ""
        translation = message.find("translation")
        if translation is None:
            translation = ET.SubElement(message, "translation", {"type": "unfinished"})
        comment = message.find("translatorcomment")
        linked = comment is not None and (comment.text or "").startswith(RESW_PREFIX)
        finished = translation.get("type") not in ("unfinished", "vanished", "obsolete")
        has_text = bool((translation.text or "").strip()) or len(translation) > 0

        if message.get("numerus") == "yes":
            if not finished:
                counts["unfinished"] += 1
            else:
                counts["manual"] += 1
            continue
        if finished and has_text and not linked:
            counts["manual"] += 1
            continue

        key, value = pick_translation(index.get(normalize(source), []), target)
        if key is None:
            if linked:  # upstream key disappeared: keep the text but mark it for review
                if translation.get("type") != "unfinished":
                    translation.set("type", "unfinished")
                    counts["changed"] += 1
            if not finished or linked:
                counts["unfinished"] += 1
            continue

        # Keep the source's trailing punctuation style if upstream lacks it (e.g. "Name" vs "Name:").
        value = value.strip()
        src_tail = re.search(r"[.:!?]+$", source.strip())
        val_tail = re.search(r"[.:!?]+$", value)
        if src_tail and not val_tail:
            value += src_tail.group(0)
        elif val_tail and not src_tail:
            value = value[: val_tail.start()].rstrip()

        new_comment = RESW_PREFIX + key
        changed = (translation.text != value or "type" in translation.attrib
                   or comment is None or comment.text != new_comment)
        if changed:
            counts["changed"] += 1
            translation.text = value
            translation.attrib.pop("type", None)
            if comment is None:
                comment = ET.Element("translatorcomment")
                # lupdate order: ... source, comment, extracomment, translatorcomment, translation
                message.insert(list(message).index(translation), comment)
            comment.text = new_comment
        counts["matched"] += 1

    if not stats_only and counts["changed"]:
        write_ts(tree, ts_path)
    return counts


def write_ts(tree, path: Path) -> None:
    """Writes in lupdate's layout (4-space indent, DOCTYPE) so lupdate runs cause no churn."""
    ET.indent(tree, space="    ")
    body = ET.tostring(tree.getroot(), encoding="unicode", short_empty_elements=True)
    body = body.replace(" />", "/>")
    text = '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n' + body + "\n"
    path.write_text(text, encoding="utf-8")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--resw-dir", required=True, type=Path, help="C# Strings directory")
    parser.add_argument("--stats", action="store_true", help="only print statistics")
    parser.add_argument("ts", nargs="+", type=Path)
    args = parser.parse_args(argv)
    for ts in args.ts:
        c = process(ts, args.resw_dir, args.stats)
        print(f"{ts}: {c['messages']} messages, {c['matched']} from resw, {c['manual']} manual, "
              f"{c['unfinished']} unfinished, {c['changed']} changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
