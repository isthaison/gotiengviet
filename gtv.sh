#!/bin/bash
# GoTiengViet developer entry point: build, test, install, package, bump.
# Single script replacing the old build.sh/clean.sh/install.sh/package.sh/bump-version.sh.
#
# Usage: ./gtv.sh <command> [args]
#   build [make-args...]  build + test (make build test)
#   test [make-args...]   run test suites (make test)
#   vet                   strict build + test (make vet)
#   clean                 remove the build tree only
#   install               install to system (needs sudo unless DESTDIR is set)
#   package [VERSION]     build .deb (default from Makefile VERSION)
#   bump X.Y.Z ["notes"]  bump version everywhere (single writer)
#   help                  this help
set -euo pipefail
cd -- "$(dirname -- "$0")"

PROG='./gtv.sh'

usage() {
    cat <<USAGE
Usage: $PROG <command> [args]
  build [make-args...]  build + test (make build test)
  test [make-args...]   run test suites (make test)
  vet                   strict build + test (make vet)
  clean                 remove the build tree only
  install               install to system (needs sudo unless DESTDIR is set)
  package [VERSION]     build .deb (default from Makefile VERSION)
  bump X.Y.Z ["notes"]  bump version everywhere (single writer)
  help                  this help
USAGE
}

cmd_help() {
    usage
}

cmd_build() {
    # Build and test the native C applications without starting a desktop session.
    make build test "$@"
}

cmd_test() {
    make test "$@"
}

cmd_vet() {
    make vet
}

cmd_clean() {
    # Only remove the default build tree; keep packaged releases and system files.
    rm -rf -- build
}

# DESTDIR supports package staging and verification without modifying the system.
cmd_install() {
    : "${BUILD_DIR:=build}"
    : "${DESTDIR:=}"
    if [[ -z "$DESTDIR" && "$EUID" -ne 0 ]]; then
        echo "Cần sudo: sudo $PROG install" >&2
        exit 1
    fi
    for binary in ibus-engine-gotiengviet ibus-setup-gotiengviet gotiengviet-demo; do
        if [[ ! -x "$BUILD_DIR/$binary" ]]; then
            echo "Thiếu $BUILD_DIR/$binary. Chạy $PROG build trước." >&2
            exit 1
        fi
    done
    install -Dm755 "$BUILD_DIR/ibus-engine-gotiengviet" "$DESTDIR/usr/libexec/ibus-engine-gotiengviet"
    install -Dm755 "$BUILD_DIR/ibus-setup-gotiengviet" "$DESTDIR/usr/libexec/ibus-setup-gotiengviet"
    install -Dm755 "$BUILD_DIR/gotiengviet-demo" "$DESTDIR/usr/bin/gotiengviet-demo"
    install -Dm644 linux/ibus/gotiengviet.xml "$DESTDIR/usr/share/ibus/component/gotiengviet.xml"
    if [[ -f linux/ibus/gotiengviet.metainfo.xml ]]; then
        install -Dm644 linux/ibus/gotiengviet.metainfo.xml "$DESTDIR/usr/share/metainfo/gotiengviet.metainfo.xml"
    fi
    install -Dm644 linux/ibus/icons/gotiengviet.svg "$DESTDIR/usr/share/icons/hicolor/scalable/apps/gotiengviet.svg"
    install -Dm644 linux/ibus/icons/gotiengviet.svg "$DESTDIR/usr/share/gotiengviet/icons/gotiengviet.svg"
    install -Dm644 data/macros.txt "$DESTDIR/usr/share/gotiengviet/macros.txt"
    install -Dm644 data/emojis.txt "$DESTDIR/usr/share/gotiengviet/emojis.txt"
    install -Dm644 data/config "$DESTDIR/usr/share/gotiengviet/config"
    install -Dm644 data/ai.conf "$DESTDIR/usr/share/gotiengviet/ai.conf"
    install -Dm644 data/prompts.conf "$DESTDIR/usr/share/gotiengviet/prompts.conf"
    install -Dm644 data/learned-corrections.txt "$DESTDIR/usr/share/gotiengviet/learned-corrections.txt"
    install -Dm644 data/learned-words.txt "$DESTDIR/usr/share/gotiengviet/learned-words.txt"
    for size in 16 22 24 32 48 64 128 256; do
        directory="$DESTDIR/usr/share/icons/hicolor/${size}x${size}/apps"
        mkdir -p "$directory"
        rsvg-convert -w "$size" -h "$size" linux/ibus/icons/gotiengviet.svg -o "$directory/gotiengviet.png"
    done
    for size in 48 256; do
        name=gotiengviet.png
        [[ "$size" == 256 ]] && name=gotiengviet-256.png
        rsvg-convert -w "$size" -h "$size" linux/ibus/icons/gotiengviet.svg -o "$DESTDIR/usr/share/gotiengviet/icons/$name"
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
}

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

