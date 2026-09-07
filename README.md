# GoTiengViet

Bộ gõ tiếng Việt cho Linux, viết hoàn toàn bằng **C**. Tên ứng dụng vẫn là GoTiengViet; build và chạy không cần Go hay CGO.

Ứng dụng gồm lõi dùng chung cho Telex/VNI, IBus adapter, giao diện cấu hình GTK 3, indicator, demo terminal, kiểm tra chính tả, macro/emoji và API gợi ý Ollama tùy chọn.

## Quy tắc gõ thống nhất

Telex và VNI chỉ khác **bảng ánh xạ phím**. Cả hai đi qua cùng thuật toán trong `engine/compose.c`:

1. Xác định âm đầu, cụm nguyên âm và phụ âm cuối của phần đang gõ.
2. Ánh xạ phím thành thao tác: đặt thanh, đổi dạng chữ, xóa thanh hoặc gõ tắt.
3. Chọn chữ/cụm nguyên âm đích theo cấu trúc âm tiết. Dấu có thể gõ trước hoặc sau phụ âm cuối; chữ `d` đầu âm tiết có thể đổi thành `đ` bằng phím gõ ở cuối.
4. Đặt dạng chữ và dấu thanh độc lập, giữ nguyên chữ hoa/thường. Cặp `uo/uô` là một đích của dấu móc, đổi thành `ươ`.
5. Gõ lại dấu đang có: **bỏ dấu đó và thêm đúng một phím thô**. Đổi sang dấu khác thay dấu cũ. Dấu thanh được đặt lại đúng vị trí khi cụm nguyên âm thay đổi.

Không có bảng ngoại lệ theo từ và không suy đoán/khôi phục tiếng Anh trong thuật toán gõ. Các bảng từ trong `spell.c` chỉ phục vụ gợi ý chính tả, không quyết định kết quả phím gõ. Macro là chức năng mở rộng riêng khi kết thúc từ.

| Thao tác | Telex | VNI |
|---|---|---|
| Sắc, huyền, hỏi, ngã, nặng | `s f r x j` | `1 2 3 4 5` |
| Mũ | `a e o` trên nguyên âm tương ứng | `6` |
| Móc/trăng | `w` | `7` / `8` |
| Gạch chữ d | `d` | `9` |
| Xóa dấu thanh | `z` | `0` |

Ví dụ:

| Chuỗi phím | Kết quả |
|---|---|
| `bawst`, `bawts`, `batsw` | `bắt` |
| `duocjwd`, `duocwjd`, `dduocjw` | `được` |
| `duoc579`, `duoc795` (VNI) | `được` |
| `ass` / `a11` | `as` / `a1` |
| `aaa` / `a66` | `aa` / `a6` |
| `uoww` / `uo77` | `uow` / `uo7` |

**Thay đổi có chủ đích:** `test` ra `tét`, `pass` ra `pas`; không còn khôi phục tiếng Anh tự động. Để gõ chữ điều khiển thô, gõ lặp (`tesst → test`, `passs → pass`) hoặc chuyển nguồn bàn phím bằng `Super+Space`.

Phím `w` khi chưa có nguyên âm thêm token `ư`; lặp phím tắt hoàn tác token đó: `ww → w`, `sww → sw`. Các phím tắt `[`/`]` và `{`/`}` thêm `ươ`/`ư` và chữ hoa tương ứng; lặp phím tắt trả lại dấu ngoặc.

## Build và kiểm thử

Trên Debian/Ubuntu, cần compiler C và các thư viện hệ thống:

```sh
sudo apt install build-essential pkg-config libibus-1.0-dev libgtk-3-dev libayatana-appindicator3-dev librsvg2-bin
./build.sh
```

Các binary nằm trong `build/`:

- `ibus-engine-gotiengviet`: adapter IBus (`--ibus` để chạy trong phiên IBus).
- `ibus-setup-gotiengviet`: giao diện cấu hình; `--tray` chạy indicator, `--cli` cấu hình qua terminal.
- `gotiengviet-demo`: demo tương tác, đổi bằng `mode telex` / `mode vni`.
- `libgotiengviet.a`: thư viện C dùng chung.

