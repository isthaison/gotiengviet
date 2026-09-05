# GoTiengViet - Gõ tiếng Việt Telex/VNI cho Linux (thuần hệ thống)

Không cài thêm thư viện ngoài. Chỉ dùng thư viện có sẵn trên Ubuntu: `libibus-1.0` (`1.5.32`), `glib-2.0`, `gio-2.0` và Go stdlib (`unicode`, `bufio`).

## Cấu trúc

```
gotiengviet/
├── go.mod              # module thuần stdlib, không require godbus/dbus
├── engine/             # core gõ - thuần Go stdlib
│   ├── charset.go      # bảng unicode ăâêôơưđ + dấu sắc huyền hỏi ngã nặng
│   ├── phonology.go    # quy tắc đặt dấu (findTonePosition) - modern style hoà
│   ├── transform.go    # applyMark, tryRemove
│   ├── telex.go        # Telex: aa ee oo aw ow uw dd s f r x j z, uow->ươ
│   ├── vni.go          # VNI: 1-5, 6-9, 0
│   └── engine.go       # Engine stateful ProcessKey
├── cmd/demo/main.go    # CLI demo - chỉ dùng stdlib + engine
├── cmd/setup/main.go   # Setup UI Go + gtk+-3.0 (cgo, không dùng gotk3) - thống nhất Go
├── cmd/gotiengviet-ibus/main.go # CGO demo liên kết libibus hệ thống (#cgo pkg-config: ibus-1.0)
└── ibus/
    ├── engine.c        # IBus engine C thuần - chỉ #include <ibus.h>
    ├── gotiengviet.xml      # component IBus (Telex/VNI 2 engine)
    └── archive/        # Python cũ (đã thống nhất sang Go)
```

## Build & Test (không cần mạng)

```bash
go vet ./engine
go run ./cmd/demo
# hoặc build C engine:
gcc -o /tmp/ibus-engine-gotiengviet ibus/engine.c $(pkg-config --cflags --libs ibus-1.0)
```

## Telex

| Gõ | Kết quả | Mô tả |
|---|---|---|
| `as af ar ax aj` | `á à ả ã ạ` | sắc huyền hỏi ngã nặng |
| `aa aw ee oo ow uw` | `â ă ê ô ơ ư` | mũ, trăng, râu |
| `dd` | `đ` | stroke |
| `z` | xóa dấu | `asz->a`, `aaz->a` |
| `w` | `ư` (đầu từ) | `w` đơn -> `ư` |
| `uow` | `ươ` | shortcut |

VNI: `1 2 3 4 5` -> sắc huyền hỏi ngã nặng, `6->âêô`, `7->ơư`, `8->ă`, `9->đ`, `0` xóa.

## IBus (hệ thống)

```bash
sudo apt install libibus-1.0-dev  # có sẵn trên Ubuntu plucky
gcc -o /usr/libexec/ibus-engine-gotiengviet ibus/engine.c $(pkg-config --cflags --libs ibus-1.0)
sudo cp ibus/gotiengviet.xml /usr/share/ibus/component/
ibus restart
gsettings set org.gnome.desktop.input-sources sources "[('xkb','us'),('ibus','gotiengviet')]"
```

Đổi Telex/VNI trong code `engine.ModeTelex` / `ModeVNI` (mặc định Telex, modern-style `hoà`).

## Yêu cầu

- `go 1.24` (stdlib)
- `libibus-1.0-dev`, `libglib2.0-dev` (có sẵn `apt` - không cài PPA/bamboo/gotiengviet package)
- `gcc`, `pkg-config`
