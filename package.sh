#!/bin/bash
# package.sh - Đóng gói .deb, chỉ dùng lib hệ thống
set -e
VERSION=${1:-0.1.0-1}
echo "=== Package gotiengviet $VERSION ==="
./build.sh 2>&1 | tail -10
rm -rf /tmp/gotiengviet-deb
mkdir -p /tmp/gotiengviet-deb/DEBIAN
cat > /tmp/gotiengviet-deb/DEBIAN/control <<CTRL
Package: gotiengviet
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Depends: ibus, libibus-1.0-5, libglib2.0-0, libgtk-3-0, libgdk-pixbuf-2.0-0, libayatana-appindicator3-1
Maintainer: GoTiengViet Project <https://github.com/isthaison/gotiengviet>
Description: GoTiengViet - Gõ tiếng Việt Telex/VNI thuần hệ thống
 Gõ Telex (s f r x j, w z, aa aw dd) và VNI (1-5, 6-9, 0) cho Linux,
 chỉ dùng lib hệ thống (Go stdlib + libibus, glib, gtk).
 Hỗ trợ hasFinal (thuaw->thưa, hoacw->hoăc), u->w vòng, t e s s t->test,
 gõ lại để xóa dấu, crash log ~/.cache/gotiengviet/crash.log.
 Homepage: https://github.com/isthaison/gotiengviet
CTRL
mkdir -p /tmp/gotiengviet-deb/usr/libexec
mkdir -p /tmp/gotiengviet-deb/usr/share/ibus/component
mkdir -p /tmp/gotiengviet-deb/usr/share/ibus/component
mkdir -p /tmp/gotiengviet-deb/usr/share/gotiengviet/icons
mkdir -p /tmp/gotiengviet-deb/usr/share/icons/hicolor/scalable/apps
mkdir -p /tmp/gotiengviet-deb/usr/share/applications
mkdir -p /tmp/gotiengviet-deb/usr/local/bin
mkdir -p /tmp/gotiengviet-deb/etc/xdg/autostart
cp /tmp/ibus-engine-gotiengviet /tmp/gotiengviet-deb/usr/libexec/
cp /tmp/ibus-setup-gotiengviet /tmp/gotiengviet-deb/usr/libexec/
cp ibus/gotiengviet.xml /tmp/gotiengviet-deb/usr/share/ibus/component/

# Cài đặt icon đa kích thước chuẩn Freedesktop
for sz in 16 22 24 32 48 64 128 256; do
  mkdir -p "/tmp/gotiengviet-deb/usr/share/icons/hicolor/${sz}x${sz}/apps"
  rsvg-convert -w $sz -h $sz ibus/icons/gotiengviet.svg -o "/tmp/gotiengviet-deb/usr/share/icons/hicolor/${sz}x${sz}/apps/gotiengviet.png"
done
cp ibus/icons/gotiengviet.svg /tmp/gotiengviet-deb/usr/share/icons/hicolor/scalable/apps/gotiengviet.svg
cp ibus/icons/gotiengviet.svg /tmp/gotiengviet-deb/usr/share/gotiengviet/icons/
rsvg-convert -w 48 -h 48 ibus/icons/gotiengviet.svg -o /tmp/gotiengviet-deb/usr/share/gotiengviet/icons/gotiengviet.png
rsvg-convert -w 256 -h 256 ibus/icons/gotiengviet.svg -o /tmp/gotiengviet-deb/usr/share/gotiengviet/icons/gotiengviet-256.png

