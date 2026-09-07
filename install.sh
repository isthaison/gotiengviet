#!/bin/bash
# DESTDIR supports package staging and verification without modifying the system.
set -euo pipefail
cd -- "$(dirname -- "$0")"
: "${BUILD_DIR:=build}"
: "${DESTDIR:=}"
if [[ -z "$DESTDIR" && "$EUID" -ne 0 ]]; then
    echo 'Cần sudo: sudo ./install.sh' >&2
    exit 1
fi
for binary in gotiengviet-assistant ibus-engine-gotiengviet ibus-setup-gotiengviet gotiengviet-demo; do
    if [[ ! -x "$BUILD_DIR/$binary" ]]; then
        echo "Thiếu $BUILD_DIR/$binary. Chạy ./build.sh trước." >&2
        exit 1
    fi
done
install -Dm755 "$BUILD_DIR/ibus-engine-gotiengviet" "$DESTDIR/usr/libexec/ibus-engine-gotiengviet"
install -Dm755 "$BUILD_DIR/ibus-setup-gotiengviet" "$DESTDIR/usr/libexec/ibus-setup-gotiengviet"
install -Dm755 "$BUILD_DIR/gotiengviet-demo" "$DESTDIR/usr/bin/gotiengviet-demo"
install -Dm644 ibus/gotiengviet.xml "$DESTDIR/usr/share/ibus/component/gotiengviet.xml"
if [[ -f ibus/gotiengviet.metainfo.xml ]]; then
    install -Dm644 ibus/gotiengviet.metainfo.xml "$DESTDIR/usr/share/metainfo/gotiengviet.metainfo.xml"
fi
install -Dm644 ibus/icons/gotiengviet.svg "$DESTDIR/usr/share/icons/hicolor/scalable/apps/gotiengviet.svg"
install -Dm644 ibus/icons/gotiengviet.svg "$DESTDIR/usr/share/gotiengviet/icons/gotiengviet.svg"
install -Dm755 "$BUILD_DIR/gotiengviet-assistant" "$DESTDIR/usr/bin/gotiengviet-assistant"
for size in 16 22 24 32 48 64 128 256; do
    directory="$DESTDIR/usr/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$directory"
    rsvg-convert -w "$size" -h "$size" ibus/icons/gotiengviet.svg -o "$directory/gotiengviet.png"
done
for size in 48 256; do
    name=gotiengviet.png
    [[ "$size" == 256 ]] && name=gotiengviet-256.png
    rsvg-convert -w "$size" -h "$size" ibus/icons/gotiengviet.svg -o "$DESTDIR/usr/share/gotiengviet/icons/$name"
done
mkdir -p "$DESTDIR/usr/bin" "$DESTDIR/usr/share/applications" "$DESTDIR/etc/xdg/autostart"
ln -sfn /usr/libexec/ibus-setup-gotiengviet "$DESTDIR/usr/bin/gotiengviet"
if [[ -z "$DESTDIR" ]]; then
    mkdir -p /usr/local/bin
    ln -sfn /usr/bin/gotiengviet /usr/local/bin/gotiengviet
    ln -sfn /usr/bin/gotiengviet-demo /usr/local/bin/gotiengviet-demo
fi
cat > "$DESTDIR/usr/share/applications/gotiengviet.desktop" <<'DESKTOP'
[Desktop Entry]
Name=GoTiengViet
GenericName=Vietnamese Input
Comment=Gõ tiếng Việt Telex/VNI
Exec=/usr/bin/gotiengviet
Icon=gotiengviet
Terminal=false
Type=Application
Categories=Utility;
Keywords=unikey;telex;vni;vietnamese;gotiengviet;
StartupNotify=true
StartupWMClass=gotiengviet
DESKTOP
cat > "$DESTDIR/etc/xdg/autostart/gotiengviet.desktop" <<'DESKTOP'
[Desktop Entry]
Name=GoTiengViet
Comment=GoTiengViet indicator
Exec=/usr/bin/gotiengviet --tray
Icon=gotiengviet
Terminal=false
Type=Application
X-GNOME-Autostart-enabled=true
DESKTOP
if [[ -z "$DESTDIR" ]]; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
    update-desktop-database /usr/share/applications || true
    if [[ -n "${SUDO_USER:-}" && "$SUDO_USER" != root ]]; then
        session_uid=$(id -u "$SUDO_USER")
        sudo -u "$SUDO_USER" env XDG_RUNTIME_DIR="/run/user/$session_uid" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$session_uid/bus" ibus restart || true
    fi
    echo 'Đã cài. Mở GoTiengViet rồi chọn bằng Super+Space.'
else
    echo "Đã tạo cây cài đặt: $DESTDIR"
fi
