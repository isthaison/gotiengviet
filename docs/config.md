# Cấu hình

Thư mục cấu hình: `$XDG_CONFIG_HOME/gotiengviet/` (Linux, mặc định `~/.config/gotiengviet/`) và `%APPDATA%\gotiengviet\` (Windows).

| File | Nội dung |
|---|---|
| `config` | Kiểu gõ, kiểu đặt dấu, chính tả, tùy chọn AI |
| `ai.conf` | Provider `rule`/`ollama`, model, URL, port — ưu tiên hơn mục `[ai]` trong `config` |
| `macros.txt`, `emojis.txt` | Bảng riêng, thay thế hoàn toàn file hệ thống |
| `learned-words.txt`, `learned-corrections.txt` | Từ điển học (tự tạo, sửa/xóa được) |
| `prompts.conf` | Mẫu prompt Ollama riêng |

Mặc định khi chạy: Telex, `modern=true`, `spellcheck=true`, AI tắt (`rule`), model `qwen2:0.5b`.

Khi chưa có file người dùng, bộ gõ dùng mặc định trong `data/` (cài vào `/usr/share/gotiengviet/`, trên Windows là `data/` cạnh exe). Muốn tùy biến: copy file từ đó về thư mục cấu hình rồi sửa. Thứ tự tìm file: `GTV_DATA_DIR` (test/dev) → file người dùng → thư mục hệ thống → `./data`.

## Macro và emoji

Macro mở rộng khi kết thúc từ: `vn`→`Việt Nam`, `hn`→`Hà Nội`, `dc`→`được`, `ko`→`không`, … Emoji: `:smile:`→😊, `:)`→😊, `<3`→❤️, … Đang gõ tiền tố `:` (ví dụ `:sm`) thì IBus gợi ý tối đa 5 emoji.

Định dạng file text: `key=value` mỗi dòng, tách ở dấu `=` đầu, `#` là chú thích, UTF-8, trùng key lấy dòng đầu, giữ thứ tự file. Ví dụ thêm macro riêng (`~/.config/gotiengviet/macros.txt`):

```ini
cty=Công ty TNHH
```

Sửa/xóa file rồi khởi động lại bộ gõ để nhận bảng mới.

## Chính tả và gợi ý

Ba lớp phối hợp, không chặn phím gõ:

1. **Gạch đỏ ngoại tuyến**: thuần quy tắc âm tiết (âm đầu, vần, phụ âm cuối, thanh) cộng từ điển học. Chữ chưa gõ dấu luôn cho qua để không gạch oan khi đang gõ dở.
2. **Gợi ý Ollama** (bật trong Cài đặt): ngừng gõ 350 ms mới gửi ngữ cảnh + từ đang gõ; gõ tiếp thì hủy yêu cầu cũ. Tắt AI hoặc mất Ollama thì không có gợi ý online.
3. **Từ điển học**: chỉ những gợi ý bạn **chủ động chọn** mới được nhớ. Lần sau: từ sai cũ bị gạch đỏ ngay không cần mạng; Ollama rớt vẫn gợi ý từ dữ liệu đã học (kể cả hoàn thành tiền tố không dấu như `thong`→`thông`). Kèm sẵn seed (`data/learned-words.txt` 489 từ, `data/learned-corrections.txt` lỗi kinh điển); lần lưu đầu copy seed vào file riêng của bạn.

Ollama là tùy chọn, cần `curl`. Request qua stdin/argv (không ghép shell), có timeout và giới hạn response. Test dùng curl giả lập, không cần mạng/model thật.

## Mẫu prompt Ollama

`prompts.conf` có placeholder `%s` điền theo thứ tự:

```ini
[suggest]             # %s = ngữ cảnh, từ đang gõ, gợi ý sửa lỗi
prompt=...            # correct_hint: từ bị đánh dấu sai; complete_hint: hoàn thành từ
```

Mọi `%` khác giữ nguyên (`%%` cho dấu phần trăm), `\n` là xuống dòng. Thiếu mẫu nào, yêu cầu đó trả rỗng/báo lỗi thay vì dùng chữ cứng.

---
Xem thêm: [README](../README.md) · [Bản Windows](windows.md) · [Bản macOS](macos.md) ·
[Phát triển](dev.md) · [Trang chủ](https://isthaison.github.io/gotiengviet/)
