CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -Iengine $(shell $(PKG_CONFIG) --cflags gio-2.0)
BUILD_DIR ?= build
VERSION ?= 0.6.0-1
# Macro/emoji tables for tests (and dev runs without install).
export GTV_DATA_DIR ?= $(abspath data)
CORE_SRC := $(wildcard engine/*.c)
CORE_OBJ := $(patsubst engine/%.c,$(BUILD_DIR)/engine/%.o,$(CORE_SRC))
CORE_LIB := $(BUILD_DIR)/libgotiengviet.a
CORE_LIBS := $(shell $(PKG_CONFIG) --libs gio-2.0) -lm
BINS := $(BUILD_DIR)/ibus-engine-gotiengviet $(BUILD_DIR)/ibus-setup-gotiengviet $(BUILD_DIR)/gotiengviet-demo

.PHONY: all build test vet install clean package help
all: build
build: $(BINS)
$(BUILD_DIR)/engine/%.o: engine/%.c engine/engine.h engine/internal.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(CORE_LIB): $(CORE_OBJ)
	$(AR) rcs $@ $^
$(BUILD_DIR)/ibus-engine-gotiengviet: ibus/engine.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(shell $(PKG_CONFIG) --cflags --libs ibus-1.0) $(CORE_LIBS) -o $@
$(BUILD_DIR)/ibus-setup-gotiengviet: cmd/setup/main.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(shell $(PKG_CONFIG) --cflags --libs gtk+-3.0 ayatana-appindicator3-0.1) $(CORE_LIBS) -o $@
$(BUILD_DIR)/gotiengviet-demo: cmd/demo/main.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(CORE_LIBS) -o $@
$(BUILD_DIR)/test-engine: tests/test_engine.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(CORE_LIBS) -o $@
$(BUILD_DIR)/test-support: tests/test_support.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(CORE_LIBS) -o $@
$(BUILD_DIR)/fixtures/curl: tests/fake_curl.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) $(CORE_LIBS) -o $@
test: $(BUILD_DIR)/test-engine $(BUILD_DIR)/test-support $(BUILD_DIR)/fixtures/curl
	$(BUILD_DIR)/test-engine
	GTV_TEST_CURL_DIR="$(abspath $(BUILD_DIR))/fixtures" $(BUILD_DIR)/test-support
vet:
	$(MAKE) build test CFLAGS='-O2 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Werror' BUILD_DIR=$(BUILD_DIR)/strict
install: build
	sudo ./install.sh
package: build
	./package.sh $(VERSION)
clean:
	./clean.sh
help:
	@echo 'make build | test | vet | install | package VERSION=... | clean'
-include $(CORE_OBJ:.o=.d)

$(BUILD_DIR)/test-setup: tests/test_setup.c cmd/setup/main.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(shell $(PKG_CONFIG) --cflags --libs gtk+-3.0 ayatana-appindicator3-0.1) $(CORE_LIBS) -o $@
.PHONY: test-ui
test-ui: $(BUILD_DIR)/test-setup
	$(BUILD_DIR)/test-setup

$(BUILD_DIR)/test-ibus: tests/test_ibus.c ibus/engine.c $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CORE_LIB) $(LDFLAGS) $(shell $(PKG_CONFIG) --cflags --libs ibus-1.0) $(CORE_LIBS) -o $@
.PHONY: test-ibus
test-ibus: $(BUILD_DIR)/test-ibus $(BUILD_DIR)/fixtures/curl
	GTV_TEST_CURL_DIR="$(abspath $(BUILD_DIR))/fixtures" $(BUILD_DIR)/test-ibus
