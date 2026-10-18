CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS ?= -lm

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0)

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
INCDIR ?= $(PREFIX)/include/rapoo

BUILDDIR ?= build
BIN ?= bin

SRC_DRIVER := src/rapoo_driver.c
SRC_CLI := src/cli_main.c
SRC_GUI := src/gui_main.c

OBJ_DRIVER := $(BUILDDIR)/rapoo_driver.o
OBJ_CLI := $(BUILDDIR)/cli_main.o
OBJ_GUI := $(BUILDDIR)/gui_main.o

TARGET_LIB := $(BUILDDIR)/librapoo.a
TARGET_CLI := $(BIN)/rapoo-mouse
TARGET_GUI := $(BIN)/rapoo-mouse-gui

.PHONY: all cli gui lib clean install uninstall

all: cli gui lib

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BIN):
	mkdir -p $(BIN)

$(OBJ_DRIVER): $(SRC_DRIVER) include/rapoo_driver.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_LIB): $(OBJ_DRIVER)
	ar rcs $@ $^

lib: $(TARGET_LIB)

$(OBJ_CLI): $(SRC_CLI) include/rapoo_driver.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_CLI): $(OBJ_CLI) $(OBJ_DRIVER) | $(BIN)
	$(CC) $^ $(LDFLAGS) -o $@

cli: $(TARGET_CLI)

$(OBJ_GUI): $(SRC_GUI) include/rapoo_driver.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

$(TARGET_GUI): $(OBJ_GUI) $(OBJ_DRIVER) | $(BIN)
	$(CC) $^ $(LDFLAGS) $(GTK_LIBS) -o $@

gui: $(TARGET_GUI)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET_CLI) $(DESTDIR)$(BINDIR)/rapoo-mouse
	install -m 755 $(TARGET_GUI) $(DESTDIR)$(BINDIR)/rapoo-mouse-gui
	install -d $(DESTDIR)$(INCDIR)
	install -m 644 include/rapoo_driver.h $(DESTDIR)$(INCDIR)/
	install -m 644 include/rapoo_mouse.hpp $(DESTDIR)$(INCDIR)/
	@echo "✓ Rapoo C/C++ binaries installed to $(DESTDIR)$(BINDIR)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/rapoo-mouse
	rm -f $(DESTDIR)$(BINDIR)/rapoo-mouse-gui
	rm -rf $(DESTDIR)$(INCDIR)
	@echo "✓ Rapoo binaries uninstalled"

clean:
	rm -rf $(BUILDDIR) $(BIN)
