# Phát triển

## Build và kiểm thử (Linux)

```sh
./build.sh                         # = make build test
make build                         # 3 binary + libgotiengviet.a
make test                          # engine + support + thuật toán
make vet                           # build + test với -Werror
make test-ibus                     # vòng đời IBus
make test-ui                       # giao diện setup
make help
```

`make test` gồm: Telex/VNI, stateful, config, JSON, Ollama (curl giả lập), chính tả, macro/emoji, prompts, config mặc định, từ điển học, so version/cập nhật, mirror màn hình, hoán vị thao tác, 20.000 chuỗi ngẫu nhiên.

Binary trong `build/`: `ibus-engine-gotiengviet` (`--ibus` trong IBus), `ibus-setup-gotiengviet` (GUI/`--tray`/`--cli`), `gotiengviet-demo`, `libgotiengviet.a`.

## Cài đặt và đóng gói (Linux)

```sh
sudo ./install.sh
make package VERSION=X.Y-1
```

Giữ nguyên cấu hình người dùng; cuối cài đặt tự `ibus restart` từng phiên desktop (báo chứ không fail nếu không được). Thử không chạm hệ thống: `DESTDIR=/tmp/gotiengviet-staging ./install.sh`. `make clean` chỉ xóa `build/`. Gỡ thủ công: xóa `/usr/libexec/ibus-engine-gotiengviet`, `/usr/libexec/ibus-setup-gotiengviet`, `/usr/bin/gotiengviet*`, `/usr/share/ibus/component/gotiengviet.xml`, rồi `ibus restart`.

## Cấu trúc mã nguồn

```text
engine/           lõi dùng chung (C): compose, charset, phonology, config,
                  spell, macro/emoji, ai+ollama, json, learn, update
                  (check version GitHub), mirror (mirror màn hình cho
                  client pass-through), text, telex/vni adapter
ibus/             adapter IBus: engine.c, gotiengviet.xml
cmd/setup         GTK, indicator, CLI  |  cmd/demo  demo terminal
tests/            kiểm thử C (+ fake curl)
windows/          hook, tray, setup, update, tsf_mode (C) + tsf/ (C++ TSF)
data/             macro, emoji, config/ai mẫu, prompts, seed từ điển
```

Script shell chỉ build/cài/đóng gói.

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

1. Bump version cùng số ở `windows/version.h`, `windows/resource.rc`, `ibus/gotiengviet.xml` (kèm `engine.c` fallback, `metainfo.xml`, default `Makefile[.win]`, `installer.iss`, `package.sh`).
2. Commit, push tag `vX.Y.Z`.
3. CI Windows build `gotiengviet-X.Y.Z-x64-setup.exe`, đính kèm release → app Windows tự cập nhật. Linux đóng `.deb` bằng `make package` và đính kèm tay nếu cần.
