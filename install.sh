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
# Gỡ tray riêng (đã gộp vào 1 app duy nhất, cờ --tray)
rm -f /usr/local/bin/gotiengviet-tray /usr/libexec/gotiengviet-tray /usr/local/bin/gotiengviet-tray-start
rm -f /etc/xdg/autostart/gotiengviet-tray.desktop /usr/share/applications/gotiengviet-tray.desktop
echo "  -> /usr/libexec/ibus-engine-gotiengviet"
echo "  -> /usr/libexec/ibus-setup-gotiengviet"
echo "  -> /usr/local/bin/gotiengviet-demo"

echo "[2/4] Cài component & icon..."
install -Dm644 ibus/gotiengviet.xml /usr/share/ibus/component/gotiengviet.xml

# Cài đặt icon đa kích thước chuẩn Freedesktop (PNG cho fixed sizes, SVG cho scalable)
rm -f /usr/share/icons/hicolor/48x48/apps/gotiengviet.svg
for sz in 16 22 24 32 48 64 128 256; do
  mkdir -p "/usr/share/icons/hicolor/${sz}x${sz}/apps"
  rsvg-convert -w $sz -h $sz ibus/icons/gotiengviet.svg -o "/usr/share/icons/hicolor/${sz}x${sz}/apps/gotiengviet.png"
done
install -Dm644 ibus/icons/gotiengviet.svg /usr/share/icons/hicolor/scalable/apps/gotiengviet.svg

mkdir -p /usr/share/gotiengviet/icons
install -Dm644 ibus/icons/gotiengviet.svg /usr/share/gotiengviet/icons/gotiengviet.svg
rsvg-convert -w 48 -h 48 ibus/icons/gotiengviet.svg -o /usr/share/gotiengviet/icons/gotiengviet.png
rsvg-convert -w 256 -h 256 ibus/icons/gotiengviet.svg -o /usr/share/gotiengviet/icons/gotiengviet-256.png

cat > /usr/share/applications/gotiengviet.desktop <<'DESKTOP'
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
rm -f /usr/share/applications/ibus-setup-gotiengviet.desktop

# Launcher: một ứng dụng duy nhất (mở Setup; cờ --tray chỉ hiện indicator)
cat > /usr/local/bin/gotiengviet <<'LAUNCHER'
#!/bin/bash
# GoTiengViet — một ứng dụng duy nhất
exec /usr/libexec/ibus-setup-gotiengviet "$@"
LAUNCHER
chmod +x /usr/local/bin/gotiengviet
gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
update-desktop-database /usr/share/applications 2>/dev/null || true
echo "  -> /usr/share/ibus/component/gotiengviet.xml"
echo "  -> /usr/share/icons/hicolor/{16x16..256x256,scalable}/apps/gotiengviet.*"
echo "  -> /usr/share/applications/gotiengviet.desktop"
echo "  -> /usr/share/applications/ibus-setup-gotiengviet.desktop (symlink)"
# Autostart: cùng 1 app với cờ --tray (KHÔNG tạo file gotiengviet-tray.desktop riêng)
cat > /etc/xdg/autostart/gotiengviet.desktop <<'AUTOSTART'
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
echo "  -> /etc/xdg/autostart/gotiengviet.desktop (Exec gotiengviet --tray)"


echo "[3/4] Cấu hình..."
mkdir -p /home/so95/.config/gotiengviet 2>/dev/null || true
if [ ! -f /home/so95/.config/gotiengviet/config ]; then
  echo -e "[input]\nmethod=telex\nmodern=true\nspellcheck=true\ncharset=unicode" > /home/so95/.config/gotiengviet/config
  chown so95:so95 /home/so95/.config/gotiengviet/config 2>/dev/null || true
  echo "  -> /home/so95/.config/gotiengviet/config (telex)"
fi
sudo -u so95 env DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus gsettings set org.gnome.desktop.input-sources sources "[('xkb','us'),('ibus','gotiengviet')]" 2>/dev/null || true
echo "  -> gsettings input-sources: gotiengviet (đổi Telex/VNI trên indicator của app)"

echo "[4/4] Restart ibus..."
sudo -u so95 env DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus ibus restart 2>/dev/null || true
sleep 2
echo "  ibus restart done"

echo "=== Cài xong ==="
echo "Mở Settings > Keyboard > Input Sources sẽ thấy GoTiengViet duy nhất"
echo "Bấm Super+Space để bật GoTiengViet, đổi Telex/VNI trên indicator statusbar của app"
ibus list-engine 2>&1 | grep gotiengviet | head -5
