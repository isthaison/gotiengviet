#!/bin/sh
# Build data/dict-vi.txt from public seeds + our own vocabulary.
# Usage: ./tools/mkdict.sh [TAG]   (default: v$(cat VERSION))
# Sources (see header of the generated file for attribution):
#   - duyet/vietnamese-wordlist Viet22K + Viet74K (GPL-2.0)
#   - data/learned-words.txt (this repo, MIT)
# Needs: curl, python3. Output: UTF-8, NFC, lowercase, one word/line.
set -eu
cd -- "$(dirname -- "$0")/.."

TAG="${1:-v$(cat VERSION)}"
TMPD="$(mktemp -d)"
trap 'rm -rf "$TMPD"' EXIT

echo "mkdict: downloading seeds..." >&2
curl -sSL --max-time 120 -o "$TMPD/v22.txt" \
  https://raw.githubusercontent.com/duyet/vietnamese-wordlist/master/Viet22K.txt
curl -sSL --max-time 120 -o "$TMPD/v74.txt" \
  https://raw.githubusercontent.com/duyet/vietnamese-wordlist/master/Viet74K.txt

echo "mkdict: filtering..." >&2
export PYTHONUTF8=1 PYTHONIOENCODING=utf-8
python3 - "$TMPD/v22.txt" "$TMPD/v74.txt" data/learned-words.txt "$TAG" > data/dict-vi.txt <<'EOF'
import sys, unicodedata

v22, v74, seed, tag = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]

def load(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read().splitlines()
    except OSError as e:
        print("mkdict: cannot read %s: %s" % (path, e), file=sys.stderr)
        return []

words = set()
for path in (v22, v74, seed):
    for raw in load(path):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        if " " in s or "\t" in s:
            continue  # phrases are not word completions
        s = unicodedata.normalize("NFC", s.lower())
        if not (2 <= len(s) <= 24):
            continue
        if any(not (c.isalpha() and c.islower()) for c in s):
            continue
        words.add(s)

print("# GoTiengViet Vietnamese dictionary: one word per line (UTF-8 NFC lowercase).")
print("# Seed sources, curated/filtered/merged by tools/mkdict.sh:")
print("#   - duyet/vietnamese-wordlist Viet22K + Viet74K (GPL-2.0; Ho Ngoc Duc wordlist)")
print("#   - data/learned-words.txt (this repo, MIT)")
print("# Regenerate: ./tools/mkdict.sh vX.Y.Z")
print("# Format: '#' comments and blank lines ignored; validated with gtv_word_key on load.")
print("# dict-tag: %s" % tag)
for w in sorted(words):
    print(w)
print("mkdict: %d words" % len(words), file=sys.stderr)
EOF

echo "mkdict: wrote data/dict-vi.txt" >&2
