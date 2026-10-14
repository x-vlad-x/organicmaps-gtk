#!/usr/bin/env bash
# strkey.sh <regex on English text> [lang]  — lists string keys of the CoMaps Android resources
# with English and translated values (default language: de, i.e. values-de; "en" prints English only).
python3 - "$(dirname "$0")/../core/android" "$1" "${2:-de}" <<'PY'
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

root, pattern, lang = Path(sys.argv[1]), re.compile(sys.argv[2]), sys.argv[3]


def load(folder):
    strings = {}
    for path in root.glob(f"*/src/main/res/{folder}/*strings.xml"):
        for element in ET.parse(path).getroot().iter("string"):
            strings[element.get("name")] = "".join(element.itertext())
    return strings


english, translated = load("values"), load(f"values-{lang}")
for key, value in sorted(english.items()):
    if pattern.search(value):
        print(f"{key:45} | {value}" + ("" if lang == "en" else f" | {translated.get(key, '')}"))
PY