```sh
make test                  # regression, cấu hình, stateful, JSON/Ollama fixture, hoán vị thao tác
make vet                   # build + test với -Werror
build/gotiengviet-demo --transform telex duocjwd
build/gotiengviet-demo --transform vni duoc579
```

Test hoán vị kiểm tra mọi thứ tự của phụ âm cuối, dấu thanh, dấu móc và dấu gạch trên cùng âm tiết, với cả hai kiểu gõ, hai kiểu đặt thanh và chữ hoa/thường. Test ngẫu nhiên kiểm tra 20.000 chuỗi, gồm phím điều khiển, chữ hoa, số và dấu ngoặc.

## Cài đặt và đóng gói

```sh
sudo ./install.sh
make package VERSION=0.2.1-1
```

Script cài đặt dùng các binary đã build trong `build/`, không lấy binary cũ từ `/tmp`. Mở GoTiengViet để thêm vào Input Sources, rồi chọn bằng `Super+Space`. Cấu hình người dùng được giữ nguyên.

Có thể kiểm tra cây cài đặt mà không cần root:

```sh
DESTDIR=/tmp/gotiengviet-staging ./install.sh
```

`make clean` chỉ xóa thư mục `build/`, không xóa các gói phát hành `.deb` hay file hệ thống.

## Cấu hình và gợi ý

Cấu hình nằm tại `$XDG_CONFIG_HOME/gotiengviet/` (mặc định `~/.config/gotiengviet/`):

- `config`: kiểu gõ, kiểu đặt thanh, chính tả và tùy chọn AI.
- `ai.conf`: provider `rule` hoặc `ollama`, model, URL và port; giá trị AI trong file này được ưu tiên khi đọc.

IBus sử dụng gợi ý chính tả cục bộ để không chặn xử lý phím. API C `gtv_ai_suggest` và lệnh demo dưới đây hỗ trợ Ollama, tự quay về quy tắc khi dịch vụ không có hoặc trả lỗi:

```sh
build/gotiengviet-demo --suggest kông
```

Ollama là tùy chọn; cần `curl` nếu bật provider này. Dữ liệu request được truyền bằng stdin và argv, không ghép vào lệnh shell. Mỗi request có timeout và giới hạn kích thước response. Test dùng chương trình C giả lập curl, không tải model hoặc gọi dịch vụ mạng.

## Cấu trúc mã nguồn

```text
engine/
  engine.h          API công khai và kiểu dữ liệu
  engine.c          xử lý chuỗi, buffer, backspace và commit
  compose.c         bảng phím + thuật toán chung Telex/VNI
  charset.c         bảng ký tự Unicode và phép biến đổi dấu
  phonology.c       vị trí đặt dấu thanh
  promotion.c       quy tắc nguyên âm ie/ye/uo
  config.c          đọc/lưu cấu hình dùng chung
  spell.c           kiểm tra và gợi ý chính tả
  macro.c           mở rộng macro/emoji
  ai.c, json.c      provider Ollama và JSON
  text.c            chuyển UTF-8/UCS-4
  telex.c, vni.c    adapter mỏng vào thuật toán chung
ibus/engine.c       vòng đời, phím và giao tiếp IBus
cmd/setup/main.c   GTK, indicator và cấu hình CLI
cmd/demo/main.c    demo C
 tests/             toàn bộ kiểm thử C
```

## Phiên bản Windows

GoTiengViet hỗ trợ Windows 10/11 native thông qua Win32 Low-Level Keyboard Hook (`WH_KEYBOARD_LL`) và System Tray:

* **Tự động build CI/CD**: Mỗi bản release trên GitHub tự động build và đính kèm gói `gotiengviet-windows-x64.zip` (chạy ngay không cần cài đặt).
* **Phím tắt chuyển ngôn ngữ**: `Ctrl + Shift` hoặc `Alt + Z` để đổi nhanh giữa chế độ [V] và [E].
* **Khay hệ thống (System Tray)**: Nhấp chuột trái vào icon [V]/[E] để đổi ngôn ngữ; nhấp chuột phải để mở Bảng điều khiển, chuyển kiểu gõ Telex/VNI hoặc Thoát.
* **Tự khởi động cùng Windows**: Tùy chọn trong bảng điều khiển hoặc menu chuột phải.
* **Build từ mã nguồn (MSYS2 MinGW-w64)**:
  ```sh
  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 make
  make -f Makefile.win
  ```

