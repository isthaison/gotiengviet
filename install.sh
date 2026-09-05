#!/bin/bash
# install.sh - Cài đặt GoTiengViet, chỉ dùng lib hệ thống
set -e
if [ "$EUID" -ne 0 ]; then
  echo "Cần sudo: sudo ./install.sh"
  exit 1
fi
echo "=== GoTiengViet Install ==="
# Build trước nếu chưa có
if [ ! -f /tmp/ibus-engine-gotiengviet ]; then
  echo "Chưa build, chạy ./build.sh trước..."
  sudo -u so95 bash ./build.sh 2>&1 | tail -5
fi
echo "[1/4] Cài binaries..."
install -Dm755 /tmp/ibus-engine-gotiengviet /usr/libexec/ibus-engine-gotiengviet
install -Dm755 /tmp/ibus-setup-gotiengviet /usr/libexec/ibus-setup-gotiengviet
install -Dm755 /tmp/gotiengviet-demo /usr/local/bin/gotiengviet-demo
echo "  -> /usr/libexec/ibus-engine-gotiengviet"
echo "  -> /usr/libexec/ibus-setup-gotiengviet"
echo "  -> /usr/local/bin/gotiengviet-demo"

echo "[2/4] Cài component & icon..."
install -Dm644 ibus/gotiengviet.xml /usr/share/ibus/component/gotiengviet.xml
install -Dm644 ibus/icons/gotiengviet.svg /usr/share/gotiengviet/icons/gotiengviet.svg
install -Dm644 ibus/icons/gotiengviet.svg /usr/share/icons/hicolor/48x48/apps/gotiengviet.svg
install -Dm644 ibus/icons/gotiengviet.svg /usr/share/icons/hicolor/scalable/apps/gotiengviet.svg
install -Dm644 /tmp/gotiengviet.desktop /usr/share/applications/gotiengviet.desktop 2>/dev/null || install -Dm644 <(cat <<'DESKTOP'
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
) /usr/share/applications/gotiengviet.desktop
gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
update-desktop-database /usr/share/applications 2>/dev/null || true
echo "  -> /usr/share/ibus/component/gotiengviet.xml"
echo "  -> /usr/share/gotiengviet/icons/gotiengviet.svg"
echo "  -> /usr/share/applications/gotiengviet.desktop"

echo "[3/4] Cấu hình..."
mkdir -p /home/so95/.config/gotiengviet 2>/dev/null || true
if [ ! -f /home/so95/.config/gotiengviet/config ]; then
  echo -e "[input]\nmethod=telex\nmodern=true\nspellcheck=true\ncharset=unicode" > /home/so95/.config/gotiengviet/config
  chown so95:so95 /home/so95/.config/gotiengviet/config 2>/dev/null || true
  echo "  -> /home/so95/.config/gotiengviet/config (telex)"
fi
sudo -u so95 env DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus gsettings set org.gnome.desktop.input-sources sources "[('xkb','us'),('ibus','gotiengviet-telex'),('ibus','gotiengviet-vni')]" 2>/dev/null || true
echo "  -> gsettings input-sources: telex + vni"

echo "[4/4] Restart ibus..."
sudo -u so95 env DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus ibus restart 2>/dev/null || true
sleep 2
echo "  ibus restart done"

echo "=== Cài xong ==="
echo "Mở Settings > Keyboard > Input Sources sẽ thấy Telex/VNI"
echo "Chọn Telex/VNI, bấm Super+Space để đổi, gear để mở GoTiengViet Setup"
ibus list-engine 2>&1 | grep gotiengviet | head -5
