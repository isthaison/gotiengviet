# GoTiengViet

Bộ gõ tiếng Việt cho Linux, viết hoàn toàn bằng **C**. Tên ứng dụng vẫn là GoTiengViet; build và chạy không cần Go hay CGO.

Lõi dùng chung cho Telex/VNI, IBus adapter, giao diện cấu hình GTK 3, indicator, demo terminal, kiểm tra chính tả, macro/emoji, gợi ý từ tiếp theo ngoại tuyến và API gợi ý Ollama tùy chọn.

## Mục lục

- [Tính năng chính](#tính-năng-chính)
- [Bắt đầu nhanh](#bắt-đầu-nhanh)
- [Quy tắc gõ thống nhất](#quy-tắc-gõ-thống-nhất)
- [Build và kiểm thử](#build-và-kiểm-thử)
- [Cài đặt và đóng gói](#cài-đặt-và-đóng-gói)
- [Tham chiếu lệnh](#tham-chiếu-lệnh)
- [Cấu hình](#cấu-hình)
- [Chính tả, macro, emoji và gợi ý](#chính-tả-macro-emoji-và-gợi-ý)
- [Ctrl+T: viết lại và dịch với Ollama (Linux)](#ctrlt-viết-lại-và-dịch-với-ollama-linux)
- [Cấu trúc mã nguồn](#cấu-trúc-mã-nguồn)
- [API thư viện C](#api-thư-viện-c)
- [Xử lý sự cố](#xử-lý-sự-cố)
- [Phiên bản Windows](#phiên-bản-windows)

## Tính năng chính

- Telex và VNI dùng chung một thuật toán (`engine/compose.c`), chỉ khác bảng ánh xạ phím.
- Hai kiểu đặt dấu: hiện đại (`modern=true`, mặc định) và truyền thống (`modern=false`); khác nhau ở vị trí dấu của các vần `oa`, `oe`, `uy` (ví dụ `hoà`/`hòa`).
- Kiểm tra chính tả cục bộ; gợi ý từ và sửa lỗi theo ngữ cảnh bằng Ollama, với debounce 350 ms.
- Macro gõ tắt (`vn` → `Việt Nam`, `ko` → `không`, …) và emoji (`:smile:` → 😊, `:)`, `<3`, …).
- Tích hợp IBus: preedit, surrounding text, gợi ý, tránh kích hoạt trong ô mật khẩu/PIN.
- Trợ lý Ctrl+T viết lại/dịch bằng Ollama (tùy chọn, mặc định tắt).
- Cấu hình qua GUI GTK, indicator khay hệ thống hoặc terminal (`--cli`).

## Bắt đầu nhanh

```sh
sudo apt install build-essential pkg-config libibus-1.0-dev libgtk-3-dev \
  libayatana-appindicator3-dev libatspi2.0-dev librsvg2-bin
./build.sh
sudo ./install.sh
```

Sau đó:

1. Mở **Settings → Keyboard → Input Sources**, thêm **GoTiengViet**.
2. Chọn bằng `Super+Space`.
3. Mở **GoTiengViet** (cấu hình) để chọn Telex/VNI, kiểu đặt dấu, chính tả, AI.
4. Kiểm tra nhanh: `build/gotiengviet-demo --transform telex duocjwd` phải ra `được`.

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

Kiểu đặt dấu `modern`:

- `modern=true` (mặc định): `oa`, `oe`, `uy` đặt dấu ở nguyên âm thứ hai (`hòa`, `tóe`).
- `modern=false`: đặt dấu ở nguyên âm thứ nhất theo kiểu truyền thống (`hoà`, `toé`).

## Build và kiểm thử

Yêu cầu trên Debian/Ubuntu:

```sh
sudo apt install build-essential pkg-config libibus-1.0-dev libgtk-3-dev \
  libayatana-appindicator3-dev libatspi2.0-dev librsvg2-bin
```

Lệnh chính:

```sh
./build.sh                         # = make build test
make build                         # build 4 binary + libgotiengviet.a
make test                          # engine + support (config, JSON, Ollama fixture, chính tả, macro, hoán vị, ngẫu nhiên 20.000 chuỗi)
make vet                           # build + test với -Werror
make test-ibus                     # kiểm thử vòng đời IBus và trợ lý
make test-assistant                # kiểm thử cửa sổ trợ lý (cần broadwayd nếu không có desktop)
make test-ui                       # kiểm thử giao diện setup
make test-text-target-live         # opt-in: mở ô nhập thật để kiểm tra AT-SPI
make help                          # liệt kê target
```

Các binary nằm trong `build/`:

- `ibus-engine-gotiengviet`: adapter IBus (`--ibus` để chạy trong phiên IBus).
- `ibus-setup-gotiengviet`: GUI cấu hình; `--tray` chạy indicator, `--cli` cấu hình terminal, `--help` xem trợ giúp.
- `gotiengviet-assistant`: cửa sổ viết lại/dịch Ollama.
- `gotiengviet-demo`: demo tương tác và batch (`--transform`, `--suggest`, `--predict`).
- `libgotiengviet.a`: thư viện C dùng chung.

Test hoán vị kiểm tra mọi thứ tự của phụ âm cuối, dấu thanh, dấu móc và dấu gạch trên cùng âm tiết, với cả hai kiểu gõ, hai kiểu đặt thanh và chữ hoa/thường. Test ngẫu nhiên kiểm tra 20.000 chuỗi, gồm phím điều khiển, chữ hoa, số và dấu ngoặc.

## Cài đặt và đóng gói

```sh
sudo ./install.sh
make package VERSION=0.3.0-1
```

- Script cài đặt dùng các binary đã build trong `build/`, không lấy binary cũ từ `/tmp`. Mở GoTiengViet để thêm vào Input Sources, rồi chọn bằng `Super+Space`. Cấu hình người dùng được giữ nguyên.
- Kiểm tra cây cài đặt mà không cần root:

```sh
DESTDIR=/tmp/gotiengviet-staging ./install.sh
```

- `make clean` chỉ xóa thư mục `build/`, không xóa các gói phát hành `.deb` hay file hệ thống.
- Gỡ thủ công: xóa `/usr/libexec/ibus-engine-gotiengviet`, `/usr/libexec/ibus-setup-gotiengviet`, `/usr/bin/gotiengviet*`, `/usr/share/ibus/component/gotiengviet.xml`, rồi `ibus restart`.

## Tham chiếu lệnh

```sh
# Demo gõ chữ (batch, dùng cho script/regression)
build/gotiengviet-demo --transform telex duocjwd
build/gotiengviet-demo --transform vni duoc579
build/gotiengviet-demo --suggest kông
build/gotiengviet-demo --suggest kông "công nghệ"
build/gotiengviet-demo --predict "công nghệ" thong
build/gotiengviet-demo --help

# Demo tương tác (gõ trực tiếp, đổi kiểu gõ trong phiên)
build/gotiengviet-demo
# > mode telex | mode vni | quit

# Cấu hình
ibus-setup-gotiengviet             # mở GUI (tự bật tray nền)
ibus-setup-gotiengviet --tray      # chỉ chạy indicator (cần DISPLAY/WAYLAND_DISPLAY)
ibus-setup-gotiengviet --cli       # cấu hình qua terminal, Enter giữ giá trị cũ
ibus-setup-gotiengviet --help

# Trợ lý Ollama (thường do IBus gọi với --stdin; mở tay để dùng độc lập)
gotiengviet-assistant [--stdin] [--headless] [--replace]
```

`--stdin` đọc nội dung từ stdin, `--headless` không hiện cửa sổ (in kết quả ra stdout), `--replace` cho phép Enter thay câu gốc khi được IBus gọi.

## Cấu hình

Cấu hình nằm tại `$XDG_CONFIG_HOME/gotiengviet/` (mặc định `~/.config/gotiengviet/`):

- `config`: kiểu gõ, kiểu đặt thanh, chính tả và tùy chọn AI.
- `ai.conf`: provider `rule` hoặc `ollama`, model, URL và port; **giá trị AI trong file này được ưu tiên** khi đọc.
- `assistant.conf`: thao tác gần nhất (viết lại/dịch) và ngôn ngữ đích; không lưu nội dung câu.

Ví dụ `config`:

```ini
[input]
method=telex
modern=true
spellcheck=true
charset=unicode

[ai]
enable=false
model=qwen2:0.5b
url=http://localhost:55602
port=55602
```

Ví dụ `ai.conf`:

```ini
[ai]
provider=rule
model=qwen2:0.5b
url=http://localhost:55602
port=55602
```

Mặc định khi chưa có file: Telex, `modern=true`, `spellcheck=true`, AI tắt (`rule`), model `qwen2:0.5b`, URL `http://localhost:55602`.

## Chính tả, macro, emoji và gợi ý

Gạch đỏ báo từ sai dùng kiểm tra ngoại tuyến (`spell_word_valid`: từ điển + quy tắc âm tiết) để không chặn xử lý phím. Gợi ý sửa lỗi và từ tiếp theo do Ollama xử lý (bất đồng bộ, có hủy khi gõ tiếp); khi tắt AI hoặc Ollama không phản hồi thì không có gợi ý:

```sh
build/gotiengviet-demo --suggest kông
# không  (cần Ollama đang chạy; tắt AI thì ra rỗng)
build/gotiengviet-demo --predict "công nghệ" thong
# thông tin ...
```

Ollama là tùy chọn; cần `curl` nếu bật provider này. Dữ liệu request được truyền bằng stdin và argv, không ghép vào lệnh shell. Mỗi request có timeout và giới hạn kích thước response. Test dùng chương trình C giả lập curl, không tải model hoặc gọi dịch vụ mạng.

### Macro gõ tắt

Macro mở rộng khi kết thúc từ (gõ dấu cách/câu):

| Gõ | Ra |
|---|---|
| `vn`, `hn`, `hcm` | `Việt Nam`, `Hà Nội`, `Hồ Chí Minh` |
| `dc`, `ko`, `ntn` | `được`, `không`, `như thế nào` |
| `cx`, `cb` | `cũng`, `chuẩn bị` |

So khớp không phân biệt hoa/thường.

### Emoji

Gõ đúng mã rồi kết thúc từ để chốt emoji, ví dụ `:smile:` → 😊:

- Mã kiểu Slack: `:smile:`, `:thumbsup:`, `:+1:`, `:heart:`, `:fire:`, `:coffee:`, `:vn:`, …
- Mặt cười gõ nhanh: `:)`, `:-)`, `:D`, `:(`, `;)`, `:P`, `<3`, `(y)`, `(n)`, …
- Gợi ý trong IBus: đang gõ tiền tố `:` (ví dụ `:sm`) sẽ gợi ý tối đa 5 emoji.

### Gợi ý Ollama và từ điển cá nhân

Bật **AI gợi ý** trong Cài đặt. Bộ gõ chờ bạn ngừng gõ 350 ms rồi gửi ngữ cảnh và từ đang gõ tới Ollama. Gõ tiếp sẽ đặt lại bộ đếm, hủy yêu cầu cũ và loại bỏ kết quả cũ. Mỗi lượt chỉ gọi một yêu cầu chạy nền; Telex/VNI không chờ AI. Không còn nguồn gợi ý vector hoặc dự phòng vector khi Ollama không phản hồi. Emoji vẫn dùng danh sách cục bộ.

Từ trong gợi ý Ollama mà bạn chủ động chọn được lưu vào `~/.config/gotiengviet/learned-words.txt`. Bộ gõ nhận diện các từ này khi kiểm tra chính tả, không phân biệt hoa/thường. Chỉ nhận gợi ý chưa làm thay đổi từ điển; từ được lưu sau khi bạn chọn. Có thể sửa/xóa file rồi khởi động lại bộ gõ để bỏ từ đã học. Đây là từ điển cá nhân, không phải huấn luyện lại model Ollama.

## Ctrl+T: viết lại và dịch với Ollama (Linux)

Nhấn **Ctrl+T** khi GoTiengViet đang hoạt động: bộ gõ tự lấy đoạn đang chọn hoặc nội dung trước con trỏ cùng chữ đang gõ dở, rồi xử lý theo lựa chọn gần nhất. Kết quả hiện ngay tại ô nhập trong bảng của IBus, không chuyển focus sang cửa sổ khác. **Enter** thay câu gốc, **Esc** hủy, **Tab** đổi giữa viết lại và dịch rồi xử lý lại. Nội dung và con trỏ phải còn khớp với lúc bắt đầu; nếu bạn đã sửa câu thì không ghi đè.

Khi nhấn Enter, bộ gõ ưu tiên AT-SPI: xác định ô đang hoạt động, so khớp câu gốc với văn bản thực, chọn đúng đoạn rồi chèn kết quả qua IBus. Sau đó đọc lại để xác nhận việc thay thế. Không dựa vào chuỗi Backspace hoặc thời gian chờ để đoán đã xóa đủ ký tự. Nếu không thể chọn/xác nhận bằng AT-SPI, chỉ dùng API surrounding text của IBus khi nội dung và vị trí vẫn khớp; không tự xóa phỏng đoán.

Trường mật khẩu/PIN không kích hoạt trợ lý. Ctrl+T được bộ gõ xử lý trong ô nhập; một số ứng dụng có thể giữ phím này trước IBus (ví dụ thao tác mở tab mới), nên không bảo đảm hoạt động trong mọi ứng dụng.

Mặc định là **Viết lại**. Chạy `gotiengviet-assistant` để chọn **Dịch** và ngôn ngữ đích; thao tác/ngôn ngữ được nhớ trong `assistant.conf`, không lưu nội dung câu vào file này. Cửa sổ cho sửa kết quả; Shift+Enter xuống dòng, Esc hủy.

Dùng model và URL Ollama trong Cài đặt GoTiengViet; Ollama cần chạy và model được tải sẵn. Yêu cầu chạy nền, tối đa 120 giây. Ctrl+T gửi nội dung tới URL đã cấu hình theo thao tác gần nhất. Chất lượng phụ thuộc model.

Ctrl+T là phím tắt của engine IBus, không phải phím tắt toàn hệ thống. Có thể chạy `gotiengviet-assistant` khi dùng English (US) hoặc khi desktop giữ tổ hợp phím này; ở chế độ mở trực tiếp, Enter sao chép kết quả.

Kiểm thử: `make test-ibus` và `make test-assistant` (cần `broadwayd` nếu không có desktop).

## Cấu trúc mã nguồn

```text
engine/
  engine.h          API công khai và kiểu dữ liệu
  engine.c          xử lý chuỗi, buffer, backspace và commit
  compose.c         bảng phím + thuật toán chung Telex/VNI
  charset.c         bảng ký tự Unicode và phép biến đổi dấu
  phonology.c       vị trí đặt dấu thanh (modern/truyền thống)
  promotion.c       quy tắc nguyên âm ie/ye/uo
  config.c          đọc/lưu cấu hình dùng chung
  spell.c           kiểm tra và gợi ý chính tả
  macro.c           mở rộng macro/emoji
  ai.c, json.c      provider Ollama và JSON
  text.c            chuyển UTF-8/UCS-4
  telex.c, vni.c    adapter mỏng vào thuật toán chung
ibus/
  engine.c          vòng đời, phím và giao tiếp IBus
  text_target.c/h   chọn/xác nhận đoạn thay thế qua AT-SPI
  gotiengviet.xml   khai báo component IBus
cmd/
  setup/main.c      GTK, indicator và cấu hình CLI
  demo/main.c       demo terminal + batch
  assistant/main.c  cửa sổ viết lại/dịch Ollama
tests/              toàn bộ kiểm thử C (+ fake curl/assistant)
windows/            hook bàn phím, tray và setup cho Windows
```

Các script shell chỉ phục vụ build, cài đặt và đóng gói. Không có mã nguồn ứng dụng Go, bridge CGO hoặc phụ thuộc toolchain Go.

## API thư viện C

Liên kết với `build/libgotiengviet.a` và `#include "engine/engine.h"`. Chuỗi/mảng trả về thuộc về caller (`g_free`/`g_ptr_array_unref`).

```c
gtv_init();
gchar *out = gtv_transform("duocjwd", GTV_TELEX, TRUE); // "được"

GtvConfig cfg;
gtv_config_load(&cfg, "/home/ban/.config/gotiengviet");
GtvEngine *e = gtv_engine_new(&cfg);
guint backspaces = 0;
gchar *commit = gtv_engine_process(e, 's', &backspaces); // NULL khi đang soạn
g_free(gtv_engine_buffer(e));
gtv_engine_free(e);

GPtrArray *fix = gtv_ai_suggest(&cfg, "kông", "công nghệ");   // cần Ollama, không thì rỗng
GPtrArray *next = gtv_predict_next(&cfg, "công nghệ", "thong"); // cần Ollama, không thì rỗng
gboolean ok = spell_word_valid("được"); // kiểm tra ngoại tuyến, không cần mạng
gtv_config_clear(&cfg);
```

Luồng bất đồng bộ cho UI: `gtv_suggest_combined_async()` / `gtv_suggest_combined_finish()` (dùng trong IBus để không chặn phím).

## Xử lý sự cố

- **Không thấy GoTiengViet trong Input Sources:** chạy `ibus restart`, đăng xuất/đăng nhập lại, kiểm tra `/usr/share/ibus/component/gotiengviet.xml` tồn tại.
- **Gõ không ra dấu:** xác nhận đang ở engine `gotiengviet` (không phải English US), kiểm tra `Super+Space` và `method` trong `~/.config/gotiengviet/config`.
- **Muốn gõ chữ điều khiển thô** (`test`, `pass`, `as`): gõ lặp ký tự (`tesst`, `passs`, `ass`) hoặc tạm chuyển sang English.
- **Ctrl+T không phản hồi:** thử trong ô nhập khác (một số app giữ Ctrl+T), kiểm tra Ollama/model đã chạy, xem log `/tmp/gotiengviet_debug.log`.
- **Crash:** xem `~/.cache/gotiengviet/crash.log`; báo lỗi kèm phiên bản `.deb` và đoạn log liên quan.
- **Kiểm tra không chạm hệ thống:** `DESTDIR=/tmp/gotiengviet-staging ./install.sh`.

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

Giấy phép: [MIT](LICENSE).