Các script shell chỉ phục vụ build, cài đặt và đóng gói. Không có mã nguồn ứng dụng Go, bridge CGO hoặc phụ thuộc toolchain Go.

Giấy phép: [MIT](LICENSE).

### Gợi ý vector cục bộ

Gợi ý kết hợp liên kết cụm từ với cosine similarity của tối đa tám từ gần nhất; từ càng gần có trọng số càng cao. Liên kết cụm từ trực tiếp được ưu tiên hơn từ chỉ tương tự về chủ đề. Bộ vector hiện là bảng 16 chiều nhỏ được khai báo trong mã C, chưa phải mô hình embedding được huấn luyện trên kho văn bản lớn.

Tiền tố chưa gõ dấu vẫn tìm được từ có dấu (`công nghệ` + `thong` → `thông tin`); dấu đã gõ được tôn trọng. Gợi ý giữ cách viết trong từ điển, không tự chuyển sang chữ hoa theo tiền tố (`Ch` hoặc `CH` đều gợi ý `chào`). Bộ xếp hạng chuẩn hóa Unicode NFC/NFD, bỏ khoảng trắng thừa, không dùng lại ngữ cảnh trước dấu kết thúc câu, không gợi ý lại chính từ đã hoàn thành.

Trong IBus, gợi ý ngữ cảnh đứng trước gợi ý sửa chính tả khi đang gõ một phần từ. Danh sách được khử trùng và giới hạn năm mục. Không có request mạng trong đường xử lý phím này.

### Win+T: viết lại và dịch với Ollama (Linux)

Nhấn **Win+T** khi GoTiengViet đang hoạt động: bộ gõ tự lấy đoạn đang chọn hoặc nội dung trước con trỏ cùng chữ đang gõ dở, rồi xử lý theo lựa chọn gần nhất. Kết quả hiện ngay tại ô nhập trong bảng của IBus, không chuyển focus sang cửa sổ khác. **Enter** thay câu gốc, **Esc** hủy, **Tab** đổi giữa viết lại và dịch rồi xử lý lại. Nội dung và con trỏ phải còn khớp với lúc bắt đầu; nếu bạn đã sửa câu thì không ghi đè.

Với ô nhập không cung cấp surrounding text nhưng có ID focus, bộ gõ có thể thay phần văn bản vừa gõ bằng các sự kiện Backspace qua IBus rồi chèn kết quả. Chỉ dùng phần bộ gõ đã ghi nhận trong ô hiện tại, tối đa 512 ký tự thông thường; không dùng cho emoji, dấu Unicode tách rời hoặc nhiều dòng. Gõ tiếp, đổi ô hoặc di chuyển con trỏ sẽ hủy kết quả cũ. Khả năng nhận phím được chuyển tiếp còn phụ thuộc ứng dụng.

Nếu không có đủ thông tin để thay, mở cửa sổ gọn có nội dung gốc thu lại trong mục “Nội dung gốc”. Enter ở đây **sao chép** kết quả; không giả định mọi ô nhập Linux đều hỗ trợ thay tự động. Trường mật khẩu/PIN không kích hoạt trợ lý.

Mặc định là **Viết lại**. Chạy `gotiengviet-assistant` để chọn **Dịch** và ngôn ngữ đích; thao tác/ngôn ngữ được nhớ trong `assistant.conf`, không lưu nội dung câu vào file này. Cửa sổ cho sửa kết quả; Shift+Enter xuống dòng, Esc hủy.

Dùng model và URL Ollama trong Cài đặt GoTiengViet; Ollama cần chạy và model được tải sẵn. Yêu cầu chạy nền, tối đa 120 giây. Win+T gửi nội dung tới URL đã cấu hình theo thao tác gần nhất. Chất lượng phụ thuộc model.

Win+T là phím tắt của engine IBus, không phải phím tắt toàn hệ thống. Có thể chạy `gotiengviet-assistant` khi dùng English (US) hoặc khi desktop giữ tổ hợp phím này; ở chế độ mở trực tiếp, Enter sao chép kết quả.

Kiểm thử: `make test-ibus` và `make test-assistant` (cần `broadwayd`).
