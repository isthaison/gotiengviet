# GoTiengViet

Bộ gõ tiếng Việt cho Linux (IBus) và Windows. Lõi gõ dùng chung viết bằng **C** (không Go, không CGO, không runtime ngoài); Windows gõ qua Text Service **C++** (TSF) kèm app khay hệ thống.

- Một thuật toán chung cho Telex/VNI, kiểu đặt dấu hiện đại/truyền thống.
- Chính tả ngoại tuyến theo quy tắc âm tiết, gạch đỏ ngay khi gõ.
- Gợi ý Ollama (tùy chọn): debounce 350 ms, hủy khi gõ tiếp, rớt mạng vẫn gợi ý từ dữ liệu đã học.
- Macro/emoji từ file text, người dùng tự thêm được.
- Windows: hook + khay hệ thống, bộ cài theo version, tự cập nhật theo release GitHub.

Tài liệu chi tiết: [Cấu hình](docs/config.md) · [Bản Windows](docs/windows.md) · [Phát triển](docs/dev.md).

## Bắt đầu nhanh

**Linux** (Ubuntu/GNOME):

```sh
sudo apt install build-essential pkg-config libibus-1.0-dev libgtk-3-dev \
  libayatana-appindicator3-dev librsvg2-bin
./build.sh
sudo ./install.sh
```

Rồi vào **Settings → Keyboard → Input Sources** thêm **GoTiengViet**, chọn bằng `Super+Space`.

**Windows 10/11**: tải `gotiengviet-<version>-x64-setup.exe` ở trang Releases và chạy (cài per-user, không cần admin). Chi tiết xem [Bản Windows](docs/windows.md).

Kiểm tra nhanh: `build/gotiengviet-demo --transform telex duocjwd` phải ra `được`.

## Quy tắc gõ

Telex và VNI chỉ khác **bảng ánh xạ phím**:

| Thao tác | Telex | VNI |
|---|---|---|
| Sắc, huyền, hỏi, ngã, nặng | `s f r x j` | `1 2 3 4 5` |
| Mũ | `a e o` trên nguyên âm tương ứng | `6` |
| Móc/trăng | `w` | `7` / `8` |
| Gạch chữ d | `d` | `9` |
| Xóa dấu thanh | `z` | `0` |

| Chuỗi phím | Kết quả |
|---|---|
| `bawst`, `bawts`, `batsw` | `bắt` |
| `duocjwd`, `duocwjd`, `dduocjw` | `được` |
| `duoc579`, `duoc795` (VNI) | `được` |
| `ass` / `a11` | `as` / `a1` |
| `uoww` / `uo77` | `uow` / `uo7` |

**Chủ đích:** `test` ra `tét` (không khôi phục tiếng Anh) — muốn chữ thô thì gõ lặp (`tesst → test`) hoặc tạm chuyển English. `w` không nguyên âm thêm `ư` (`ww → w` hoàn tác). Kiểu `modern` (mặc định) đặt dấu ở nguyên âm thứ hai của `oa/oe/uy` (`hòa`).

Linux: click sang chỗ khác khi đang gõ dở không commit bừa — quay về đúng ô nhập thì gõ tiếp, sang ô khác thì bỏ.

## Lệnh thường dùng

```sh
build/gotiengviet-demo --transform telex duocjwd   # thử thuật toán
build/gotiengviet-demo --suggest kông              # cần Ollama
ibus-setup-gotiengviet            # GUI (tự bật tray nền)
ibus-setup-gotiengviet --tray     # chỉ indicator
ibus-setup-gotiengviet --cli      # terminal
```

## Xử lý sự cố

- **Không thấy GoTiengViet trong Input Sources:** `ibus restart`, đăng xuất/đăng nhập lại, kiểm tra `/usr/share/ibus/component/gotiengviet.xml`.
- **Gõ không ra dấu:** đang ở engine `gotiengviet`? Kiểm tra `Super+Space` và `method` trong `~/.config/gotiengviet/config`.
- **Crash:** xem `~/.cache/gotiengviet/crash.log`, báo kèm phiên bản và log liên quan.
- Windows: xem [Bản Windows](docs/windows.md) (vị trí log: `%APPDATA%\gotiengviet\update.log`).

Giấy phép: [MIT](LICENSE).
