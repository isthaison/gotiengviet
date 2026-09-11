# Bản Windows

Native Windows 10/11 (x64). Gõ bằng **TSF Text Service** (`gtv_tsf.dll`, composition chuẩn Windows — đúng trong mọi ô nhập kể cả autocomplete/khó tính); app khay hệ thống lo cấu hình, gợi ý AI, cập nhật. Không còn hook bàn phím.

- Sau khi cài: đăng xuất/đăng nhập lại rồi chọn GoTiengViet bằng `Win+Space`.
- Nếu chưa thấy bàn phím GoTiengViet: vào **Settings → Time & Language → Language** → chọn `English` (hoặc `Tiếng Việt`) → **Options** → **Add a keyboard** → chọn **GoTiengViet**. Không thấy nữa thì đăng xuất/đăng nhập lại một lần.
- Nút `GoTV` trên language bar click để chuyển Việt/Anh (đỏ `V` / xám `E`); menu tray **Bật gõ tiếng Việt [V]** làm việc tương tự (đồng bộ qua file cấu hình).
- Telex/VNI, chuẩn dấu, chính tả đổi trong Bảng điều khiển/menu tray, TSF nhận ngay khi chuyển ô nhập (đọc lại cấu hình mỗi lần kích hoạt).

## Cài đặt và cập nhật

- Mỗi release GitHub đính kèm `gotiengviet-<version>-x64-setup.exe` (Inno Setup, per-user, không cần admin, gỡ sạch qua Add/Remove Programs, giữ nguyên cấu hình).
- App tự kiểm tra release mới nhất mỗi ngày; menu tray **Kiểm tra cập nhật...** để kiểm tra tay. Có bản mới thì hiện balloon — **click vào balloon** để tải và chạy bộ cài silent, app tự thoát để thay file. So sánh semver, chỉ cài đúng asset `gotiengviet-<version>-x64-setup.exe`.
- Chẩn đoán: `%APPDATA%\gotiengviet\update.log` (mọi bước check/tải/cài của updater).

## Dùng hằng ngày

- **Menu chuột phải**: Bật gõ tiếng Việt, Bảng điều khiển, Telex/VNI, chính tả, chuẩn dấu, tự khởi động, Kiểm tra cập nhật, Thoát.
- **Bảng điều khiển**: kiểu gõ, chuẩn dấu, chính tả, tự khởi động, cụm AI (Ollama, model, URL) — lưu ở `%APPDATA%\gotiengviet\` như bản Linux.
- **Gợi ý AI dạng balloon**: TSF báo mỗi từ vừa commit về app khay; từ sai được hỏi Ollama nền (`"sai" co the ban muon go "dung"?`, chống spam 10 giây). Cần Ollama + `curl` (có sẵn từ Windows 10). **Click balloon** để nhận (chưa gõ tiếp thì thay tại chỗ, rồi thì copy vào clipboard); lựa chọn được học vào `learned-corrections.txt` nên offline vẫn gợi ý.
- Macro/emoji/prompts/config: chung lõi và file `data/` với bản Linux.
- Chữ Việt hiển thị đúng mọi locale: toàn bộ UI dùng Unicode API, dialog đặt chữ lúc chạy.

## Giới hạn đã biết

- Chưa có gợi ý inline trong ô nhập.
- TSF chưa hỗ trợ trường mật khẩu đặc biệt.

## Build và release

```sh
# MSYS2 MinGW-w64:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 make
make -f Makefile.win            # gotiengviet.exe + gtv_tsf.dll, chạy test
make -f Makefile.win setup VERSION=X.Y.Z   # cần Inno Setup (iscc.exe)
```

Cắt release: bump `windows/version.h` + `windows/resource.rc` + `ibus/gotiengviet.xml` cùng số, commit, push tag `vX.Y.Z` — CI build `gotiengviet-X.Y.Z-x64-setup.exe` và đính kèm release để app tự cập nhật.
