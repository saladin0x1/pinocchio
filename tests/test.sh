#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=${1:-"$root/pinocchio"}
cc=${CC:-cc}
tmp=${TMPDIR:-/tmp}/pinocchio-tests.$$
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

"$cc" -O2 -Wall -Wextra -Werror -fPIC -shared \
    -o "$tmp/memfd_exec.so" "$root/tests/memfd_exec.c"
"$cc" -O2 -Wall -Wextra -Werror -fPIC -shared \
    -o "$tmp/file_exec.so" "$root/tests/file_exec.c"
"$cc" -O2 -Wall -Wextra -Werror -fPIC -shared \
    -o "$tmp/many_maps.so" "$root/tests/many_maps.c"
"$cc" -O2 -Wall -Wextra -Werror -fPIC -shared \
    -o "$tmp/deny_proc.so" "$root/tests/deny_proc.c"

run_case()
{
    expected=$1
    name=$2
    shift 2
    if "$@" >"$tmp/$name.out" 2>&1; then
        actual=0
    else
        actual=$?
    fi
    if [ "$actual" -ne "$expected" ]; then
        echo "not ok - $name (expected $expected, got $actual)"
        cat "$tmp/$name.out"
        exit 1
    fi
    echo "ok - $name"
}

run_case 0 clean "$bin"
run_case 0 one-inherited-fd sh -c 'exec 9</dev/null; exec "$1"' sh "$bin"
run_case 1 memfd-executable env LD_PRELOAD="$tmp/memfd_exec.so" "$bin"
run_case 0 documented-file-backed-blind-spot env LD_PRELOAD="$tmp/file_exec.so" "$bin"
run_case 1 maps-larger-than-one-read env LD_PRELOAD="$tmp/many_maps.so" "$bin"
run_case 2 unavailable-proc-evidence env LD_PRELOAD="$tmp/deny_proc.so" "$bin"

echo "all local tests passed"
