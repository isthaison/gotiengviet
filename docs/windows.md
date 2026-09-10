# Bản Windows

Native Windows 10/11 (x64). Hai chế độ gõ, **không bao giờ chạy cùng lúc**:

- **Hook** (mặc định): `WH_KEYBOARD_LL` + khay hệ thống. Ổn định, đầy đủ tính năng.
- **TSF Text Service** (thử nghiệm): `gtv_tsf.dll`, gõ bằng composition chuẩn Windows — đúng trong ô autocomplete/khó tính mà hook giả lập phím không xử lý nổi. Bật trong menu tray **TSF Text Service (thử nghiệm)**, rồi đăng xuất/đăng nhập lại và chọn GoTiengViet bằng `Win+Space`.

## Cài đặt và cập nhật

- Mỗi release GitHub đính kèm `gotiengviet-<version>-x64-setup.exe` (Inno Setup, per-user, không cần admin, gỡ sạch qua Add/Remove Programs, giữ nguyên cấu hình).
- App tự kiểm tra release mới nhất mỗi ngày; menu tray **Kiểm tra cập nhật...** để kiểm tra tay. Có bản mới thì hiện balloon — **click vào balloon** để tải và chạy bộ cài silent, app tự thoát để thay file. So sánh semver, chỉ cài đúng asset `gotiengviet-<version>-x64-setup.exe`.
- Chẩn đoán updater: `%APPDATA%\gotiengviet\update.log` (mọi bước check/tải/cài + quyết định commit/resend của hook).

## Dùng hằng ngày

- **Đổi ngôn ngữ**: `Ctrl+Shift` / `Alt+Z` (chống lặp khi giữ phím), hoặc click trái icon `[V]`/`[E]` (ghi nhớ qua restart).
- **Menu chuột phải**: Bảng điều khiển, Telex/VNI, chính tả, chuẩn dấu, tự khởi động, Kiểm tra cập nhật, TSF (thử nghiệm), Thoát.
- **Bảng điều khiển**: kiểu gõ, chuẩn dấu, chính tả, tự khởi động, cụm AI (Ollama, model, URL) — lưu ở `%APPDATA%\gotiengviet\` như bản Linux.
- **Gợi ý AI dạng balloon**: từ sai sau khi gõ xong được hỏi Ollama nền (`"sai" co the ban muon go "dung"?`, chống spam 10 giây). Cần Ollama + `curl` (có sẵn từ Windows 10). **Click balloon** để nhận (còn nguyên thì thay tại chỗ, gõ tiếp rồi thì copy vào clipboard); lựa chọn được học vào `learned-corrections.txt` nên offline vẫn gợi ý.
- **Quy tắc gõ/commit**: Space commit đúng một lần; trong trình duyệt thì bôi đen đoạn cần thay rồi gõ đè (thanh địa chỉ nuốt Backspace để tắt gợi ý). Click chuột/đổi cửa sổ khi đang gõ dở thì bỏ phần dở.
- Macro/emoji/prompts/config: chung lõi và file `data/` với bản Linux.
- Chữ Việt hiển thị đúng mọi locale: toàn bộ UI dùng Unicode API, dialog đặt chữ lúc chạy.

## Giới hạn đã biết

- Chưa có gợi ý inline trong ô nhập (cả hook lẫn TSF).
- TSF chưa có gợi ý AI, chưa hỗ trợ trường mật khẩu đặc biệt (hook bỏ qua ô password cổ điển).
- Không bật đồng thời hook và TSF.

## Build và release

```sh
# MSYS2 MinGW-w64:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 make
make -f Makefile.win            # gotiengviet.exe + gtv_tsf.dll, chạy test
make -f Makefile.win setup VERSION=X.Y.Z   # cần Inno Setup (iscc.exe)
```

Cắt release: bump `windows/version.h` + `windows/resource.rc` + `ibus/gotiengviet.xml` cùng số, commit, push tag `vX.Y.Z` — CI build `gotiengviet-X.Y.Z-x64-setup.exe` và đính kèm release để app tự cập nhật.
