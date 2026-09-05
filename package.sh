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
Depends: ibus, libibus-1.0-5, libglib2.0-0, libgtk-3-0, libgdk-pixbuf2.0-0
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
mkdir -p /tmp/gotiengviet-deb/usr/share/gotiengviet/icons
mkdir -p /tmp/gotiengviet-deb/usr/share/icons/hicolor/48x48/apps
mkdir -p /tmp/gotiengviet-deb/usr/share/icons/hicolor/scalable/apps
mkdir -p /tmp/gotiengviet-deb/usr/share/applications
mkdir -p /tmp/gotiengviet-deb/usr/local/bin
cp /tmp/ibus-engine-gotiengviet /tmp/gotiengviet-deb/usr/libexec/
cp /tmp/ibus-setup-gotiengviet /tmp/gotiengviet-deb/usr/libexec/
cp ibus/gotiengviet.xml /tmp/gotiengviet-deb/usr/share/ibus/component/
cp ibus/icons/gotiengviet.svg /tmp/gotiengviet-deb/usr/share/gotiengviet/icons/
cp ibus/icons/gotiengviet.svg /tmp/gotiengviet-deb/usr/share/icons/hicolor/48x48/apps/gotiengviet.svg
cp ibus/icons/gotiengviet.svg /tmp/gotiengviet-deb/usr/share/icons/hicolor/scalable/apps/gotiengviet.svg
cat > /tmp/gotiengviet-deb/usr/share/applications/gotiengviet.desktop <<DESKTOP
[Desktop Entry]
Name=GoTiengViet
GenericName=Vietnamese Input
Comment=Gõ tiếng Việt Telex/VNI thuần hệ thống - github.com/isthaison/gotiengviet
Exec=/usr/libexec/ibus-setup-gotiengviet
Icon=gotiengviet
Terminal=false
Type=Application
Categories=Utility;Settings;
Keywords=telex;vni;vietnamese;gotiengviet;
StartupNotify=true
DESKTOP
cp /tmp/gotiengviet-demo /tmp/gotiengviet-deb/usr/local/bin/gotiengviet-demo 2>/dev/null || true
cat > /tmp/gotiengviet-deb/DEBIAN/postinst <<'POST'
#!/bin/bash
set -e
gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
update-desktop-database /usr/share/applications 2>/dev/null || true
if [ -f /home/so95/.config/unikey/config ] && [ ! -f /home/so95/.config/gotiengviet/config ]; then
  mkdir -p /home/so95/.config/gotiengviet
  cp /home/so95/.config/unikey/config /home/so95/.config/gotiengviet/config || true
  chown so95:so95 /home/so95/.config/gotiengviet/config 2>/dev/null || true
fi
sudo -u so95 env DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus gsettings set org.gnome.desktop.input-sources sources "[('xkb','us'),('ibus','gotiengviet-telex'),('ibus','gotiengviet-vni')]" 2>/dev/null || true
ibus restart 2>/dev/null || true
echo "GoTiengViet $VERSION installed."
POST
chmod 755 /tmp/gotiengviet-deb/DEBIAN/postinst
cat > /tmp/gotiengviet-deb/DEBIAN/prerm <<'PRERM'
#!/bin/bash
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
  rm -f /usr/share/ibus/component/gotiengviet.xml
  rm -f /usr/share/applications/gotiengviet.desktop
  update-desktop-database /usr/share/applications 2>/dev/null || true
  gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
fi
PRERM
chmod 755 /tmp/gotiengviet-deb/DEBIAN/prerm
dpkg-deb --build /tmp/gotiengviet-deb gotiengviet_${VERSION}_amd64.deb 2>&1 | tail -5
ls -lh gotiengviet_${VERSION}_amd64.deb 2>&1 | awk '{print $9, $5}'
echo "=== Package xong: gotiengviet_${VERSION}_amd64.deb ==="
