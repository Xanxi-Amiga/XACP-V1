# AmigaAMP external engine - ZZ9000 / XACP backend
#
# Original EngineLibrary framework: Thomas Wenzel
# ZZ9000/XACP adaptation: Copyright (C) 2026 Xanxi
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# This is intentionally a LEAN build. The XACP backend performs MPEG audio
# decoding on the ZZ9000 ARM and does not use the historical local amp /
# equalizer decoder objects from the original EngineLibrary distribution.

CROSS   ?= m68k-amigaos-
CC      := $(CROSS)gcc
STRIP   := $(CROSS)strip
RM      ?= rm -f

CPU     ?= -m68020 -m68881
CFLAGS  ?= -g -O3 $(CPU) -I. -Iinclude -fstrength-reduce -fomit-frame-pointer \
           -fno-aggressive-loop-optimizations -DHAVE_CONFIG_H \
           -DBYTE_ORDER_IS_BIG_ENDIAN -DLIBNAME=\"zz9000.engine\"
LDFLAGS ?= -g $(CPU) -nostartfiles

OBJS = StartUp.o LibInit.o Funcs.o EngineAccess.o iEngine_ZZ9000.o MyStdlib.o

BIN_DBG = zz9000.engine.debug
BIN     = zz9000.engine

.PHONY: all clean mrproper
all: $(BIN)

$(BIN): $(BIN_DBG)
	$(STRIP) -s -o $@ $<

$(BIN_DBG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

StartUp.o: StartUp.c
	$(CC) $(CFLAGS) -fno-force-addr -c $< -o $@

LibInit.o: LibInit.c
	$(CC) $(CFLAGS) -Wno-pointer-to-int-cast -c $< -o $@

clean:
	$(RM) $(OBJS)

mrproper: clean
	$(RM) $(BIN_DBG) $(BIN)
