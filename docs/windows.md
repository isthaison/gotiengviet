# Bản Windows

Native Windows 10/11 (x64). Gõ bằng **TSF Text Service** (stub `gtv_tsf.dll` đăng ký một lần + engine versioned trong `ver\<bản>\`, composition chuẩn Windows — đúng trong mọi ô nhập kể cả autocomplete/khó tính); app khay hệ thống lo cấu hình, gợi ý AI, cập nhật. Không còn hook bàn phím.

- Cài lần đầu: đăng xuất/đăng nhập lại; ứng dụng tự gắn **GoTiengViet** làm bàn phím US duy nhất và xóa mục VIE cũ. Các bản **cập nhật sau không cần** đóng app, đăng xuất hay reboot (xem dưới).
- Nếu chưa thấy bàn phím GoTiengViet: vào **Settings → Time & Language → Language** → chọn `English (United States)` → **Options** → **Add a keyboard** → chọn **GoTiengViet**. Không thấy nữa thì đăng xuất/đăng nhập lại một lần.
- Icon khay hiện kiểu gõ như bản Linux: đỏ `T` = Telex, đỏ `V` = VNI (**click-trái** trực tiếp vào icon khay để chuyển đổi nhanh giữa Telex và VNI; chuột-phải mở menu; click đúp mở Cài đặt). Tooltip khay cũng hiện `GoTiengViet — Telex/VNI` nên chụp màn hình vẫn thấy mode gõ hiện tại.
- Telex/VNI, chuẩn dấu, chính tả đổi trong Bảng điều khiển/menu tray/click khay, TSF nhận ngay khi chuyển ô nhập (đọc lại cấu hình mỗi lần kích hoạt).

## Cài đặt và cập nhật

- Mỗi release GitHub đính kèm `gotiengviet-<version>-x64-setup.exe` (Inno Setup, per-user, không cần admin, gỡ sạch qua Add/Remove Programs, giữ nguyên cấu hình).
- Cập nhật không chạm app đang chạy, không cần đăng xuất/reboot: mỗi bản nằm trong thư mục version riêng (`ver\<bản>\`), stub TSF (`gtv_tsf.dll`) đăng ký đúng một lần nên không cài lại, không UAC. App đang mở giữ bản cũ tới khi thoát; app mở mới dùng bản mới ngay; app khay tự tiếp quản bản mới trong ~1 giây. Chỉ giữ bản hiện tại + 1 bản rollback, cũ hơn tự dọn.
- App tự kiểm tra release mới nhất mỗi ngày; menu tray **Kiểm tra cập nhật...** để kiểm tra tay. Có bản mới thì hiện balloon — **click vào balloon** để tải và chạy bộ cài silent. So sánh semver, chỉ cài đúng asset `gotiengviet-<version>-x64-setup.exe`.
- Code signing: bản release ký self-signed cert (RSA 4096, 1 năm) — bypass SmartScreen trên大部分 Windows. Nếu cert chưa cấu hình, bản unsigned vẫn hoạt động bình thường nhưng SmartScreen có thể hiện cảnh báo lần đầu chạy.
- Chẩn đoán: `%APPDATA%\gotiengviet\update.log` (mọi bước check/tải/cài của updater).

## Dùng hằng ngày

- **Menu khay**: Bảng điều khiển, Telex/VNI, chính tả, chuẩn dấu,
  tự khởi động (thường/admin), Kiểm tra cập nhật, Thoát.
- **Bảng điều khiển**: kiểu gõ, chuẩn dấu, chính tả, tự khởi động, cụm AI (Ollama, model chọn
  trong dropdown như bản Linux — `qwen2.5:0.5b`/`qwen2.5:1.5b`/`rule`, gõ tay model khác vẫn được — URL,
  dòng trạng thái + log `ollama serve` như bản Linux),
  nút **Dữ liệu...** — lưu ở `%APPDATA%\gotiengviet\` như bản Linux. Log serve nằm ở `%TEMP%\ollama_serve.log`;
  khi bật AI (tick checkbox hoặc lưu), app tự kiểm tra → tự động cài Ollama bằng lệnh chính thức (`irm https://ollama.com/install.ps1 | iex`) chạy nền hoàn toàn im lặng nếu thiếu →
  chạy `ollama serve` nền → pull model nếu chưa có, chờ `/api/tags` tối đa 15s. Cần mạng lần đầu.
- **Gợi ý AI dạng balloon**: TSF báo mỗi từ vừa commit về app khay; từ sai được hỏi Ollama nền (`"sai" co the ban muon go "dung"?`, chống spam 10 giây). Cần Ollama + `curl` (có sẵn từ Windows 10). **Click balloon** để nhận (chưa gõ tiếp thì thay tại chỗ, rồi thì copy vào clipboard); lựa chọn được học vào `learned-corrections.txt` nên offline vẫn gợi ý.
- **Bảng gợi ý keyword inline (broker ngoài process)**: TSF trong app nhập liệu chỉ giữ composition/candidate và bắt phím, không tạo cửa sổ (an toàn với Electron/OpenCode); process tray riêng vẽ overlay cạnh caret, không lấy focus. Gõ `:sm` gợi ý emoji ngay không cần AI; từ thường gợi ý từ Ollama sau debounce 350 ms, rớt mạng dùng dữ liệu đã học. Phím: `Up/Down` di chuyển, `Tab`/`Enter` hoặc số `1-5` (Telex) để nhận, `Esc` bỏ (vẫn gõ tiếp), `Space` giữ nguyên chữ. Không lấy được tọa độ caret thật thì không hiện gì (không fallback góc màn hình). Từ trùng macro/emoji khớp tuyệt đối vẫn ưu tiên bung bằng `Tab` như cũ. Lựa chọn được học như balloon.
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

Macro và emoji đều kích hoạt bằng phím **Tab** (gõ từ tắt/trigger rồi nhấn Tab để bung; bấm Space hoặc dấu câu giữ nguyên, không tự động bung ngoài ý muốn).

## Gạch đỏ dưới chữ Việt trong trình duyệt/app khác

Không phải của GoTiengViet: TSF của app không bao giờ vẽ gạch đỏ (không set display
attribute nào), bản Windows cũng không có gạch đỏ inline (chỉ có balloon gợi ý AI).
Gạch đỏ cả câu như ảnh là **spellchecker của trình duyệt** (từ điển Anh gặp chữ Việt)
hoặc Windows (Settings → Time & language → Typing → Highlight misspelt words khi đang
ở ENG). Tắt spellcheck của ô nhập đó, hoặc thêm tiếng Việt vào trình duyệt là hết.

## Giới hạn đã biết

- Gợi ý Ollama inline chạy qua overlay của process tray (không tạo cửa sổ trong app nhập liệu nên an toàn với Electron/OpenCode); macro/emoji vẫn bung bằng `Tab`.
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