cat > /tmp/gotiengviet-deb/usr/share/applications/gotiengviet.desktop <<DESKTOP
[Desktop Entry]
Name=GoTiengViet
GenericName=Vietnamese Input
Comment=Gõ tiếng Việt Telex/VNI thuần hệ thống - github.com/isthaison/gotiengviet
Exec=/usr/local/bin/gotiengviet
Icon=gotiengviet
Terminal=false
Type=Application
Categories=Utility;Settings;InputMethod;
Keywords=unikey;telex;vni;vietnamese;gotiengviet;
StartupNotify=true
StartupWMClass=gotiengviet
DESKTOP
cat > /tmp/gotiengviet-deb/usr/local/bin/gotiengviet <<'LAUNCHER'
#!/bin/bash
# GoTiengViet — một ứng dụng duy nhất
exec /usr/libexec/ibus-setup-gotiengviet "$@"
LAUNCHER
chmod +x /tmp/gotiengviet-deb/usr/local/bin/gotiengviet
cp /tmp/gotiengviet-demo /tmp/gotiengviet-deb/usr/local/bin/gotiengviet-demo 2>/dev/null || true
# Autostart: cùng 1 app với cờ --tray (KHÔNG tạo file gotiengviet-tray.desktop riêng)
cat > /tmp/gotiengviet-deb/etc/xdg/autostart/gotiengviet.desktop <<'AUTOSTART'
[Desktop Entry]
Name=GoTiengViet
Comment=GoTiengViet - Gõ tiếng Việt Telex/VNI (indicator trên statusbar)
Exec=/usr/local/bin/gotiengviet --tray
Icon=gotiengviet
Terminal=false
Type=Application
Categories=Utility;
X-GNOME-Autostart-enabled=true
AUTOSTART
cat > /tmp/gotiengviet-deb/DEBIAN/postinst <<'POST'
#!/bin/bash
set -e
rm -f /usr/share/icons/hicolor/48x48/apps/gotiengviet.svg 2>/dev/null || true
gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
update-desktop-database /usr/share/applications 2>/dev/null || true
# Gỡ tàn dư tray riêng của bản cũ (đã gộp vào 1 app duy nhất)
rm -f /usr/local/bin/gotiengviet-tray /usr/libexec/gotiengviet-tray /usr/local/bin/gotiengviet-tray-start
rm -f /etc/xdg/autostart/gotiengviet-tray.desktop /usr/share/applications/gotiengviet-tray.desktop /usr/share/applications/ibus-setup-gotiengviet.desktop
if [ -f /home/so95/.config/unikey/config ] && [ ! -f /home/so95/.config/gotiengviet/config ]; then
  mkdir -p /home/so95/.config/gotiengviet
  cp /home/so95/.config/unikey/config /home/so95/.config/gotiengviet/config || true
  chown so95:so95 /home/so95/.config/gotiengviet/config 2>/dev/null || true
fi
sudo -u so95 env DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus ibus restart 2>/dev/null || true
echo "GoTiengViet $VERSION installed."
POST
chmod 755 /tmp/gotiengviet-deb/DEBIAN/postinst
cat > /tmp/gotiengviet-deb/DEBIAN/prerm <<'PRERM'
#!/bin/bash
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
  rm -f /usr/share/ibus/component/gotiengviet.xml
  rm -f /usr/share/applications/gotiengviet.desktop
  rm -f /etc/xdg/autostart/gotiengviet.desktop
  # Dọn tàn dư tray riêng của bản cũ
  rm -f /usr/local/bin/gotiengviet-tray /usr/libexec/gotiengviet-tray /usr/local/bin/gotiengviet-tray-start
  rm -f /etc/xdg/autostart/gotiengviet-tray.desktop /usr/share/applications/gotiengviet-tray.desktop
  pkill -f "gotiengviet-tray" 2>/dev/null || true
  update-desktop-database /usr/share/applications 2>/dev/null || true
  gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
fi
PRERM
chmod 755 /tmp/gotiengviet-deb/DEBIAN/prerm
chmod 755 /tmp/gotiengviet-deb/DEBIAN
dpkg-deb --root-owner-group --build /tmp/gotiengviet-deb gotiengviet_${VERSION}_amd64.deb 2>&1 | tail -5
ls -lh gotiengviet_${VERSION}_amd64.deb 2>&1 | awk '{print $9, $5}'
echo "=== Package xong: gotiengviet_${VERSION}_amd64.deb ==="
