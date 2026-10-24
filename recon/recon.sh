#!/bin/sh
# Shell-only reconnaissance for the instrumented shell process. Numeric
# /proc paths are used deliberately: /proc/self inside grep/ls/cat would
# describe those helper programs instead of this process.
set -eu

pid=$$

echo "=== /proc/$pid/status (tracer lines) ==="
grep -E "TracerPid|Threads" "/proc/$pid/status"

echo "=== /proc/$pid/task ==="
ls "/proc/$pid/task"

echo "=== /proc/$pid/fd ==="
ls -l "/proc/$pid/fd" 2>/dev/null

echo "=== /proc/$pid/maps (executable mappings) ==="
grep -E "r-xp|rwxp" "/proc/$pid/maps"

echo "=== parent ==="
awk '{print $4}' "/proc/$PPID/stat" 2>/dev/null || true
cat "/proc/$PPID/comm" 2>/dev/null || true

echo "=== auxv size ==="
wc -c <"/proc/$pid/auxv"

echo "=== ASLR ==="
cat /proc/sys/kernel/randomize_va_space
