# GoTiengViet — Bộ gõ tiếng Việt Telex/VNI cho Linux, Windows & macOS

[![Build Linux](https://github.com/isthaison/gotiengviet/actions/workflows/build-linux.yml/badge.svg)](https://github.com/isthaison/gotiengviet/actions/workflows/build-linux.yml)
[![Build Windows](https://github.com/isthaison/gotiengviet/actions/workflows/build-windows.yml/badge.svg)](https://github.com/isthaison/gotiengviet/actions/workflows/build-windows.yml)
[![Build macOS](https://github.com/isthaison/gotiengviet/actions/workflows/build-macos.yml/badge.svg)](https://github.com/isthaison/gotiengviet/actions/workflows/build-macos.yml)
[![Release](https://img.shields.io/github/v/release/isthaison/gotiengviet)](https://github.com/isthaison/gotiengviet/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
🌐 Trang chủ: **<https://isthaison.github.io/gotiengviet/>**

Bộ gõ tiếng Việt mã nguồn mở: một **lõi gõ duy nhất viết bằng C** (không Go, không CGO, không runtime ngoài),
chạy trên cả ba hệ điều hành qua adapter mỏng của từng nền tảng —
[IBus](https://github.com/ibus/ibus) trên Linux, [TSF Text Service](https://learn.microsoft.com/en-us/windows/win32/tsf/text-services-framework) trên Windows,
[InputMethodKit](https://developer.apple.com/documentation/inputmethodkit) trên macOS.

> **English:** GoTiengViet is a free, open-source **Vietnamese input method (Telex & VNI)** for
> Linux (IBus), Windows 10/11 (TSF) and macOS (InputMethodKit) — a lightweight Unikey/EVKey
> alternative with offline spellcheck, text macros/emoji and optional local AI suggestions via Ollama.

```text
$ build/gotiengviet-demo --transform telex duocjwd
được
```

## Tính năng

- **Một thuật toán chung** cho Telex và VNI — chỉ khác bảng ánh xạ phím; kiểu đặt dấu hiện đại (`hòa`) hoặc truyền thống (`hoà`).
- **Chính tả ngoại tuyến**: gạch đỏ ngay khi gõ theo quy tắc âm tiết (âm đầu–vần–phụ âm cuối–thanh), không cần mạng.
- **Gợi ý AI cục bộ (tùy chọn)**: Ollama trên máy bạn — debounce 350 ms, hủy khi gõ tiếp, rớt mạng vẫn gợi ý từ dữ liệu đã học.
- **Macro/emoji bằng file text**: tự thêm `cty=Công ty TNHH`, `:smile:`→😊, `(y)`→👍.
- **Nhẹ, riêng tư**: lõi C thuần + glib; không keylogger đám mây, AI chạy local.
- **Tự cập nhật**: kiểm tra release GitHub mỗi ngày, cài đè giữ nguyên cấu hình.

## Trạng thái nền tảng

| Nền tảng | Cách gõ | Trạng thái |
|---|---|---|
| Linux (Ubuntu/GNOME) | IBus engine + app khay (indicator) | ✅ Ổn định — `.deb` ở [Releases](https://github.com/isthaison/gotiengviet/releases/latest) |
| Windows 10/11 | TSF Text Service + app khay | ✅ Ổn định — `gotiengviet-*-x64-setup.exe` ở [Releases](https://github.com/isthaison/gotiengviet/releases/latest) |
| macOS | InputMethodKit `.app` | 🧪 Preview (đang hoàn thiện) — xem [docs/macos.md](docs/macos.md) |

## Bắt đầu nhanh

**Linux** (Ubuntu/GNOME):

```sh
sudo apt install build-essential pkg-config libibus-1.0-dev libgtk-3-dev \
  libayatana-appindicator3-dev librsvg2-bin
./gtv.sh build
sudo ./gtv.sh install
```

Rồi vào **Settings → Keyboard → Input Sources** thêm **GoTiengViet (Telex)** và/hoặc **GoTiengViet (VNI)**,
chuyển bằng `Super+Space`. Chi tiết: [docs/dev.md](docs/dev.md).

**Windows 10/11**: tải `gotiengviet-<version>-x64-setup.exe` ở trang
[Releases](https://github.com/isthaison/gotiengviet/releases/latest) và chạy
(cài per-user, không cần admin) → đăng xuất/đăng nhập lại → chọn **GoTV** bằng `Win+Space`.
Chi tiết: [docs/windows.md](docs/windows.md).

**macOS**: tải `gotiengviet-<version>-macos.zip` ở trang Releases (bản preview),
giải nén vào `~/Library/Input Methods`, đăng xuất/đăng nhập lại. Chi tiết: [docs/macos.md](docs/macos.md).

## Quy tắc gõ (Telex/VNI)

| Thao tác | Telex | VNI |
|---|---|---|
| Sắc, huyền, hỏi, ngã, nặng | `s f r x j` | `1 2 3 4 5` |
| Mũ (`â ê ô`) | lặp nguyên âm (`aa`) | `6` |
| Móc/trăng (`ơ ư`, `ă`) | `w` | `7` / `8` |
| Gạch chữ `đ` | `dd` | `9` |
| Xóa dấu thanh | `z` | `0` |
| Chữ thô (không dấu) | gõ lặp (`ass` → `as`) | gõ lặp tương tự |

Ví dụ: `bawst` → `bắt`, `duocjwd` → `được`, `hoaf` → `hoà`/`hòa` (tùy kiểu đặt dấu).

## So với Unikey / EVKey / OpenKey

|  | GoTiengViet | Unikey | EVKey/OpenKey |
|---|---|---|---|
| Telex + VNI | ✅ | ✅ | ✅ |
| Linux (IBus) / Windows / macOS | ✅ cả 3 | Win + Linux | Win + macOS |
| Chính tả gạch đỏ khi gõ | ✅ | ❌ | ❌/✅ |
| Gợi ý AI local (Ollama) | ✅ | ❌ | ❌ |
| Mã nguồn mở | ✅ MIT | ✅ | ✅ |

## Tài liệu

- [Cấu hình & macro/emoji/AI](docs/config.md) · [Bản Windows](docs/windows.md) ·
  [Bản macOS](docs/macos.md) · [Phát triển & đóng gói](docs/dev.md) ·
  [Trang chủ](https://isthaison.github.io/gotiengviet/)

## Lệnh thường dùng

```sh
build/gotiengviet-demo --transform telex duocjwd   # thử thuật toán
build/gotiengviet-demo --suggest kông              # cần Ollama
ibus-setup-gotiengviet            # GUI Linux (tự bật tray nền)
ibus-setup-gotiengviet --tray     # chỉ indicator
ibus-setup-gotiengviet --cli      # terminal
./gtv.sh build | test | vet | install | uninstall | package | bump | help
# Windows (MSYS2 MinGW hoặc cmd):  gtv build | test | install | uninstall | package
# macOS: ./gtv.sh build | test | install | uninstall | package  (app vào ~/Library/Input Methods)
```

## Xử lý sự cố

- **Không thấy GoTiengViet trong Input Sources (Linux):** `ibus restart`, đăng xuất/đăng nhập lại,
  kiểm tra `/usr/share/ibus/component/gotiengviet.xml`.
- **Gõ không ra dấu (Linux):** đang ở engine `gotiengviet`? Kiểm tra `Super+Space` và
  `method` trong `~/.config/gotiengviet/config`.
- **Không thấy GoTV sau khi cài (Windows):** đăng xuất/đăng nhập lại;
  Settings → Time & Language → Language → Options → Add a keyboard → GoTiengViet.
- **Crash (Linux):** xem `~/.cache/gotiengviet/crash.log`, báo kèm phiên bản và log liên quan.
- **Updater (Windows):** xem `%APPDATA%\gotiengviet\update.log`.

## Đóng góp

Mời issue/PR: mở issue mô tả lỗi kèm OS + phiên bản + log, hoặc PR nhỏ, test xanh
(`./gtv.sh test` mọi nền tảng — Windows chạy trong MSYS2 MinGW hoặc `gtv test` từ cmd).
Chuẩn code: `engine/` thuần C đa nền tảng, adapter riêng từng OS trong `linux/`, `windows/`, `macos/`.

> 💡 **Gợi ý topics cho repo** (mục About → ⚙️): `vietnamese` `tieng-viet` `telex` `vni`
> `input-method` `ime` `ibus` `tsf` `unikey` `linux` `windows` `macos` `ollama` `c`

Giấy phép: [MIT](LICENSE).
