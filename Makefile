# TUX 64 - build for Linux (native gcc) and Windows (MinGW, native or cross)
#
#   make            -> Linux build:   ./tux64
#   make windows    -> Windows build: ./tux64.exe  (needs x86_64-w64-mingw32-gcc)
#   make all-platforms
#   make package    -> dist/: Windows .7z, Linux .deb and .rpm  (tools/package.sh)
#   On Windows with MinGW/MSYS2:  mingw32-make   (auto-detects Windows_NT)

SRC_COMMON = src/main.c src/game.c src/player.c src/camera.c src/objects.c src/levels.c \
             src/levelbuild.c src/models.c src/gfx.c src/audio.c src/collision.c src/bot.c src/lang.c src/lang_ja.c src/sky.c src/space.c
CFLAGS ?= -O2
CFLAGS += -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers

MINGW ?= x86_64-w64-mingw32-gcc
WIN_LIBS = -lopengl32 -lgdi32 -lwinmm -luser32 -mwindows -static-libgcc

ifeq ($(OS),Windows_NT)
# native MinGW on Windows
CC ?= gcc
all: tux64.exe
tux64.exe: $(SRC_COMMON) src/plat_win32.c src/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC_COMMON) src/plat_win32.c $(WIN_LIBS) -lm
else
CC ?= gcc
LINUX_LIBS = -lGL -lX11 -lpthread -ldl -lm
all: tux64
tux64: $(SRC_COMMON) src/plat_x11.c src/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC_COMMON) src/plat_x11.c $(LINUX_LIBS)
windows: tux64.exe
tux64.exe: $(SRC_COMMON) src/plat_win32.c src/*.h
	$(MINGW) $(CFLAGS) -o $@ $(SRC_COMMON) src/plat_win32.c $(WIN_LIBS) -lm
all-platforms: tux64 tux64.exe
endif

package:
	tools/package.sh

clean:
	rm -f tux64 tux64.exe
	rm -rf dist

.PHONY: all windows all-platforms package clean
