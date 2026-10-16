# SPDX-License-Identifier: GPL-3.0-only
# Parámetros adaptados de los ejemplos hello_world/input de OpenOrbis v0.5.4.
ifeq ($(strip $(OO_PS4_TOOLCHAIN)),)
$(error Falta OO_PS4_TOOLCHAIN. Consulta docs/BUILDING.md)
endif

SDK := $(OO_PS4_TOOLCHAIN)
X4_WEBRTC_ROOT ?= $(CURDIR)/.local/deps
X4_WEBRTC_PREFIX ?= $(X4_WEBRTC_ROOT)/prefix
CC := clang
CXX := clang++
LD := ld.lld
CFLAGS := -ffile-prefix-map=$(CURDIR)=/nexo-x4 -ffile-prefix-map=$(SDK)=/openorbis --target=x86_64-pc-freebsd12-elf -std=c23 -fPIC -funwind-tables -O2 -Wall -Wextra -MMD -MP -c -isysroot $(SDK) -I$(X4_WEBRTC_ROOT)/overlay -I$(X4_WEBRTC_PREFIX)/include -isystem $(SDK)/include -D_BSD_SOURCE -DX4_OPENORBIS
# Instrumented try-gate baseline for the owner-requested A/B/A trial.
X4_INGRESS_MODE ?= mpsc
ifeq ($(X4_INGRESS_MODE),baseline)
CFLAGS += -DX4_INGRESS_BASELINE
else ifneq ($(X4_INGRESS_MODE),mpsc)
$(error X4_INGRESS_MODE must be mpsc or baseline)
endif
X4_RECEIVER_NACK ?= 1
ifneq ($(filter $(X4_RECEIVER_NACK),0 1),$(X4_RECEIVER_NACK))
$(error X4_RECEIVER_NACK must be 0 or 1)
endif
CFLAGS += -DX4_RECEIVER_NACK=$(X4_RECEIVER_NACK)
X4_NACK_GRACE_MS ?= 0
ifneq ($(filter $(X4_NACK_GRACE_MS),0 45),$(X4_NACK_GRACE_MS))
$(error X4_NACK_GRACE_MS must be 0 or 45)
endif
ifeq ($(X4_NACK_GRACE_MS),45)
ifneq ($(X4_RECEIVER_NACK),1)
$(error Grace45 requires the existing receiver NACK generator ON)
endif
endif
X4_RUMBLE ?= 0
ifneq ($(filter $(X4_RUMBLE),0 1),$(X4_RUMBLE))
$(error X4_RUMBLE must be 0 or 1)
endif
CFLAGS += -DX4_NACK_GRACE_MS=$(X4_NACK_GRACE_MS) -DX4_RUMBLE=$(X4_RUMBLE)
X4_DEV_CONTROL ?= 0
ifneq ($(filter $(X4_DEV_CONTROL),0 1),$(X4_DEV_CONTROL))
$(error X4_DEV_CONTROL must be 0 or 1)
endif
CFLAGS += -DX4_DEV_CONTROL=$(X4_DEV_CONTROL)
X4_PSBOX_UI ?= 1
ifneq ($(filter $(X4_PSBOX_UI),0 1),$(X4_PSBOX_UI))
$(error X4_PSBOX_UI must be 0 or 1)
endif
CFLAGS += -DX4_PSBOX_UI=$(X4_PSBOX_UI)
CXXFLAGS := $(filter-out -std=c23,$(CFLAGS)) -std=c++17 -nostdinc++ -isystem $(SDK)/include/c++/v1
LDFLAGS := --strip-debug  -m elf_x86_64 -pie --script $(SDK)/link.x --eh-frame-hdr -L$(SDK)/lib
RTC_LIBS := $(X4_WEBRTC_ROOT)/build-datachannel/libdatachannel-static.a \
    $(X4_WEBRTC_ROOT)/build-datachannel/deps/libjuice/libjuice-static.a \
    $(X4_WEBRTC_ROOT)/build-datachannel/deps/usrsctp/usrsctplib/libusrsctp.a \
    $(X4_WEBRTC_ROOT)/build-datachannel/deps/libsrtp/libsrtp2.a \
    $(X4_WEBRTC_PREFIX)/lib/libmbedtls.a $(X4_WEBRTC_PREFIX)/lib/libmbedx509.a \
    $(X4_WEBRTC_PREFIX)/lib/libmbedcrypto.a $(X4_WEBRTC_PREFIX)/lib/libeverest.a \
    $(X4_WEBRTC_PREFIX)/lib/libp256m.a $(X4_WEBRTC_PREFIX)/lib/libopus.a $(X4_WEBRTC_PREFIX)/lib/libz.a
LIBS := -lScePosix -lc -lkernel -lSceVideoOut -lSceGnmDriver -lScePad -lSceUserService -lc++ -lc++abi -lunwind -lm
SOURCES := $(wildcard src/core/*.c src/ui/*.c src/video/*.c src/input/*.c src/audio/*.c src/auth/*.c src/streaming/*.c src/media/*.c)
# Rebuild this complete LLVM 11 object with native ETIMEDOUT=60. The SDK's
# precompiled object compares against 110. Explicit objects precede -lc++;
# providing all five strong methods keeps that archive member unselected.
# llvmorg-11.0.0, commit 176249bd6732a8044d457092ed932768724a6f06.
# Exact upstream SHA256: 39d39fc245662b760c7c74a29218eeb8c66805996dc00e25e1e77693e53a77a5.
# Full license: docs/licenses/LLVM-libcxx-11-LICENSE.TXT.
CXX_SOURCES := src/streaming/rtc_condition_variable.cpp
ifeq ($(X4_PSBOX_UI),1)
CFLAGS += -Isrc/ui/psbox/vendor -Isrc/ui/psbox/vendor/extern
CXXFLAGS += -Isrc/ui/psbox/vendor -Isrc/ui/psbox/vendor/extern
SOURCES += src/ui/psbox/vendor/extern/qrcodegen.c src/ui/psbox_backend/metadata.c src/ui/psbox_backend/curated.c src/ui/psbox_backend/backend.c
CXX_SOURCES += $(wildcard src/ui/psbox/*.cpp src/ui/psbox/vendor/ui/*.cpp src/ui/psbox/vendor/input/*.cpp) src/ui/psbox_backend/image_cache.cpp
endif
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES)) $(patsubst src/%.cpp,build/%.o,$(CXX_SOURCES))

.PHONY: all package
all: build/eboot.bin
package: build/eboot.bin
	bash scripts/empaquetar.sh

build:
	mkdir -p build

build/%.o: src/%.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

build/streaming/rtc_condition_variable.o: CXXFLAGS += -D_LIBCPP_BUILDING_LIBRARY

build/%.o: src/%.cpp Makefile
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

build/nexo-x4.elf: $(OBJECTS) $(RTC_LIBS)
	$(LD) $(SDK)/lib/crt1.o $(OBJECTS) -o $@ $(LDFLAGS) --start-group $(RTC_LIBS) $(LIBS) --end-group

build/eboot.bin: build/nexo-x4.elf
	$(SDK)/bin/linux/create-fself -in=$< -out=build/nexo-x4.oelf --eboot $@ --paid 0x3800000000000011

-include $(OBJECTS:.o=.d)
