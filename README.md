# 🇻🇳 GoTiengViet — Bộ Gõ Tiếng Việt Thuần Hệ Thống Cho Linux

<p align="center">
  <img src="ibus/icons/gotiengviet.svg" width="120" height="120" alt="GoTiengViet Logo" />
</p>

<p align="center">
  <strong>Bộ gõ tiếng Việt siêu nhẹ, hiện đại, thuần hệ thống dành cho Linux (Ubuntu, Debian, Fedora, Arch)</strong><br>
  Tương thích hoàn hảo với <strong>GNOME Wayland</strong> & <strong>X11</strong> • Không thư viện ngoài • Hỗ trợ Telex, VNI, Macro, Kiểm tra chính tả & AI Offline.
</p>

<p align="center">
  <a href="https://github.com/isthaison/gotiengviet/releases"><img src="https://img.shields.io/badge/release-v0.1.0-blue.svg?style=flat-square" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green.svg?style=flat-square" alt="License"></a>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Wayland%20%7C%20X11-orange.svg?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/framework-IBus%201.5-purple.svg?style=flat-square" alt="IBus">
  <img src="https://img.shields.io/badge/language-Go%20%7C%20C-00ADD8.svg?style=flat-square" alt="Language">
</p>

---

## ✨ Điểm Nổi Bật

- ⚡ **Thuần Hệ Thống (Zero Bloatware)**: Không cài thêm daemon rườm rà hay dependency bên thứ ba. Chỉ sử dụng `libibus-1.0` và `glib-2.0` có sẵn của hệ điều hành.
- 🖥️ **Tương Thích Tuyệt Đối Wayland & X11**: Hoạt động mượt mà trên GNOME Shell, không giật lag, không delay phím, tự động tích hợp trực tiếp vào danh sách **Input Sources** của hệ thống (`ibus gotiengviet`).
- ✍️ **Đa Dạng Kiểu Gõ**:
  - **Telex**: Hỗ trợ đầy đủ phím tắt (`w` đơn -> `ư`, `uow` -> `ươ`, `z` hủy dấu...).
  - **VNI**: Phím số `1-5` bỏ dấu, `6-9` mũ/râu/trăng, `0` hủy dấu.
  - Chuẩn đặt dấu hiện đại (*Bộ GD&ĐT*: `hoà`, `toán`) hoặc kiểu truyền thống (`hòa`).
- 🔍 **Kiểm Tra Chính Tả & Gợi Ý Chuẩn Ngữ Âm**:
  - Tự động gạch chân đỏ khi gõ sai quy tắc chính tả tiếng Việt (như luật thanh điệu với phụ âm cuối `c, ch, p, t`, âm đầu `k/c/gh/g/ngh/ng`, kẹt phím Telex `hoacw` $\rightarrow$ `hoặc`, `tếst` $\rightarrow$ `tết`).
  - Chọn nhanh từ gợi ý bằng phím số `1..5`, `Tab`, `Enter`, hoặc click chuột.
- 🚀 **Gõ Tắt Macro & Emoji**:
  - Mở rộng từ viết tắt: `vn` $\rightarrow$ `Việt Nam`, `hn` $\rightarrow$ `Hà Nội`, `dc` $\rightarrow$ `được`, `ko` $\rightarrow$ `không`...
  - Emoji tức thì: `:smile:` $\rightarrow$ 😊, `:heart:` $\rightarrow$ ❤️, `:fire:` $\rightarrow$ 🔥, `:check:` $\rightarrow$ ✅...
- 🤖 **Tích Hợp AI Local (Ollama)**:
  - Tùy chọn kết nối tới LLM nội bộ (ví dụ: `qwen2:0.5b`) để sửa lỗi chính tả ngữ cảnh nâng cao, hoàn toàn offline và riêng tư.
- 🎯 **Trải Nghiệm Đồng Nhất (Single App)**:
  - Khay hệ thống (System Tray Indicator) gọn gàng, chuyển đổi nhanh Telex/VNI bằng 1 click.
  - Bảng điều khiển cấu hình trực quan viết bằng GTK+ 3.

---

## 📦 Cài Đặt Nhanh

### Cách 1: Tải và cài đặt gói `.deb` (Khuyên Dùng cho Ubuntu/Debian)

