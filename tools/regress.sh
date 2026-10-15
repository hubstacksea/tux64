#!/bin/bash
# Automated play-tests: drives Tux with the debug autopilot (--bot) and checks the outcome.
# Usage: make && tools/regress.sh   (needs an X display)
cd "$(dirname "$0")/.." || exit 1
run() { # name, expect-regex, args...
  local name="$1"; local expect="$2"; shift 2
  out=$(timeout 120 ./tux64 --mute --nosave --fast --size 160 120 "$@" 2>&1)
  if echo "$out" | grep -qE "$expect"; then echo "PASS $name"; else echo "FAIL $name"; echo "$out" | grep -E "BOT|act=" | tail -5; fi
}
run king  "stars=1" --level 2 --pos 9 18 -38 0 --quit 6000 --bot "w 30;g 4 -34;d;w 30;b 10 2.0;p b 3;w 20;p b 3;w 150;b 10 2.0;p b 3;w 20;p b 3;w 150;b 10 2.0;p b 3;w 20;p b 3;w 150;b 10 2.0;p b 3;w 20;p b 3;w 150;g 0 -34;w 30;p a 10;g 0 -33;w 60;g 1 -34;P"
run race  "type 29 state 4" --level 2 --pos 8 0 50 180 --quit 3500 --bot "w 10;b 29 -1.5;p b 3;d;w 200;g 20 12;g 28 -2;g 7 -2;g 4 -8;g -21 -12;g -21 -38.5;g -16 -38.5;g -12 -43;g 11 -43;g 9 -38;g 8 -22.5;w 60;P"
run cannon "pos=\(5[0-9]\.[0-9]+ 22\.00" --level 2 --pos 40 0 60 0 --quit 400 --bot "w 5;g 40 56 0.3;w 40;C 151 68;w 200;P"
run pole  "type 4 |1-UP|act=19" --level 2 --pos -60 0 -44 180 --quit 1400 --bot "w 5;j -60 -50;k 0 1 400;P"
run rings "stars=1" --level 3 --quit 6000 --bot "w 5;g 6 44;g 6 38;S 3 -2 37;S 0 -2 36;S 8 -5 26;S 14 -9 14;S 6 -13 -2;S -8 -10 -12;S -15 -6 -19;w 200;P"
run baby  "type 27 state 50" --level 4 --pos 0 8 -32 180 --quit 5000 --bot "w 5;g 10 -46;w 30;b 27 -1.2;w 5;p b 3;w 5;g 0 -36;g 0 -28;g 0 -14;g 10 20;g 12 25.0;w 10;p z 3;w 300;P"
run brute "param 3$|stars=1" --level 4 --pos 30 1 5 90 --quit 6000 --bot "w 5;W;g 44 5;w 60;f 16;p b 3;w 30;f 16;p b 3;w 30;f 16;p b 3;w 40;W;g 52 5 1.5;f 16;p b 3;w 30;f 16;p b 3;w 30;W;g 52 5 1.5;f 16;p b 3;w 30;f 16;p b 3;w 30;P;w 200;P"
run elevator "pos=\(-?[0-9.]+ 20\.00" --level 4 --quit 5000 --bot "w 5;g 0 -10;g 0 -29;g 8 -38;g 0 -38;g 0 -40.4 0.3;p z 260;W;g 0 -46;P"
run slide "BOT 28: .*hp=8" --level 5 --quit 3000 --bot "w 5;g 0 -4 2;g 0 -20 3;g 8 -34 3;g 24 -40 3;g 40 -32 3;g 46 -16 3;g 38 -2 3;g 22 4 3;g 8 14 3;g 6 30 3;g 18 42 3;g 36 42 3;g 48 30 3;g 48 12 3;g 36 0 3;g 18 -6 3;g 2 -2 3;g -10 8 3;g -14 24 3;g -26 30 3;g -40 30 3;g -52 24 3;g -58 12 3;g -56 -2 3;g -56 -6 3;w 200;P"
run segfault "type 17 state 7|BOT: done" --level 6 --spawn 1 --quit 12000 --bot "w 10;g 0 -140;d;w 60;b 17 4.2;p b 2;R;w 150;g 0 -150;b 17 4.2;p b 2;R;w 150;g 0 -150;b 17 4.2;p b 2;R;w 150;g 0 -150;b 17 4.2;p b 2;R;w 150;g 0 -150;b 17 4.2;p b 2;R;w 150;P"
run ghost "coins=1" --level 8 --pos -25 0.3 16 0 --quit 3000 --bot "w 5;F 45;N 45 1.9;f 45;p b 3;w 40;P"
run ghostking "type 5 state 1 hp 0 pos=\(0\.00" --level 8 --pos 0 0.1 -28 180 --quit 4000 --bot "w 5;d;F 46;N 46 3.0;f 46;p b 3;w 60;F 46;N 46 3.0;f 46;p b 3;w 60;F 46;N 46 3.0;f 46;p b 3;w 100;P"
run obelisk "type 5 state 1 hp 0 .* param 2" --level 7 --quit 4000 --bot "w 5;T -36 9 20;w 60;T 36 9 20;w 60;T -52 9 -36;w 60;T 52 9 -36;w 30;P"
run painting "lv=2" --level 1 --quit 3000 --bot "w 5;g -20 -10;g -29.5 -10;w 60;p a 3;w 60;P"
run flight "act=40" --level 0 --pos 14 0 31 180 --quit 3000 --bot "w 5;g 14 28.2 0.3;p a 20;w 30;g 14 50;J 14 0;P"
run chomp "stars=1" --level 2 --quit 2000 --bot "w 5;X 50 2;T 46 3.5 -44;w 8;p z 3;w 200;g 53 -52.5 0.4;w 100;P"
run quicksand "lv=1" --level 7 --pos -36 1 45 270 --quit 600 --bot "w 400;P"
run pond-exit "pos=\([0-9.]+ -0\.60" --level 0 --pos 45 -1.5 5 0 --quit 400 --bot "w 30;h 45 15 - 25;h 45 15 a 25;h 45 15 - 40;w 30;P"
run door-bay "pos=\(39\.[0-9]+ -?0\.00 -10" --level 1 --pos 25 0 -10 90 --quit 1500 --bot "w 3;* 20;g 29.5 -10;w 60;g 40 -10;P"
run door-fort "pos=\(-?0\.[0-9]+ 6\.00 -49" --level 1 --pos 0 6 -36 180 --quit 1500 --bot "w 3;* 20;g 0 -40;w 60;g 0 -50;P"
run door-final "pos=\(-18\.00 0\.00 -49" --level 1 --pos -18 0 -36 180 --quit 1500 --bot "w 3;* 20;g -18 -40;w 60;g -18 -50;P"
run door-manor "pos=\(3[67]\.[0-9]+ 6\.00 -36" --level 1 --pos 26 6 -36 90 --quit 1500 --bot "w 3;* 20;g 30 -36;w 60;g 37 -36;P"
run slide-return "lv=1" --level 5 --quit 400 --bot "w 10;h 0 9 - 100;w 100;P"
run alcove-edge "act=0 pos=\(-?[0-9.]+ 6\.00" --level 1 --pos -31.5 6 -37.5 0 --quit 300 --bot "w 3;h -30 -45 - 8;h -30 -45 a 25;h -30 -45 - 60;w 20;P"
run spire-climb "pos=\(-?[0-9.]+ 91\.00" --level 9 --quit 9000 --bot "w 5;g -1.2 0 0.3;K 1 4;g -3.5 3.0 0.5;g 4.3 3.0 0.5;g 4.3 0 0.3;K 1 4;g 9 3.0 0.5;g -1.2 3.0 0.5;g -1.2 0 0.3;K 1 4;g -3.5 3.0 0.5;g 4.3 3.0 0.5;g 4.3 0 0.3;K 1 4;g 9 3.0 0.5;g -1.2 3.0 0.5;g -1.2 0 0.3;K 1 4;g -3.5 3.0 0.5;g 4.3 3.0 0.5;g 4.3 0 0.3;K 1 4;g 9 3.0 0.5;g -1.2 3.0 0.5;g -1.2 0 0.3;K 1 4;g -3.5 3.0 0.5;g 4.3 3.0 0.5;g 4.3 0 0.3;w 60;P"
run maze-sees "spotted=1" --level 10 --seed 7 --quit 200 --bot "w 3;T -14 0.1 -28;O 5 0 -90;w 4;V"
run maze-wall-blocks "spotted=0" --level 10 --seed 7 --quit 200 --bot "w 3;T -21 0.1 -28;O 1 1 180;w 4;V"
run maze-behind "spotted=0" --level 10 --seed 7 --quit 200 --bot "w 3;T -14 0.1 -28;O 5 0 90;w 4;V"
run maze-catch "hp=5" --level 10 --seed 7 --quit 600 --bot "w 3;T -14 0.1 -28;O 5 0 -90;w 180;P"
run maze-unseen-exit "act=11" --level 10 --seed 7 --quit 600 --bot "w 3;T 28 0.1 21;O 0 0 180;g 28 28 0.5;w 150;P"
