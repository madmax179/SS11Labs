# Minimal standalone Makefile (repo root variant)
UNIMRCP_DIR ?= /opt/unimrcp
PREFIX ?= $(UNIMRCP_DIR)
CC ?= gcc
CFLAGS ?= -O2 -fPIC -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?= -shared

CFLAGS += \
  -I./include \
  -I$(PREFIX)/include \
  -I$(PREFIX)/include/apt \
  -I$(PREFIX)/include/mpf \
  -I$(PREFIX)/include/mrcp \
  -I$(PREFIX)/include/mrcp-engine \
  -I$(PREFIX)/include/apr-toolkit

CFLAGS += $(shell pkg-config --cflags apr-1 apr-util-1 libcurl)
LDLIBS += $(shell pkg-config --libs apr-1 apr-util-1 libcurl)

SRC := \
  ./src/elevenlabs_synth_engine.c \
  ./src/elevenlabs_synth_channel.c \
  ./src/elevenlabs_http.c \
  ./src/ulaw_decode.c

OBJ := $(SRC:.c=.o)
TARGET := elevenlabs-synth.so

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS) -lm

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/plugin
	install -m 0755 $(TARGET) $(PREFIX)/plugin/

.PHONY: all clean install
