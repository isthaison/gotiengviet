# Phát triển

## Build và kiểm thử (mọi nền tảng — một cửa lệnh)

```sh
./gtv.sh build        # Linux: make build test | Win: Makefile.win | mac: core + app
./gtv.sh test         # chạy test suite của OS hiện tại
./gtv.sh vet          # build + test nghiêm (-Werror nếu hỗ trợ)
./gtv.sh clean        # xóa build/ và release-pkg/
# Windows cmd (MSYS2/Git Bash đằng sau): gtv build | test | vet | clean
```

Linux giữ `make ...` trực tiếp vẫn dùng được:

```sh
make build                         # 3 binary + libgotiengviet.a
make test                          # engine + support + thuật toán
make vet                           # build + test với -Werror
make test-ibus                     # vòng đời IBus
make test-ui                       # giao diện setup
make help
```

Mọi lệnh shell gom trong một file duy nhất: `./gtv.sh <build|test|vet|clean|install|uninstall|package|bump|help>`.

`make test` gồm: Telex/VNI, stateful, config, JSON, Ollama (curl giả lập), chính tả, macro/emoji, prompts, config mặc định, từ điển học, so version/cập nhật, hoán vị thao tác, 20.000 chuỗi ngẫu nhiên.

Binary trong `build/`: `ibus-engine-gotiengviet` (`--ibus` trong IBus), `ibus-setup-gotiengviet` (GUI/`--tray`/`--cli`), `gotiengviet-demo`, `libgotiengviet.a`.

## Cài đặt, gỡ và đóng gói

```sh
sudo ./gtv.sh install              # Linux: chép file hệ thống + ibus restart
sudo ./gtv.sh uninstall            # Linux: gỡ đúng danh sách install (giữ ~/.config)
./gtv.sh package [VERSION]         # Linux: .deb
make package VERSION=X.Y-1         # tương đương (Linux)
```

Windows (MSYS2 MinGW, hoặc `gtv ...` từ cmd):

```sh
./gtv.sh package                   # staging + iscc -> gotiengviet-X.Y.Z-x64-setup.exe
./gtv.sh install                   # dựng setup (nếu thiếu) + cài silent + chạy app
./gtv.sh uninstall                 # gỡ silent + dọn Run/task/profile thừa
```

macOS: `./gtv.sh package` (zip `GoTiengViet.app`), `./gtv.sh install` (chép vào
`~/Library/Input Methods`), `./gtv.sh uninstall` (xóa app, giữ `~/.config`).

Giữ nguyên cấu hình người dùng; cuối cài đặt tự `ibus restart` từng phiên desktop (báo chứ không fail nếu không được). Thử không chạm hệ thống: `DESTDIR=/tmp/gotiengviet-staging ./gtv.sh install`. `make clean` chỉ xóa `build/`. Gỡ thủ công: xóa `/usr/libexec/ibus-engine-gotiengviet`, `/usr/libexec/ibus-setup-gotiengviet`, `/usr/bin/gotiengviet*`, `/usr/share/ibus/component/gotiengviet.xml`, rồi `ibus restart`.

## Cấu trúc mã nguồn

```text
engine/           lõi dùng chung (C): compose, charset, phonology, config,
                  spell, macro/emoji, ai+ollama, json, learn, update
                  (check version GitHub), text, telex/vni adapter
linux/
  ibus/           adapter IBus: engine.c, gotiengviet.xml/icons
  setup/          GTK, indicator, CLI
windows/          app khay + setup + update (C: main/app/tray/tray_ai/
                  setup/update/tsf_install/win_utf) + tsf/ (C++ TSF Text Service)
macos/            IME InputMethodKit: controller, main, Info.plist (bundle),
                  SetupWindowController (cửa sổ Dữ liệu Macro/Emoji)
site/             trang chủ GitHub Pages (HTML/CSS tĩnh + SEO meta),
                  deploy bằng .github/workflows/pages.yml
.github/          CI: build-linux, build-windows, build-macos (test lõi +
                  đóng gói .deb/.exe/.app), pages (deploy site)
mk/common.mk      danh sách file dùng chung (ENGINE_SRCS/WIN_SRCS/TSF_SRCS)
VERSION           số version duy nhất (Makefile/Makefile.win đọc từ đây;
                  bump bằng ./gtv.sh bump, không sửa tay)
tools/
  demo/           demo terminal dùng chung mọi nền tảng
tests/            kiểm thử C (+ fake curl)
data/             macro, emoji, config/ai mẫu, prompts, seed từ điển
```

Quy ước đa nền tảng: `engine/` không chứa code riêng OS nào
(chỉ glib/gio); mỗi OS là một adapter mỏng trong `linux/`, `windows/`,
`macos/` gọi vào engine. Thêm file nguồn mới thì khai báo ở
`mk/common.mk` (Windows) — Linux tự glob `engine/*.c`.

Mọi lệnh shell gom trong `./gtv.sh` duy nhất (`build|test|vet|clean|install|package|bump|help`).

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
gboolean ok = spell_word_valid("được"); // ngoại tuyến
gint cmp = gtv_version_compare("v0.6.0", "0.5.3"); // > 0
gtv_config_clear(&cfg);
```

Luồng bất đồng bộ cho UI: `gtv_suggest_combined_async()` / `gtv_suggest_combined_finish()`.

## Cắt release

1. Bump một lệnh (đừng sửa tay từng file — script là nơi duy nhất ghi version): `make bump V=X.Y.Z ["ghi chú"]` (chạy `./gtv.sh bump`). Kiểm tra `git diff`, rồi commit.
2. Commit, push tag `vX.Y.Z`.
3. CI Windows build `gotiengviet-X.Y.Z-x64-setup.exe` (ký bằng SignPath.io — xem `.signpath/`),
   CI Linux build `gotiengviet_X.Y.Z-1_<arch>.deb`, CI macOS build `.dmg` + `.pkg`, cả ba đính kèm release để app tự cập nhật.
4. Linux: khay hệ thống có mục **Kiểm tra cập nhật...** (tự kiểm tra mỗi ngày, báo qua notify khi có bản mới) — tải `.deb` đúng kiến trúc rồi cài qua `pkexec dpkg -i`, xong tự `ibus restart`. Cần `curl`.

### SignPath code signing (Windows)

File cài Windows ký bằng [SignPath.io](https://signpath.io) (miễn phí cho open source). Cần cấu hình một lần:

1. Đăng ký tài khoản ở https://signpath.io → tạo Organization.
2. Cài [SignPath GitHub App](https://github.com/apps/signpath) → cho phép repo `isthaison/gotiengviet`.
3. Trong SignPath: tạo Project `GoTiengViet`, linked repo, artifact config `.signpath/artifact-configurations/default.xml`.
4. Tạo signing policy `release-signing` (origin verification, restricted to `main` + tag `v*`).
5. Tạo API Token (Submitter role) → thêm vào GitHub repo secrets tên `SIGNPATH_API_TOKEN`.
6. Thêm GitHub repo variable `SIGNPATH_ORGANIZATION_ID` = organization ID từ SignPath.
7. Push tag → CI sẽ build unsigned → upload → SignPath ký → release asset là file đã ký.

---
Xem thêm: [README](../README.md) · [Cấu hình](config.md) · [Bản Windows](windows.md) ·
[Bản macOS](macos.md) · [Trang chủ](https://isthaison.github.io/gotiengviet/)