cmd_package() {
    version=${1:-0.8.1-1}
    architecture=$(dpkg --print-architecture)
    dpkg --validate-version "$version"
    make build test
    staging=$(mktemp -d /tmp/gotiengviet-package.XXXXXX)
    chmod 755 "$staging"
    trap 'rm -rf -- "$staging"' EXIT
    DESTDIR="$staging" cmd_install
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
}

# Single writer for every place that carries the release version, so a
# release can no longer ship with half-bumped files. Idempotent.
cmd_bump() {
    local ver="${1:?Usage: $PROG bump X.Y.Z [\"release notes\"]}"
    [[ "$ver" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "bump: version must be X.Y.Z, got '$ver'" >&2; exit 1; }
    local notes="${2:-Phát hành $ver.}"
    local today csv old
    today=$(date +%F)
    csv="$(echo "$ver" | tr . ,),0"    # 0,7,0,0 for VERSIONINFO
    old=$(sed -n 's/^#define GTV_VERSION "\(.*\)"/\1/p' windows/version.h)
    [[ -n "$old" ]] || { echo "bump: cannot read current version from windows/version.h" >&2; exit 1; }

    printf '%s\n' "$ver" > VERSION
    sed -i "s/#define GTV_VERSION \"[^\"]*\"/#define GTV_VERSION \"$ver\"/" windows/version.h
    sed -i "s/Tags like v[0-9.]*/Tags like v$ver/" windows/version.h
    sed -i -E "s/^FILEVERSION[ \t]+[0-9,]+/FILEVERSION     $csv/" windows/resource.rc
    sed -i -E "s/^PRODUCTVERSION[ \t]+[0-9,]+/PRODUCTVERSION  $csv/" windows/resource.rc
    sed -i -E "s/(VALUE \"(File|Product)Version\", \")[0-9.]+/\\1$ver.0/" windows/resource.rc
    sed -i "s|<version>[^<]*</version>|<version>$ver</version>|" linux/ibus/gotiengviet.xml
    sed -i 's/"[0-9][0-9.]*","GPL"/"'"$ver"'","GPL"/' linux/ibus/engine.c

    if grep -q "<release version=\"$ver\"" linux/ibus/gotiengviet.metainfo.xml; then
        echo "(metainfo already has $ver, keeping existing entry)"
    else
        local esc_notes
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
            1' linux/ibus/gotiengviet.metainfo.xml > linux/ibus/gotiengviet.metainfo.xml.new
        mv linux/ibus/gotiengviet.metainfo.xml.new linux/ibus/gotiengviet.metainfo.xml
    fi

    sed -i "s/^VERSION ?= .*/VERSION ?= \$(GTV_VERSION)/" Makefile.win
    sed -i "s/^VERSION ?= .*/VERSION ?= \$(GTV_VERSION)-1/" Makefile
    sed -i 's/^\([ \t]*\)version=${1:-.*}/\1version=${1:-'"$ver"'-1}/' "$0"
    sed -i "s/#define AppVersion \".*\"/#define AppVersion \"$ver\"/" windows/installer.iss
    sed -i "s/#define AppVerNum \".*\"/#define AppVerNum \"$ver.0\"/" windows/installer.iss
    sed -i "/CFBundleShortVersionString/,+1 s|<string>[^<]*</string>|<string>$ver</string>|" macos/Info.plist
    sed -i "/CFBundleVersion/,+1 s|<string>[^<]*</string>|<string>$ver</string>|" macos/Info.plist
    # installer.iss carries Vietnamese shortcut names: keep UTF-8 with BOM.
    if [ "$(head -c 3 windows/installer.iss)" != "$(printf '\xef\xbb\xbf')" ]; then
        printf '\xef\xbb\xbf' | cat - windows/installer.iss > windows/installer.iss.new
        mv windows/installer.iss.new windows/installer.iss
        echo "(restored UTF-8 BOM in windows/installer.iss)"
    fi
    sed -i 's/\[ -n "\$VER" \] || VER="[^"]*"/[ -n "$VER" ] || VER="'"$ver"'"/' .github/workflows/build-windows.yml

    echo "--- bumped $old -> $ver ---"
    echo "--- leftover $old refs (history/tests/examples are fine) ---"
    grep -rnF "$old" --include="*.c" --include="*.h" --include="*.xml" --include="*.yml" \
        --include="*.iss" --include="Makefile*" --include="*.sh" . 2>/dev/null \
        | grep -v "^\./\.git" | grep -v "tests/" | grep -v "metainfo" || true
    echo "--- done (review with: git diff --stat) ---"
}

command="${1:-help}"
case "$command" in
    build)   shift; cmd_build "$@" ;;
    test)    shift; cmd_test "$@" ;;
    vet)     cmd_vet ;;
    clean)   cmd_clean ;;
    install) cmd_install ;;
    package) shift; cmd_package "${1:-}" ;;
    bump)    shift; cmd_bump "${1:?Usage: $PROG bump X.Y.Z [\"notes\"]}" "${2:-}" ;;
    help|--help|-h) cmd_help ;;
    *) echo "Unknown command '$command'. Usage: $PROG <build|test|vet|clean|install|package|bump|help>" >&2; exit 1 ;;
esac
