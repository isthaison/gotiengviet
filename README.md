# GoTiengViet

Bộ gõ tiếng Việt cho Linux (và Windows), viết hoàn toàn bằng **C** — build và chạy không cần Go hay CGO.

Telex/VNI dùng chung một lõi, IBus adapter, GUI cấu hình GTK 3, indicator, demo terminal, kiểm tra chính tả ngoại tuyến, macro/emoji, gợi ý và sửa lỗi bằng Ollama (tùy chọn), trợ lý viết lại/dịch Ctrl+T.

## Mục lục

- [Tính năng chính](#tính-năng-chính)
- [Bắt đầu nhanh](#bắt-đầu-nhanh)
- [Quy tắc gõ](#quy-tắc-gõ)
- [Build và kiểm thử](#build-và-kiểm-thử)
- [Cài đặt và đóng gói](#cài-đặt-và-đóng-gói)
- [Tham chiếu lệnh](#tham-chiếu-lệnh)
- [Cấu hình](#cấu-hình)
- [Macro và emoji](#macro-và-emoji)
- [Chính tả và gợi ý](#chính-tả-và-gợi-ý)
- [Ctrl+T: viết lại và dịch (Linux)](#ctrlt-viết-lại-và-dịch-linux)
- [Cấu trúc mã nguồn](#cấu-trúc-mã-nguồn)
- [API thư viện C](#api-thư-viện-c)
- [Xử lý sự cố](#xử-lý-sự-cố)
- [Phiên bản Windows](#phiên-bản-windows)

## Tính năng chính

- Một thuật toán chung cho Telex và VNI (`engine/compose.c`), chỉ khác bảng ánh xạ phím; hai kiểu đặt dấu hiện đại/truyền thống.
- Kiểm tra chính tả ngoại tuyến bằng quy tắc âm tiết, gạch đỏ trực tiếp khi gõ.
- Gợi ý sửa lỗi và hoàn thành từ bằng Ollama (debounce 350 ms, hủy khi gõ tiếp); khi mất mạng vẫn gợi ý từ dữ liệu đã học.
- Từ điển học: từ và cặp sửa lỗi do bạn chọn từ gợi ý Ollama được nhớ lại, dùng offline từ lần sau.
- Macro gõ tắt và emoji đọc từ file text, người dùng tự thêm được.
- Trợ lý Ctrl+T viết lại/dịch văn bản ngay trong ô nhập (Linux).
- Mọi bảng dữ liệu (macro, emoji, config mẫu, prompt, seed từ điển) đều là file trong `data/`, không hardcode.

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
3. Mở **GoTiengViet** để chọn Telex/VNI, kiểu đặt dấu, chính tả, AI.
4. Kiểm tra nhanh: `build/gotiengviet-demo --transform telex duocjwd` phải ra `được`.

## Quy tắc gõ

Telex và VNI chỉ khác **bảng ánh xạ phím**, cùng đi qua một thuật toán:

1. Xác định âm đầu, cụm nguyên âm và phụ âm cuối của phần đang gõ.
2. Ánh xạ phím thành thao tác: đặt thanh, đổi dạng chữ, xóa thanh hoặc gõ tắt.
3. Dấu có thể gõ trước hoặc sau phụ âm cuối; chữ `d` đầu âm tiết đổi thành `đ` bằng phím gõ ở cuối.
4. Dạng chữ và dấu thanh đặt độc lập, giữ nguyên hoa/thường; cặp `uo/uô` là một đích của dấu móc, đổi thành `ươ`.
5. Gõ lại dấu đang có: **bỏ dấu đó và thêm đúng một phím thô**; đổi sang dấu khác thì thay dấu cũ.

Không có ngoại lệ theo từ, không đoán/khôi phục tiếng Anh trong thuật toán gõ. `spell.c` chỉ kiểm tra cấu trúc âm tiết, không quyết định kết quả phím gõ.

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
| `aaa` / `a66` | `aa` / `a6` |
| `uoww` / `uo77` | `uow` / `uo7` |

**Chủ đích:** `test` ra `tét`, `pass` ra `pas` (không khôi phục tiếng Anh). Muốn chữ điều khiển thô thì gõ lặp (`tesst → test`, `passs → pass`) hoặc chuyển nguồn bằng `Super+Space`. Phím `w` khi chưa có nguyên âm thêm `ư`; lặp phím tắt hoàn tác (`ww → w`). `[`/`]` và `{`/`}` thêm `ươ`/`ư` (và chữ hoa); lặp trả lại dấu ngoặc.

Kiểu đặt dấu `modern`: `true` (mặc định) đặt dấu ở nguyên âm thứ hai của `oa/oe/uy` (`hòa`); `false` đặt ở nguyên âm đầu (`hoà`).

## Build và kiểm thử

Cần đúng bộ dependency ở [Bắt đầu nhanh](#bắt-đầu-nhanh).

```sh
./build.sh                         # = make build test
make build                         # 4 binary + libgotiengviet.a
make test                          # engine + support + thuật toán
make vet                           # build + test với -Werror
make test-ibus                     # vòng đời IBus, trợ lý, xóa-xác minh
make test-assistant                # cửa sổ trợ lý (cần broadwayd nếu headless)
make test-ui                       # giao diện setup
make test-text-target-live         # opt-in: ô nhập thật để kiểm tra AT-SPI
make help
```

`make test` gồm: gõ Telex/VNI, stateful, config, JSON, Ollama (qua curl giả lập, không cần mạng/model), chính tả, macro/emoji, prompts, config mặc định, hoán vị mọi thứ tự thao tác trên cùng âm tiết, và 20.000 chuỗi ngẫu nhiên.

Các binary trong `build/`: `ibus-engine-gotiengviet` (`--ibus` để chạy trong IBus), `ibus-setup-gotiengviet` (GUI/`--tray`/`--cli`), `gotiengviet-assistant`, `gotiengviet-demo`, `libgotiengviet.a`.

## Cài đặt và đóng gói

```sh
sudo ./install.sh
make package VERSION=0.3.0-1
```

- Dùng binary đã build trong `build/`; giữ nguyên cấu hình người dùng.
- Cuối cài đặt tự `ibus restart` trong từng phiên desktop đang hoạt động và kiểm tra engine đã đăng ký; không thấy session hoặc thất bại thì chỉ báo, không fail cài đặt.
- Kiểm tra không cần root: `DESTDIR=/tmp/gotiengviet-staging ./install.sh`.
- `make clean` chỉ xóa `build/`. Gỡ thủ công: xóa `/usr/libexec/ibus-engine-gotiengviet`, `/usr/libexec/ibus-setup-gotiengviet`, `/usr/bin/gotiengviet*`, `/usr/share/ibus/component/gotiengviet.xml`, rồi `ibus restart`.

## Tham chiếu lệnh

```sh
build/gotiengviet-demo --transform telex duocjwd
build/gotiengviet-demo --transform vni duoc579
build/gotiengviet-demo --suggest kông ["công nghệ"]
build/gotiengviet-demo --predict "công nghệ" [thong]
build/gotiengviet-demo            # tương tác: mode telex | mode vni | quit

ibus-setup-gotiengviet            # GUI (tự bật tray nền)
ibus-setup-gotiengviet --tray     # chỉ indicator (cần DISPLAY/WAYLAND_DISPLAY)
ibus-setup-gotiengviet --cli      # terminal, Enter giữ giá trị cũ

gotiengviet-assistant [--stdin] [--headless] [--replace]
```

`--stdin` đọc từ stdin, `--headless` in kết quả ra stdout thay vì mở cửa sổ, `--replace` cho Enter thay câu gốc khi được IBus gọi.

## Cấu hình

Thư mục cấu hình: `$XDG_CONFIG_HOME/gotiengviet/` (mặc định `~/.config/gotiengviet/`).

| File | Nội dung |
|---|---|
| `config` | Kiểu gõ, kiểu đặt dấu, chính tả, tùy chọn AI |
| `ai.conf` | Provider `rule`/`ollama`, model, URL, port — ưu tiên hơn mục `[ai]` trong `config` |
| `assistant.conf` | Thao tác gần nhất (viết lại/dịch) + ngôn ngữ đích; không lưu nội dung câu |
| `macros.txt`, `emojis.txt` | Bảng riêng, thay thế hoàn toàn file hệ thống |
| `learned-words.txt`, `learned-corrections.txt` | Từ điển học (tự tạo, sửa/xóa được) |
| `prompts.conf` | Mẫu prompt Ollama riêng |

Khi chưa có file người dùng, bộ gõ dùng mặc định đi kèm trong `data/` (được cài vào `/usr/share/gotiengviet/`). Muốn tùy biến: copy file từ đó về thư mục cấu hình rồi sửa. Thứ tự tìm file dữ liệu: `GTV_DATA_DIR` (test/dev) → file người dùng → `/usr/share/gotiengviet/` → `./data` (trên Windows thêm `data/` cạnh exe).

Ví dụ `config` / `ai.conf` mặc định: Telex, `modern=true`, `spellcheck=true`, AI tắt (`rule`), model `qwen2:0.5b`, URL `http://localhost:55602`.

### Mẫu prompt Ollama

`prompts.conf` có các placeholder `%s` điền theo thứ tự:

```ini
[suggest]             # %s = ngữ cảnh, từ đang gõ, gợi ý sửa lỗi
prompt=...            # correct_hint: từ bị đánh dấu sai; complete_hint: hoàn thành từ
[assistant_rewrite]   # %s = đoạn văn
[assistant_translate] # %s = ngôn ngữ đích, đoạn văn
```

Mọi `%` khác giữ nguyên (viết `%%` cho dấu phần trăm), `\n` là xuống dòng. Thiếu mẫu nào, yêu cầu đó trả rỗng/báo lỗi thay vì dùng chữ cứng.

## Macro và emoji

Macro mở rộng khi kết thúc từ: `vn`→`Việt Nam`, `hn`→`Hà Nội`, `dc`→`được`, `ko`→`không`, `ntn`→`như thế nào`, … Emoji: `:smile:`→😊, `:thumbsup:`→👍, `:)`→😊, `<3`→❤️, … Đang gõ tiền tố `:` (ví dụ `:sm`) thì IBus gợi ý tối đa 5 emoji.

Cả hai đọc từ file text (`key=value` mỗi dòng, tách ở dấu `=` đầu, `#` là chú thích, UTF-8, trùng key lấy dòng đầu, giữ thứ tự file). Thêm macro riêng bằng cách tạo `~/.config/gotiengviet/macros.txt`:

```ini
cty=Công ty TNHH
```

Sửa/xóa file rồi khởi động lại bộ gõ để nhận bảng mới.

## Chính tả và gợi ý

Ba lớp phối hợp, không chặn phím gõ:

1. **Gạch đỏ ngoại tuyến** (`spell_word_valid`): thuần quy tắc âm tiết — âm đầu, vần, phụ âm cuối, thanh — cộng từ điển học. Chữ chưa gõ dấu luôn cho qua để không gạch oan khi đang gõ dở.
2. **Gợi ý Ollama** (bật trong Cài đặt): ngừng gõ 350 ms mới gửi ngữ cảnh + từ đang gõ; gõ tiếp thì hủy yêu cầu cũ. Tắt AI hoặc mất Ollama thì không có gợi ý online.
3. **Từ điển học** (`learned-words.txt`, `learned-corrections.txt`): chỉ những gợi ý bạn **chủ động chọn** mới được nhớ — từ đúng vào danh sách từ, cặp sai→đúng vào map sửa lỗi. Lần sau: từ sai cũ bị gạch đỏ ngay không cần mạng; khi Ollama rớt, IBus vẫn gợi ý từ dữ liệu đã học (sửa lỗi đã nhớ, hoặc hoàn thành tiền tố kể cả không dấu như `thong`→`thông`). Kèm app có sẵn seed (`data/learned-words.txt` 489 từ, `data/learned-corrections.txt` lỗi kinh điển) để có tác dụng từ lần đầu; lần lưu đầu tiên copy seed vào file riêng của bạn, từ đó file riêng là chuẩn.

Ollama là tùy chọn, cần `curl`. Request đi bằng stdin/argv (không ghép shell), có timeout và giới hạn kích thước response. Test dùng curl giả lập, không cần mạng hay model thật.

```sh
build/gotiengviet-demo --suggest kông   # cần Ollama, không thì rỗng
build/gotiengviet-demo --predict "công nghệ" thong
```

## Ctrl+T: viết lại và dịch (Linux)

Nhấn **Ctrl+T** khi đang gõ: bộ gõ lấy đoạn đang chọn (hoặc nội dung trước con trỏ + chữ gõ dở), xử lý theo lựa chọn gần nhất, hiện kết quả ngay tại ô nhập. **Enter** thay câu gốc (nhấn lặp/giữ Enter cũng chỉ chạy một lượt — chống chèn đúp và xóa lố), **Esc** hủy, **Tab** đổi viết lại/dịch. Nội dung/con trỏ đổi rồi thì không ghi đè.

Khi Enter, câu gốc bị xóa bằng surrounding text đã xác minh trước, rồi mới tới AT-SPI select và cuối cùng là Backspace đúng số ký tự. Mọi nhánh đều **xác nhận câu gốc đã mất mới chèn** — đọc độc lập 2 kênh (surrounding + AT-SPI), kênh nào mâu thuẫn là dừng; snapshot rỗng chỉ được tin khi con trỏ cũng đứng đúng điểm xóa (terminal báo text rỗng mà không xóa thật sẽ bị bắt). Đoạn cần xóa đổi giữa chừng thì dừng. Terminal thường lờ lệnh xóa nên tự rơi vào nhánh Backspace đã xác minh thay vì chèn thêm. Kẹt hẳn thì báo lỗi, giữ kết quả cho Enter thử lại.

Ô mật khẩu/PIN không kích hoạt. Ctrl+T khi không có chữ nào báo ngay thay vì treo; mỗi lượt chạy giới hạn 150 giây nên Enter không bao giờ kẹt ở "đang xử lý". Một số app giữ Ctrl+T trước IBus (ví dụ mở tab mới) nên không đảm bảo mọi nơi. Mặc định **Viết lại**; chạy `gotiengviet-assistant` để chọn **Dịch** + ngôn ngữ đích (nhớ trong `assistant.conf`). Ollama phải chạy sẵn, model đã tải; mỗi yêu cầu curl tối đa 120 giây. Mở tay `gotiengviet-assistant` khi dùng layout English thì Enter sao chép kết quả.

## Cấu trúc mã nguồn

```text
engine/
  engine.h          API công khai và kiểu dữ liệu
  engine.c          chuỗi, buffer, backspace và commit
  compose.c         bảng phím + thuật toán chung Telex/VNI
  charset.c         bảng ký tự Unicode và biến đổi dấu
  phonology.c       vị trí đặt dấu (modern/truyền thống)
  promotion.c       quy tắc ie/ye/uo
  config.c          cấu hình + resolver/prompts dùng chung
  spell.c           kiểm tra chính tả (quy tắc âm tiết)
  macro.c           nạp bảng macro/emoji từ file
  ai.c, json.c      Ollama và JSON
  learn.c           từ điển học dùng chung (words/fixes, mọi nền tảng)
  text.c            chuyển UTF-8/UCS-4
  telex.c, vni.c    adapter mỏng vào thuật toán chung
data/
  macros.txt, emojis.txt            bảng đi kèm (/usr/share/gotiengviet/)
  learned-words.txt                 seed 489 từ phổ thông
  learned-corrections.txt           seed lỗi Telex/VNI kinh điển
  config, ai.conf, assistant.conf   cấu hình mặc định
  prompts.conf                      mẫu prompt [suggest]/[assistant_*]
ibus/
  engine.c          vòng đời, phím, gợi ý, thay thế đã xác minh
  text_target.c/h   chọn/đọc ô nhập qua AT-SPI
  gotiengviet.xml   khai báo component IBus
cmd/setup/main.c    GTK, indicator, CLI  |  cmd/demo/main.c    demo
cmd/assistant/main.c  cửa sổ viết lại/dịch
tests/              kiểm thử C (+ fake curl/assistant)
windows/            hook, tray, setup cho Windows
```

Script shell chỉ build/cài/đóng gói. Không mã Go, không CGO, không toolchain Go.

## API thư viện C

Liên kết `build/libgotiengviet.a`, `#include "engine/engine.h"`. Chuỗi/mảng trả về thuộc về caller (`g_free`/`g_ptr_array_unref`).

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

GPtrArray *fix = gtv_ai_suggest(&cfg, "kông", "công nghệ");    // cần Ollama
GPtrArray *next = gtv_predict_next(&cfg, "công nghệ", "thong"); // cần Ollama
gboolean ok = spell_word_valid("được"); // ngoại tuyến
gtv_config_clear(&cfg);
```

Luồng bất đồng bộ cho UI: `gtv_suggest_combined_async()` / `gtv_suggest_combined_finish()`.

## Xử lý sự cố

- **Không thấy GoTiengViet trong Input Sources:** `ibus restart`, đăng xuất/đăng nhập lại, kiểm tra `/usr/share/ibus/component/gotiengviet.xml`.
- **Gõ không ra dấu:** đang ở engine `gotiengviet` (không phải English US)? Kiểm tra `Super+Space` và `method` trong `~/.config/gotiengviet/config`.
- **Muốn chữ điều khiển thô** (`test`, `pass`): gõ lặp (`tesst`, `passs`) hoặc tạm chuyển English.
- **Ctrl+T không phản hồi:** thử ô nhập khác, kiểm tra Ollama/model, xem `~/.cache/gotiengviet/debug.log`.
- **Ctrl+T trong terminal, Enter chèn thêm:** cập nhật bản mới rồi `ibus restart`. Còn lỗi thì gửi các dòng `[assistant-state]` trong log (cho biết đã đi nhánh AT-SPI, surrounding hay Backspace).
- **Crash:** xem `~/.cache/gotiengviet/crash.log`, báo kèm phiên bản `.deb` và log liên quan.
- **Thử cài đặt không chạm hệ thống:** `DESTDIR=/tmp/gotiengviet-staging ./install.sh`.

## Phiên bản Windows

Native Windows 10/11 qua Win32 Low-Level Keyboard Hook (`WH_KEYBOARD_LL`) + System Tray:

* **Gói portable**: mỗi release GitHub đính kèm `gotiengviet-windows-x64.zip` (chạy ngay, `data/` nằm cạnh exe).
* **Đổi ngôn ngữ**: `Ctrl + Shift` / `Alt + Z` (chống lặp khi giữ phím), hoặc click trái icon [V]/[E] (chế độ được nhớ qua restart); chuột phải mở menu: Bảng điều khiển, Telex/VNI, chính tả, chuẩn dấu, tự khởi động, Thoát.
* **Bảng điều khiển**: kiểu gõ, chuẩn dấu, chính tả, tự khởi động, cụm AI (bật/tắt Ollama, model, URL) — lưu ở `%APPDATA%/gotiengviet/` như bản Linux.
* **Gợi ý AI dạng balloon**: từ sai cấu trúc sau khi gõ xong được hỏi Ollama nền, hiện `"sai" co the ban muon go "dung"?` (không dấu cho mọi locale, chống spam 10 giây). Cần Ollama + `curl` (Windows 10+ có sẵn). **Click vào balloon** để nhận gợi ý: văn bản còn nguyên thì tự xóa từ sai và gõ chữ đúng, gõ tiếp rồi thì copy chữ đúng vào clipboard; cả hai đều học mapping vào `learned-corrections.txt`, nên Ollama rớt vẫn gợi ý từ dữ liệu đã học. Từ điển học dùng chung định dạng và seed với bản Linux (module `engine/learn.c`).
* **Macro/emoji/prompts/config**: chung engine và file `data/`; file người dùng vẫn ưu tiên. Typo đã học/seed (`hoăc`, `kông`, …) vẫn balloon ngay cả khi Ollama rớt, click để nhận + học tiếp.
* **Build (MSYS2 MinGW-w64)**: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 make`, rồi `make -f Makefile.win`.

Chưa có: gợi ý inline trong ô nhập, cửa sổ Ctrl+T. Lõi gõ, cấu hình, macro/emoji, kiểm tra âm tiết, gợi ý AI (kèm nhận + học + offline) đã ngang Linux.

Giấy phép: [MIT](LICENSE).
