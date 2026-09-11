# Bản macOS (Preview)

> 🧪 Đang hoàn thiện: gõ cơ bản (Telex/VNI) đã chạy, cửa sổ Cài đặt và gói `.dmg`
> ký số vẫn trong roadmap. Dùng hàng ngày nên chờ bản ổn định; dev/test mời dùng thử và góp ý.

Native macOS qua [InputMethodKit](https://developer.apple.com/documentation/inputmethodkit):
`GoTiengViet.app` chạy nền (agent, không icon Dock), dùng chung lõi C với bản Linux/Windows —
cùng quy tắc Telex/VNI, cùng file cấu hình `~/.config/gotiengviet/`.

## Cài bản preview

1. Tải file cài đặt `gotiengviet-<version>-macos.dmg` hoặc `gotiengviet-<version>-macos.pkg` ở trang
   [Releases](https://github.com/isthaison/gotiengviet/releases/latest)
   (do CI build, chưa ký số Apple).
2. **Cài đặt**:
   - Nếu dùng file `.pkg`: Click đúp để chạy trình cài đặt chuẩn của macOS, ứng dụng sẽ được tự động cài vào `/Library/Input Methods/`.
   - Nếu dùng file `.dmg`: Mở file và kéo thả `GoTiengViet.app` vào thư mục `Input Methods`.
3. Mở app một lần (chuột phải → Open, chấp nhận cảnh báo "unidentified developer"),
   rồi đăng xuất/đăng nhập lại.
4. Vào **System Settings → Keyboard → Text Input → Edit → Add** → chọn **GoTiengViet**,
   chuyển bằng `Fn/Caps Lock` hoặc `Control+Space`.

## Dùng hằng ngày

- Gõ Telex/VNI như bản Linux/Windows (kiểu gõ, chuẩn dấu, chính tả đọc từ
  `~/.config/gotiengviet/config` — xem [Cấu hình](config.md)).
- Mở app lần nữa để mở cửa sổ **Dữ liệu** (bảng Macro/Emoji: xem, thêm/sửa/xóa,
  mở thư mục — chung API với bản Windows/Linux; lưu xong khởi động lại app để nhận).
  Bảng Cài đặt đầy đủ (kiểu gõ/chuẩn dấu/AI) đang làm tiếp.
- Tự kiểm tra cập nhật mỗi ngày như bản Windows (tự tải bản cập nhật mới nhất về `/tmp` và mở để cài đặt).
- Macro/emoji/prompts/config: chung lõi và file `data/` với mọi nền tảng.
- Hỗ trợ AI (Ollama): cài đặt Ollama trên macOS bằng lệnh chính thức:
  ```sh
  curl -fsSL https://ollama.com/install.sh | sh
  ```

## Build từ source (dev)

```sh
brew install glib pkg-config
```

CI (`.github/workflows/build-macos.yml`) làm ba việc, bạn chạy lại y hệt trên máy Mac:

1. **Test lõi C** (cùng suite với Windows: engine + support + algorithm —
   test IBus/GTK chỉ chạy trên Linux):

   ```sh
   mkdir -p build/macos/fixtures
   CORE_CFLAGS="$(pkg-config --cflags glib-2.0 gio-2.0) -Iengine"
   CORE_LIBS="$(pkg-config --libs glib-2.0 gio-2.0) -lm"
   clang -O2 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
     $CORE_CFLAGS tests/test_engine.c engine/*.c $CORE_LIBS -o build/macos/test-engine
   clang -O2 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
     $CORE_CFLAGS tests/test_support.c engine/*.c $CORE_LIBS -o build/macos/test-support
   clang -O2 -g tests/fake_curl.c $CORE_LIBS -o build/macos/fixtures/curl
   GTV_DATA_DIR="$PWD/data" ./build/macos/test-engine
   GTV_DATA_DIR="$PWD/data" GTV_TEST_CURL_DIR="$PWD/build/macos/fixtures" \
     ./build/macos/test-support
   ```

2. **Link bundle** `GoTiengViet.app` (lõi C + ObjC, manual-retain, frameworks
   Cocoa + InputMethodKit), copy `macos/Info.plist` + `data/` vào `Resources`.
3. Trên tag `v*`: zip thành `gotiengviet-<version>-macos.zip`, đính kèm release
   (app chưa ký số Apple — mở lần đầu bằng chuột phải → Open).

Cấu trúc bundle do CI tạo:

```text
GoTiengViet.app/Contents/
  Info.plist            Bundle ID vn.gotiengviet.GoTiengViet, LSUIElement (agent)
  MacOS/GoTiengViet     binary (lõi C + IMK controller)
  Resources/data/       macro, emoji, config/ai mẫu, prompts, seed từ điển
```

## Giới hạn đã biết (preview)

- Chưa có bảng Cài đặt đầy đủ, chưa gợi ý inline/AI trong ô nhập.
- Chưa ký số + notarize → macOS chặn mở trực tiếp lần đầu (chuột phải → Open).
- Chưa hỗ trợ trường mật khẩu đặc biệt.

Báo lỗi kèm macOS version + log Console.app chứa `GoTiengViet`.

---
Xem thêm: [README](../README.md) · [Cấu hình](config.md) · [Phát triển](dev.md) ·
[Trang chủ](https://isthaison.github.io/gotiengviet/)
