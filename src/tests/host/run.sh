#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Host-side tests for the pure-C core (no Flipper SDK needed).
#   ./run.sh        run checks
#   ./run.sh -v     also dump rendered labels as ASCII
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/fichero_host_test"
SAN="-fsanitize=address,undefined -g"

cc -std=c11 -Wall -Wextra -Wno-unused-parameter $SAN \
    -o "$OUT" test.c ../../core/*.c ../../third_party/qrcodegen/qrcodegen.c
"$OUT" "$@"

# ESP32 bridge framing must interoperate with the Flipper's.
cc -std=c11 -Wall -Wextra $SAN -c -o "$OUT.uart_frame.o" ../../core/uart_frame.c
c++ -std=c++17 -Wall -Wextra $SAN -o "$OUT.bridge" test_bridge_frame.cpp "$OUT.uart_frame.o"
"$OUT.bridge"
