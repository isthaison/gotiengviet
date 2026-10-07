# Cấu hình

Thư mục cấu hình: `$XDG_CONFIG_HOME/gotiengviet/` (Linux, mặc định `~/.config/gotiengviet/`) và `%LOCALAPPDATA%\gotiengviet\` (Windows).

| File | Nội dung |
|---|---|
| `config` | Kiểu gõ, kiểu đặt dấu, chính tả, bật/tắt gợi ý từ điển |
| `macros.txt`, `emojis.txt` | Bảng riêng, thay thế hoàn toàn file hệ thống |
| `learned-words.txt`, `learned-corrections.txt` | Từ điển học (tự tạo, sửa/xóa được) |

Mặc định khi chạy: Telex, `modern=true`, `spellcheck=true`, gợi ý từ điển bật.

Khi chưa có file người dùng, bộ gõ dùng mặc định trong `data/` (cài vào `/usr/share/gotiengviet/`, trên Windows là `data/` cạnh exe). Muốn tùy biến: copy file từ đó về thư mục cấu hình rồi sửa. Thứ tự tìm file: `GTV_DATA_DIR` (test/dev) → file người dùng → thư mục hệ thống → `./data`.

## Macro và emoji

Macro (gõ tắt) mở rộng khi bấm **Tab**: gõ `vn` rồi nhấn **Tab** → `Việt Nam`, `hn`+[Tab] → `Hà Nội`, `dc`+[Tab] → `được`, `ko`+[Tab] → `không`, … Bấm phím Space, dấu câu hoặc Enter sẽ giữ nguyên chữ viết tắt (không tự động bung gõ tắt, thuận tiện khi bạn cần gõ đúng ký tự đó). Emoji cũng bung bằng Tab (gõ trigger rồi nhấn Tab; Space/dấu câu giữ nguyên trigger). Emoji: `:smile:`→😊, `:)`→😊, `<3`→❤️, … Đang gõ tiền tố `:` (ví dụ `:sm`) thì IBus gợi ý tối đa 5 emoji.

Định dạng file text: `key=value` mỗi dòng, tách ở dấu `=` đầu, `#` là chú thích, UTF-8, trùng key lấy dòng đầu, giữ thứ tự file. Ví dụ thêm macro riêng (`~/.config/gotiengviet/macros.txt`):

```ini
cty=Công ty TNHH
```

Sửa/xóa file rồi khởi động lại bộ gõ để nhận bảng mới.

## Chính tả và gợi ý từ điển

Ba lớp phối hợp, không chặn phím gõ, tất cả offline:

1. **Gạch đỏ ngoại tuyến**: thuần quy tắc âm tiết (âm đầu, vần, phụ âm cuối, thanh) cộng từ điển học. Chữ chưa gõ dấu luôn cho qua để không gạch oan khi đang gõ dở.
2. **Sửa lỗi**: từ sai (`kông`) được gợi ý từ đúng (`không`) nhờ từ điển + từ điển học; bấm vào balloon (Windows) hoặc chọn trong bảng (Linux) để nhận.
3. **Hoàn thành tiền tố**: đang gõ `thong` thì gợi ý `thông`, `thống`… (kể cả không dấu); nhận bằng **Tab**, số `1-5` hoặc click.

Chỉ những gợi ý bạn **chủ động chọn** mới được nhớ vào từ điển học. Lần sau: từ sai cũ bị gạch đỏ ngay không cần mạng. Kèm sẵn seed (`data/learned-words.txt`, `data/learned-corrections.txt` lỗi kinh điển); lần lưu đầu copy seed vào file riêng của bạn.

## Từ điển `dict-vi.txt`

`data/dict-vi.txt` (~7000 từ, một từ một dòng, UTF-8) là seed đi kèm app. File được sinh bởi `tools/mkdict.sh` từ wordlist công cộng + seed của repo (xem header file). Mỗi release GitHub đính kèm `dict-vi.txt` mới nhất; app tự tải về khi kiểm tra cập nhật (cùng nhịp kiểm tra bản mới mỗi ngày, hoặc nút **Cập nhật từ điển** trong Cài đặt) và chỉ nhận file hợp lệ, mới hơn bản đang có. Bản tải về nằm ở thư mục dữ liệu người dùng (`~/.local/share/gotiengviet/` Linux, `%LOCALAPPDATA%\gotiengviet\` Windows) và được ưu tiên hơn seed.

---
Xem thêm: [README](../README.md) · [Bản Windows](windows.md) · [Bản macOS](macos.md) ·
[Phát triển](dev.md) · [Trang chủ](https://isthaison.github.io/gotiengviet/)
