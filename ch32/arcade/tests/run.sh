#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
TASK_TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TASK_TEST_DIR"' EXIT
cc -std=gnu99 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -DARCADE_HOST -I. -Icompat -Ivendor/mini -Ivendor/tiny -Itests -include compat/base.h \
 arcade.c platform.c te.c ui.c fractals.c life.c video.c vendor/mini/*.c vendor/tiny/invaders.c \
 tests/test_arcade.c -o "$TASK_TEST_DIR/test"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$TASK_TEST_DIR/test" "$@"
