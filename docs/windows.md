# Bản Windows

Native Windows 10/11 (x64). Gõ bằng **TSF Text Service** (`gtv_tsf.dll`, composition chuẩn Windows — đúng trong mọi ô nhập kể cả autocomplete/khó tính); app khay hệ thống lo cấu hình, gợi ý AI, cập nhật. Không còn hook bàn phím.

- Sau khi cài: đăng xuất/đăng nhập lại rồi chọn GoTiengViet bằng `Win+Space`.
- Nếu chưa thấy bàn phím GoTiengViet: vào **Settings → Time & Language → Language** → chọn `English` (hoặc `Tiếng Việt`) → **Options** → **Add a keyboard** → chọn **GoTiengViet**. Không thấy nữa thì đăng xuất/đăng nhập lại một lần.
- Icon khay hiện kiểu gõ như bản Linux: đỏ `T` = Telex, đỏ `V` = VNI (click-trái/chuột-phải đều mở menu). Không có chữ `E`/nút tắt: chuyển Anh/Việt bằng `Win+Space` (Windows quản lý indicator ngôn ngữ).
- Telex/VNI, chuẩn dấu, chính tả đổi trong Bảng điều khiển/menu tray, TSF nhận ngay khi chuyển ô nhập (đọc lại cấu hình mỗi lần kích hoạt).

## Cài đặt và cập nhật

- Mỗi release GitHub đính kèm `gotiengviet-<version>-x64-setup.exe` (Inno Setup, per-user, không cần admin, gỡ sạch qua Add/Remove Programs, giữ nguyên cấu hình).
- App tự kiểm tra release mới nhất mỗi ngày; menu tray **Kiểm tra cập nhật...** để kiểm tra tay. Có bản mới thì hiện balloon — **click vào balloon** để tải và chạy bộ cài silent, app tự thoát để thay file. So sánh semver, chỉ cài đúng asset `gotiengviet-<version>-x64-setup.exe`.
- Chẩn đoán: `%APPDATA%\gotiengviet\update.log` (mọi bước check/tải/cài của updater).

## Dùng hằng ngày

- **Menu khay**: Bảng điều khiển, Telex/VNI, chính tả, chuẩn dấu,
  tự khởi động (thường/admin), Kiểm tra cập nhật, Thoát.
- **Bảng điều khiển**: kiểu gõ, chuẩn dấu, chính tả, tự khởi động, cụm AI (Ollama, model chọn
  trong dropdown như bản Linux — `qwen2:0.5b`/`qwen2:1.5b`/`rule`, gõ tay model khác vẫn được — URL,
  dòng trạng thái + log `ollama serve` như bản Linux),
  nút **Dữ liệu...** — lưu ở `%APPDATA%\gotiengviet\` như bản Linux. Log serve nằm ở `%TEMP%\ollama_serve.log`;
  khi bật AI (tick checkbox hoặc lưu), app tự kiểm tra → tải `OllamaSetup.exe` và cài im lặng nếu thiếu →
  chạy `ollama serve` nền → pull model nếu chưa có, chờ `/api/tags` tối đa 15s. Cần mạng lần đầu (bản cài ~1GB).
- **Gợi ý AI dạng balloon**: TSF báo mỗi từ vừa commit về app khay; từ sai được hỏi Ollama nền (`"sai" co the ban muon go "dung"?`, chống spam 10 giây). Cần Ollama + `curl` (có sẵn từ Windows 10). **Click balloon** để nhận (chưa gõ tiếp thì thay tại chỗ, rồi thì copy vào clipboard); lựa chọn được học vào `learned-corrections.txt` nên offline vẫn gợi ý.
- Macro/emoji/prompts/config: chung lõi và file `data/` với bản Linux.
- Chữ Việt hiển thị đúng mọi locale: toàn bộ UI dùng Unicode API, dialog đặt chữ lúc chạy.

## Khởi động với quyền admin

App thường (Run key) không chạm được vào cửa sổ admin (Task Manager, regedit, cmd admin):
chọn menu **Khởi động với quyền admin**, đồng ý UAC một lần — app tạo tác vụ logon
`GoTiengViet` chạy elevated, tắt mục Run thường để không mở 2 bản. Tắt/chuyển chế độ
cũng cần UAC một lần. Hai mục khởi động loại trừ nhau: chỉ một được check.

## Quản lý dữ liệu (macro/emoji)

Bảng điều khiển → **Dữ liệu...**: chuyển Macro/Emoji, chọn dòng để sửa, **Lưu** để thêm/cập nhật,
**Xóa** để gỡ, **Mở thư mục** để sửa file text tay. Khóa không chứa dấu cách hay `=`,
giá trị không rỗng. Lưu ghi đè file riêng của bạn (`macros.txt`/`emojis.txt` trong
`%APPDATA%\gotiengviet\`, thay thế hoàn toàn file hệ thống) — **app đang mở phải khởi
động lại mới nhận bảng mới** (TSF đọc bảng lúc app khởi động).

## Gạch đỏ dưới chữ Việt trong trình duyệt/app khác

Không phải của GoTiengViet: TSF của app không bao giờ vẽ gạch đỏ (không set display
attribute nào), bản Windows cũng không có gạch đỏ inline (chỉ có balloon gợi ý AI).
Gạch đỏ cả câu như ảnh là **spellchecker của trình duyệt** (từ điển Anh gặp chữ Việt)
hoặc Windows (Settings → Time & language → Typing → Highlight misspelt words khi đang
ở ENG). Tắt spellcheck của ô nhập đó, hoặc thêm tiếng Việt vào trình duyệt là hết.

## Giới hạn đã biết

- Chưa có gợi ý inline trong ô nhập.
- TSF chưa hỗ trợ trường mật khẩu đặc biệt.

## Build và release

```sh
# MSYS2 MinGW-w64 (cửa lệnh chung, như Linux/macOS):
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 make
./gtv.sh build                 # gotiengviet.exe + gtv_tsf.dll + test
./gtv.sh package               # gotiengviet-X.Y.Z-x64-setup.exe (cần Inno Setup)
./gtv.sh install               # cài silent + chạy app (đóng app giữ DLL trước)
./gtv.sh uninstall             # gỡ + dọn Run/task/profile
# Từ cmd.exe: gtv build | test | package | install | uninstall
# Lệnh make gốc vẫn dùng được: mingw32-make -f Makefile.win [test|staging]
```

Cắt release: `./gtv.sh bump X.Y.Z` (ghi `VERSION` + mọi file mang version),
commit, push tag `vX.Y.Z` — CI build `gotiengviet-X.Y.Z-x64-setup.exe` và đính kèm release để app tự cập nhật.

---
Xem thêm: [README](../README.md) · [Cấu hình](config.md) · [Bản macOS](macos.md) ·
[Phát triển](dev.md) · [Trang chủ](https://isthaison.github.io/gotiengviet/)
