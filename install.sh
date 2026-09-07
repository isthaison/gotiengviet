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
# Restart IBus trong từng phiên desktop đang hoạt động để nhận engine mới
# mà không cần đăng xuất. Không bao giờ làm fail cài đặt; báo cáo từng user.
restart_ibus_sessions() {
    local sock rundir uid user t u dup
    local -a candidates=() users=()
    command -v ibus >/dev/null 2>&1 || {
        echo 'Chưa cài gói ibus nên bỏ qua restart (cài ibus rồi chạy: ibus restart).'
        return 0
    }
    if [[ -n "${SUDO_USER:-}" && "$SUDO_USER" != root ]]; then
        candidates+=("$SUDO_USER")
    else
        # Đăng nhập root trực tiếp: quét mọi session bus đang hoạt động.
        for sock in /run/user/*/bus; do
            [[ -S "$sock" ]] || continue
            uid=$(stat -c %u "$sock" 2>/dev/null || true)
            [[ -n "$uid" && "$uid" != 0 ]] || continue
            user=$(id -nu "$uid" 2>/dev/null || true)
            [[ -n "$user" ]] && candidates+=("$user")
        done
    fi
    if [[ ${#candidates[@]} -gt 0 ]]; then
        for t in "${candidates[@]}"; do
            dup=""
            for u in "${users[@]}"; do [[ "$u" == "$t" ]] && dup=1; done
            [[ -z "$dup" ]] && users+=("$t")
        done
    fi
    if [[ ${#users[@]} -eq 0 ]]; then
        echo 'Không thấy phiên desktop nào để restart IBus (đăng nhập desktop rồi chạy: ibus restart).'
        return 0
    fi
    for u in "${users[@]}"; do
        uid=$(id -u "$u" 2>/dev/null || true)
        [[ -n "$uid" ]] || continue
        rundir="/run/user/$uid"
        [[ -S "$rundir/bus" ]] || continue
        echo "Restart IBus cho $u..."
        if sudo -u "$u" env XDG_RUNTIME_DIR="$rundir" \
                DBUS_SESSION_BUS_ADDRESS="unix:path=$rundir/bus" \
                ibus restart >/dev/null 2>&1 \
            && sudo -u "$u" env XDG_RUNTIME_DIR="$rundir" \
                DBUS_SESSION_BUS_ADDRESS="unix:path=$rundir/bus" \
                ibus list-engine 2>/dev/null | grep -q gotiengviet; then
            echo "IBus đã nhận engine GoTiengViet cho $u."
        else
            echo "Chưa xong cho $u (thử đăng xuất/đăng nhập lại, hoặc chạy tay: ibus restart)."
        fi
    done
    return 0
}
install -Dm755 "$BUILD_DIR/ibus-engine-gotiengviet" "$DESTDIR/usr/libexec/ibus-engine-gotiengviet"
install -Dm755 "$BUILD_DIR/ibus-setup-gotiengviet" "$DESTDIR/usr/libexec/ibus-setup-gotiengviet"
install -Dm755 "$BUILD_DIR/gotiengviet-demo" "$DESTDIR/usr/bin/gotiengviet-demo"
install -Dm644 ibus/gotiengviet.xml "$DESTDIR/usr/share/ibus/component/gotiengviet.xml"
if [[ -f ibus/gotiengviet.metainfo.xml ]]; then
    install -Dm644 ibus/gotiengviet.metainfo.xml "$DESTDIR/usr/share/metainfo/gotiengviet.metainfo.xml"
fi
install -Dm644 ibus/icons/gotiengviet.svg "$DESTDIR/usr/share/icons/hicolor/scalable/apps/gotiengviet.svg"
install -Dm644 ibus/icons/gotiengviet.svg "$DESTDIR/usr/share/gotiengviet/icons/gotiengviet.svg"
install -Dm644 data/macros.txt "$DESTDIR/usr/share/gotiengviet/macros.txt"
install -Dm644 data/emojis.txt "$DESTDIR/usr/share/gotiengviet/emojis.txt"
install -Dm644 data/config "$DESTDIR/usr/share/gotiengviet/config"
install -Dm644 data/ai.conf "$DESTDIR/usr/share/gotiengviet/ai.conf"
install -Dm644 data/assistant.conf "$DESTDIR/usr/share/gotiengviet/assistant.conf"
install -Dm644 data/prompts.conf "$DESTDIR/usr/share/gotiengviet/prompts.conf"
install -Dm644 data/learned-corrections.txt "$DESTDIR/usr/share/gotiengviet/learned-corrections.txt"
install -Dm644 data/learned-words.txt "$DESTDIR/usr/share/gotiengviet/learned-words.txt"
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
    restart_ibus_sessions
    echo 'Đã cài. Mở GoTiengViet rồi chọn bằng Super+Space.'
else
    echo "Đã tạo cây cài đặt: $DESTDIR"
fi
