@echo off
rem TUX 64 - Windows build with MinGW-w64 (gcc in PATH)
gcc -O2 -std=gnu99 -o tux64.exe src\main.c src\game.c src\player.c src\camera.c src\objects.c src\levels.c src\levelbuild.c src\models.c src\gfx.c src\audio.c src\collision.c src\bot.c src\lang.c src\lang_ja.c src\plat_win32.c -lopengl32 -lgdi32 -lwinmm -luser32 -mwindows -static-libgcc -lm
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)
echo Built tux64.exe
