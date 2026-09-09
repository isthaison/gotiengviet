#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
version=${1:-0.5.1-1}
architecture=$(dpkg --print-architecture)
dpkg --validate-version "$version"
./build.sh
staging=$(mktemp -d /tmp/gotiengviet-package.XXXXXX)
chmod 755 "$staging"
trap 'rm -rf -- "$staging"' EXIT
DESTDIR="$staging" ./install.sh
mkdir -p "$staging/DEBIAN"
cat > "$staging/DEBIAN/control" <<CONTROL
Package: gotiengviet
Version: $version
Section: utils
Priority: optional
Architecture: $architecture
Depends: ibus, libibus-1.0-5, libglib2.0-0, libgtk-3-0, libgdk-pixbuf-2.0-0, libayatana-appindicator3-1
Recommends: curl, libnotify-bin
Maintainer: GoTiengViet Project <https://github.com/isthaison/gotiengviet>
Homepage: https://github.com/isthaison/gotiengviet
Description: Vietnamese Telex/VNI input method in native C
 Shared C input engine, GTK settings, indicator, spelling and macro support.
 Optional local Ollama suggestions use curl; no Go runtime is required.
CONTROL
cat > "$staging/DEBIAN/postinst" <<'POST'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null; then
    update-desktop-database /usr/share/applications || true
fi
mkdir -p /usr/local/bin 2>/dev/null || true
ln -sfn /usr/bin/gotiengviet /usr/local/bin/gotiengviet 2>/dev/null || true
ln -sfn /usr/bin/gotiengviet-demo /usr/local/bin/gotiengviet-demo 2>/dev/null || true
POST
chmod -R u=rwX,go=rX "$staging"
chmod 755 "$staging/DEBIAN"
chmod 755 "$staging/DEBIAN/postinst"
output="gotiengviet_${version}_${architecture}.deb"
dpkg-deb --root-owner-group --build "$staging" "$output"
echo "Đã đóng gói: $output"
