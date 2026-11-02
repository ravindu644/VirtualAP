# VirtualAP engine - build system
# Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the static musl `virtualap` binary from src/. scripts/build-static.sh
# runs `make native` inside the same Alpine container that builds hostapd and
# dnsmasq; `make aarch64` / `make armhf` do the same from the host with a
# musl-cross gcc, straight into backend/<arch>/ for a quick push to a phone.

BINARY_NAME = virtualap
SRC_DIR     = src
OUT_DIR     = output

# VERSION at the repository root is the one place the version lives.
VERSION := $(shell cat VERSION)

V ?= 0
ifeq ($(V),1)
  Q      =
  msg_cc =
  msg_ld =
else
  Q      = @
  msg_cc = @printf "  CC      %s\n" $<
  msg_ld = @printf "  LD      %s\n" $@
endif

# Every .c under src/ is part of the binary, so a new module needs no edit here.
SRCS := $(shell find $(SRC_DIR) -name '*.c' | sort)

# gcc only: -Wlogical-op and the -Wduplicated-* family are unknown to clang.
CFLAGS  = -Wall -Wextra -Wpedantic -O2 -std=gnu11 -I$(SRC_DIR) -fno-pie
CFLAGS += -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2
CFLAGS += -Wnull-dereference -Wcast-qual -Wlogical-op -Wshadow -Wdouble-promotion -Wundef
CFLAGS += -Wduplicated-cond -Wduplicated-branches -Wimplicit-fallthrough=3
CFLAGS += -fstack-protector-strong -DVAP_VERSION='"$(VERSION)"'
CFLAGS += -MMD -MP
# Only the container build turns warnings into errors: its gcc is pinned by the
# Alpine image, a host musl-cross gcc of another version warns differently.
ifeq ($(WERROR),1)
  CFLAGS += -Werror
endif
# -no-pie: a plain -static link on Alpine gives a static-PIE that still wants
# /lib/ld-musl-*.so.1, which Android does not have.
LDFLAGS = -static -no-pie
ifeq ($(NO_STRIP),)
  LDFLAGS += -s
endif

ARCH := $(shell $(CC) -dumpmachine 2>/dev/null | cut -d'-' -f1 | sed 's/^arm.*/armhf/')

OBJ_DIR = $(OUT_DIR)/.obj/$(ARCH)
OBJS    = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

BINARY = $(OUT_DIR)/$(BINARY_NAME)-$(ARCH)

find-cc = $(shell \
	if [ -n "$(MUSL_CROSS)" ] && [ -f "$(MUSL_CROSS)/$(1)-gcc" ]; then \
		echo "$(MUSL_CROSS)/$(1)-gcc"; \
	elif [ -f "$(HOME)/toolchains/$(1)-cross/bin/$(1)-gcc" ]; then \
		echo "$(HOME)/toolchains/$(1)-cross/bin/$(1)-gcc"; \
	elif command -v $(1)-gcc >/dev/null 2>&1; then \
		echo "$(1)-gcc"; \
	fi)

triplet_aarch64 = aarch64-linux-musl
triplet_armhf   = arm-linux-musleabihf

.DEFAULT_GOAL := help
.PHONY: help native aarch64 armhf check format clean

help:
	@echo "VirtualAP v$(VERSION) engine"
	@echo "  make native   - build with \$$CC (a musl gcc, gcc only), output/$(BINARY_NAME)-<arch>"
	@echo "  make aarch64  - cross build into backend/aarch64/$(BINARY_NAME)"
	@echo "  make armhf    - cross build into backend/armhf/$(BINARY_NAME)"
	@echo "  make check    - build and run tests/check.c with the host gcc"
	@echo "  make format   - clang-format src/ and tests/"
	@echo "  make clean    - remove output/"
	@echo "Options: V=1, WERROR=1, NO_STRIP=1, MUSL_CROSS=<dir with <triplet>-gcc>"

# VAP_VERSION and the flags come from these two, so a bump or a flag edit rebuilds.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c Makefile VERSION
	$(msg_cc)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BINARY): $(OBJS)
	$(msg_ld)
	$(Q)$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "[+] Built: $@"

native: $(BINARY)

# The sub-make re-derives ARCH, OBJ_DIR and BINARY from the cross compiler.
aarch64 armhf:
	@CROSS_CC="$(call find-cc,$(triplet_$@))"; \
	[ -n "$$CROSS_CC" ] || { echo "Error: $(triplet_$@)-gcc not found, set MUSL_CROSS"; exit 1; }; \
	$(MAKE) --no-print-directory $(OUT_DIR)/$(BINARY_NAME)-$@ CC=$$CROSS_CC && \
	mkdir -p backend/$@ && cp -f $(OUT_DIR)/$(BINARY_NAME)-$@ backend/$@/$(BINARY_NAME)

# Host-side asserts over the pure logic: everything but main.c, dynamically
# linked with whatever gcc the host has, never shipped.
# Headers are listed too: there is no .d file for this one-shot link.
CHECK_BIN = $(abspath $(OUT_DIR))/check
$(CHECK_BIN): tests/check.c $(filter-out $(SRC_DIR)/main.c,$(SRCS)) $(shell find $(SRC_DIR) -name '*.h') Makefile VERSION
	@mkdir -p $(dir $@)
	$(Q)gcc -Wall -Wextra -O1 -g -std=gnu11 -I$(SRC_DIR) -DVAP_VERSION='"$(VERSION)"' $(filter %.c,$^) -o $@

check: $(CHECK_BIN)
	$(Q)$(CHECK_BIN)

format:
	@find $(SRC_DIR) tests -name '*.[ch]' | xargs clang-format -i

clean:
	@rm -rf $(OUT_DIR)

# Last, so a dependency file can never become the default goal.
-include $(OBJS:.o=.d)