Tải file `.deb` mới nhất từ mục [Releases](https://github.com/isthaison/gotiengviet/releases) và cài đặt:

```bash
sudo dpkg -i gotiengviet_0.1.0-10_amd64.deb
```

### Cách 2: Cài đặt từ mã nguồn

Chỉ cần cài đặt các gói dev cơ bản có sẵn trong kho `apt`:

```bash
# 1. Cài thư viện hệ thống
sudo apt update
sudo apt install -y git golang gcc pkg-config libibus-1.0-dev libgtk-3-dev libayatana-appindicator3-dev

# 2. Clone repo và cài đặt
git clone https://github.com/isthaison/gotiengviet.git
cd gotiengviet
./build.sh
sudo bash ./install.sh
```

Sau khi cài đặt xong, mở **Settings > Keyboard > Input Sources** hoặc ấn `Super + Space` để bắt đầu gõ tiếng Việt!

---

## ⌨️ Bảng Quy Tắc Gõ

### Kiểu gõ Telex
| Phím gõ | Kết quả | Mô tả |
|---|---|---|
| `s`, `f`, `r`, `x`, `j` | `á`, `à`, `ả`, `ã`, `ạ` | Sắc, huyền, hỏi, ngã, nặng |
| `aa`, `aw`, `ee`, `oo`, `ow`, `uw` | `â`, `ă`, `ê`, `ô`, `ơ`, `ư` | Dấu mũ, dấu á, dấu móc |
| `dd` | `đ` | Chữ đ |
| `w` | `ư` (đầu từ) / râu | Gõ nhanh âm ư |
| `uow` | `ươ` | Tổ hợp nhanh ươ |
| `z` | Xoá dấu thanh | Ví dụ: `toasz` $\rightarrow$ `toa` |

### Kiểu gõ VNI
| Phím gõ | Kết quả | Mô tả |
|---|---|---|
| `1`, `2`, `3`, `4`, `5` | `á`, `à`, `ả`, `ã`, `ạ` | Sắc, huyền, hỏi, ngã, nặng |
| `6`, `7`, `8`, `9` | `â/ê/ô`, `ơ/ư`, `ă`, `đ` | Mũ, móc, á, gạch đ |
| `0` | Xoá dấu thanh | Xoá dấu đã gõ |

---

## 🛠️ Cấu Trúc Dự Án

```
gotiengviet/
├── cmd/
│   ├── demo/                # CLI demo không cần môi trường đồ hoạ
│   ├── gotiengviet-ibus/    # CGO bridge kiểm thử libibus
│   └── setup/               # Ứng dụng khay hệ thống & Bảng cấu hình (Go + GTK3 CGO)
├── engine/                  # Lõi bộ gõ thuần Go stdlib
│   ├── charset.go           # Bảng mã Unicode tiếng Việt dựng sẵn
│   ├── phonology.go         # Quy tắc cấu trúc âm tiết & đặt dấu chuẩn
│   ├── telex.go             # Bộ máy biến đổi Telex
│   ├── vni.go               # Bộ máy biến đổi VNI
│   ├── spell.go             # Thuật toán kiểm tra chính tả & Levenshtein
│   ├── macro.go             # Lõi mở rộng gõ tắt & emoji
│   └── ai.go                # Tích hợp AI Local Ollama
├── ibus/
│   ├── engine.c             # IBus Engine C thuần siêu tốc (0ms latency)
│   ├── gotiengviet.xml      # Component descriptor cho IBus
│   └── icons/               # Bộ icon đa độ phân giải (16x16 -> 256x256, SVG)
├── build.sh                 # Kịch bản biên dịch toàn bộ dự án
├── install.sh               # Kịch bản cài đặt tự động vào hệ điều hành
└── package.sh               # Kịch bản đóng gói file .deb chuẩn Debian
```

---

## 🤝 Đóng Góp (Contributing)

Mọi đóng góp, báo lỗi (issues) và đề xuất tính năng (pull requests) đều rất được hoan nghênh!
1. Fork dự án
2. Tạo nhánh tính năng (`git checkout -b feature/tinh-nang-moi`)
3. Commit thay đổi (`git commit -m 'Thêm tính năng mới'`)
4. Push nhánh lên GitHub (`git push origin feature/tinh-nang-moi`)
5. Tạo Pull Request mới

---

## 📄 Bản Quyền (License)

Dự án được phát hành theo giấy phép mã nguồn mở **[MIT License](LICENSE)**. Tự do sử dụng, chỉnh sửa và phân phối.
