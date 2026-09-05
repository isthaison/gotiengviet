.PHONY: all build install clean package test vet

VERSION ?= 0.1.0-1
PREFIX ?= /usr

all: build

build:
	@echo "=== Build ==="
	go vet ./engine
	CGO_ENABLED=1 go build -o /tmp/gotiengviet-demo ./cmd/demo
	CGO_ENABLED=1 go build -o /tmp/ibus-setup-gotiengviet ./cmd/setup
	CGO_ENABLED=1 go build -o /tmp/ibus-engine-gotiengviet-go ./cmd/gotiengviet-ibus
	gcc -O2 -g -o /tmp/ibus-engine-gotiengviet ibus/engine.c $$(pkg-config --cflags --libs ibus-1.0)
	@echo "Build ok: /tmp/gotiengviet-demo, /tmp/ibus-setup-gotiengviet, /tmp/ibus-engine-gotiengviet"

test: build
	@echo "=== Test ==="
	printf "quit\n" | /tmp/gotiengviet-demo 2>&1 | grep -E "✓|✗" | head -20
	/tmp/ibus-engine-gotiengviet 2>&1 | head -5

vet:
	go vet ./...

install: build
	sudo ./install.sh

clean:
	./clean.sh

package: build
	./package.sh $(VERSION)

help:
	@echo "make build    - Build tất cả (Go + C)"
	@echo "make test     - Test nhanh"
	@echo "make install  - Cài đặt (cần sudo)"
	@echo "make package  - Đóng gói .deb (VERSION=0.1.0-1)"
	@echo "make clean    - Dọn dẹp"
	@echo "make vet      - go vet"
