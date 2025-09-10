# Minimal standalone Makefile (run from standalone/)

UNIMRCP_DIR ?= /opt/unimrcp
PREFIX ?= $(UNIMRCP_DIR)

CC ?= gcc
CFLAGS ?= -O2 -fPIC -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?= -shared

# Include paths
CFLAGS += \
  -I../include \
  -I$(PREFIX)/include \
  -I$(PREFIX)/include/apt \
  -I$(PREFIX)/include/mpf \
  -I$(PREFIX)/include/mrcp \
  -I$(PREFIX)/include/mrcp-engine \
  -I$(PREFIX)/include/apr-toolkit

# APR and CURL flags via pkg-config (ignore if not found)
CFLAGS += $(shell pkg-config --cflags apr-1 apr-util-1 libcurl 2>/dev/null)
LDLIBS += $(shell pkg-config --libs apr-1 apr-util-1 libcurl 2>/dev/null)

# UniMRCP plugin flags via pkg-config from $(PREFIX)
UNIMRCP_PKG_PATH := $(PREFIX)/lib/pkgconfig
CFLAGS += $(shell PKG_CONFIG_PATH=$(UNIMRCP_PKG_PATH) pkg-config --cflags unimrcpplugin 2>/dev/null)
LDLIBS += $(shell PKG_CONFIG_PATH=$(UNIMRCP_PKG_PATH) pkg-config --libs unimrcpplugin 2>/dev/null)

# Fallback: ensure we can find UniMRCP libs at link time and at runtime
LDFLAGS += -L$(PREFIX)/lib -Wl,-rpath,'$$ORIGIN/../lib'

# Sources live one level up
SRC_NAMES := \
  elevenlabs_synth_engine.c \
  elevenlabs_synth_channel.c \
  elevenlabs_http.c \
  ulaw_decode.c

SRC := $(addprefix ../src/,$(SRC_NAMES))
OBJ := $(SRC_NAMES:.c=.o)  # Objects will be in current directory
TARGET := elevenlabs-synth.so

all: $(TARGET)

rebuild: clean all

# Build objects locally from ../src
%.o: ../src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/plugin
	install -m 0755 $(TARGET) $(PREFIX)/plugin/

.PHONY: all clean install