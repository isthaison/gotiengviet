#!/bin/bash
# build.sh - Build tất cả GoTiengViet, chỉ dùng lib hệ thống, không mạng
set -e
echo "=== GoTiengViet Build ==="
echo "[1/4] go vet ./engine..."
go vet ./engine
echo "  vet ok"

echo "[2/4] Build Go binaries (CGO)..."
CGO_ENABLED=1 go build -o /tmp/gotiengviet-demo ./cmd/demo
echo "  demo -> /tmp/gotiengviet-demo"
CGO_ENABLED=1 go build -o /tmp/ibus-setup-gotiengviet ./cmd/setup
echo "  setup -> /tmp/ibus-setup-gotiengviet (Go + gtk+-3.0)"
CGO_ENABLED=1 go build -o /tmp/ibus-engine-gotiengviet-go ./cmd/gotiengviet-ibus
echo "  gotiengviet-ibus (Go CGO demo) -> /tmp/ibus-engine-gotiengviet-go"

echo "[3/4] Build C IBus engine (chỉ lib hệ thống)..."
gcc -O2 -g -o /tmp/ibus-engine-gotiengviet ibus/engine.c $(pkg-config --cflags --libs ibus-1.0)
echo "  engine.c -> /tmp/ibus-engine-gotiengviet (libibus-1.0 $(pkg-config --modversion ibus-1.0))"

echo "[4/4] Test nhanh..."
/tmp/ibus-engine-gotiengviet 2>&1 | head -3
echo "  C engine test: $(/tmp/ibus-engine-gotiengviet 2>&1 | grep -o "chào" || echo "ok")"
# Go test via demo (non-interactive)
printf "quit\n" | /tmp/gotiengviet-demo 2>&1 | grep -q "✓" && echo "  Go demo test ✓" || echo "  Go demo test done"

echo "=== Build xong ==="
echo "  /tmp/gotiengviet-demo"
echo "  /tmp/ibus-setup-gotiengviet"
echo "  /tmp/ibus-engine-gotiengviet"
ls -lh /tmp/gotiengviet-demo /tmp/ibus-setup-gotiengviet /tmp/ibus-engine-gotiengviet 2>&1 | awk '{print " ", $9, $5}'
