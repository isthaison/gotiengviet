#!/bin/bash
# Bump the release version everywhere with one command.
#
# Usage: ./bump-version.sh X.Y.Z ["release notes for metainfo"]
#    or: make bump V=X.Y.Z
#
# Single writer for every place that carries the version, so a release
# can no longer ship with half-bumped files. Idempotent: re-running with
# the current version changes nothing.
set -euo pipefail
cd -- "$(dirname -- "$0")"

die() { echo "bump-version: $*" >&2; exit 1; }

ver="${1:?Usage: ./bump-version.sh X.Y.Z [\"release notes\"]}"
[[ "$ver" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "version must be X.Y.Z, got '$ver'"
notes="${2:-Phát hành $ver.}"
today=$(date +%F)
csv="$(echo "$ver" | tr . ,),0"    # 0,7,0,0 for VERSIONINFO

old=$(sed -n 's/^#define GTV_VERSION "\(.*\)"/\1/p' windows/version.h)
[ -n "$old" ] || die "cannot read current version from windows/version.h"

# --- C/C++ sources, headers, configs ---
sed -i "s/#define GTV_VERSION \"[^\"]*\"/#define GTV_VERSION \"$ver\"/" windows/version.h
sed -i "s/Tags like v[0-9.]*/Tags like v$ver/" windows/version.h
sed -i -E "s/^FILEVERSION[ \t]+[0-9,]+/FILEVERSION     $csv/" windows/resource.rc
sed -i -E "s/^PRODUCTVERSION[ \t]+[0-9,]+/PRODUCTVERSION  $csv/" windows/resource.rc
sed -i -E "s/(VALUE \"(File|Product)Version\", \")[0-9.]+/\\1$ver.0/" windows/resource.rc
sed -i "s|<version>[^<]*</version>|<version>$ver</version>|" ibus/gotiengviet.xml
sed -i 's/"[0-9][0-9.]*","GPL"/"'"$ver"'","GPL"/' ibus/engine.c

# --- metainfo: prepend a new release entry, keep history; skip if present ---
if grep -q "<release version=\"$ver\"" ibus/gotiengviet.metainfo.xml; then
    echo "(metainfo already has $ver, keeping existing entry)"
else
    esc_notes=$(printf '%s' "$notes" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g')
    awk -v ver="$ver" -v date="$today" -v notes="$esc_notes" '
        /<releases>/ {
            print
            print "    <release version=\"" ver "\" date=\"" date "\">"
            print "      <description>"
            print "        <p>" notes "</p>"
            print "      </description>"
            print "    </release>"
            next
        }
        1' ibus/gotiengviet.metainfo.xml > ibus/gotiengviet.metainfo.xml.new
    mv ibus/gotiengviet.metainfo.xml.new ibus/gotiengviet.metainfo.xml
fi

# --- Build, packaging, installer, CI defaults ---
sed -i "s/^VERSION ?= .*/VERSION ?= $ver/" Makefile.win
sed -i "s/^VERSION ?= .*/VERSION ?= $ver-1/" Makefile
sed -i 's/^version=${1:-.*}/version=${1:-'"$ver"'-1}/' package.sh
sed -i "s/#define AppVersion \".*\"/#define AppVersion \"$ver\"/" windows/installer.iss
sed -i "s/#define AppVerNum \".*\"/#define AppVerNum \"$ver.0\"/" windows/installer.iss
# installer.iss carries Vietnamese shortcut names: it must stay UTF-8
# with BOM or ISCC reads them in the system codepage (mojibake).
if [ "$(head -c 3 windows/installer.iss)" != "$(printf '\xef\xbb\xbf')" ]; then
    printf '\xef\xbb\xbf' | cat - windows/installer.iss > windows/installer.iss.new
    mv windows/installer.iss.new windows/installer.iss
    echo "(restored UTF-8 BOM in windows/installer.iss)"
fi
sed -i 's/\[ -n "\$VER" \] || VER="[^"]*"/[ -n "$VER" ] || VER="'"$ver"'"/' .github/workflows/build-windows.yml

echo "--- bumped $old -> $ver ---"
echo "--- leftover $old refs (history/tests/examples are fine) ---"
grep -rn "$old" --include="*.c" --include="*.h" --include="*.xml" --include="*.yml" \
    --include="*.iss" --include="Makefile*" --include="*.sh" . 2>/dev/null \
    | grep -v "^\./\.git" | grep -v "tests/" | grep -v "metainfo" || true
echo "--- done (review with: git diff --stat) ---"
