#!/usr/bin/env bash
# strkey.sh <regex on English text> [lang]  — lists twine keys with English and translated values
# (default language: de).
F="$(dirname "$0")/../organicmaps/data/strings/strings.txt"
awk -v re="$1" -v lang="${2:-de}" '
/^\[\[/ {next}
/^\[/ {key=$0; en=""; hit=0; next}
/^en = / {en=substr($0,6); hit = (en ~ re); if (hit && lang == "en") {printf "%-45s | %s\n", key, en; hit=0}; next}
index($0, lang " = ") == 1 {if (hit) printf "%-45s | %s | %s\n", key, en, substr($0, length(lang) + 4); hit=0}
' "$F"
