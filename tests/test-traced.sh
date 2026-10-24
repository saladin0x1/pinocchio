#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
stock_image=${TRACER_IMAGE:-tracerpin}
stealth_image=${STEALTH_IMAGE:-tracerpin-stealth}

command -v docker >/dev/null 2>&1 || {
    echo "docker is required" >&2
    exit 2
}
for image in "$stock_image" "$stealth_image"; do
    docker image inspect "$image" >/dev/null 2>&1 || {
        echo "missing docker image: $image" >&2
        exit 2
    }
done

run_traced()
{
    image=$1
    launcher=$2
    repetitions=$3
    i=1
    while [ "$i" -le "$repetitions" ]; do
        if output=$(docker run --rm -v "$root:/workdir:ro" "$image" sh -lc \
            "cc -O2 -Wall -Wextra -Werror -std=gnu11 -o /tmp/pinocchio /workdir/src/poc.c && $launcher -o /tmp/trace.log -- /tmp/pinocchio" 2>&1); then
            status=0
        else
            status=$?
        fi
        if [ "$status" -ne 1 ] || ! printf '%s\n' "$output" | grep -q 'verdict: INSTRUMENTED'; then
            echo "not ok - $image run $i (status $status)"
            printf '%s\n' "$output"
            exit 1
        fi
        echo "ok - $image run $i"
        i=$((i + 1))
    done
}

run_naive()
{
    image=$1
    launcher=$2
    expected_status=$3
    expected_text=$4
    label=$5
    if output=$(docker run --rm -v "$root:/workdir:ro" "$image" sh -lc \
        "cc -O2 -Wall -Wextra -Werror -std=gnu11 -o /tmp/naive /workdir/lab/naive.c && $launcher -o /tmp/trace.log -- /tmp/naive" 2>&1); then
        status=0
    else
        status=$?
    fi
    if [ "$status" -ne "$expected_status" ] || \
       ! printf '%s\n' "$output" | grep -q "$expected_text"; then
        echo "not ok - $label (status $status)"
        printf '%s\n' "$output"
        exit 1
    fi
    echo "ok - $label"
}

run_smc()
{
    if output=$(docker run --rm -v "$root:/workdir:ro" "$stock_image" sh -lc \
        'cc -O2 -Wall -Wextra -Werror -std=gnu11 -o /tmp/smc /workdir/lab/smc_canary.c && Tracer -o /tmp/trace.log -- /tmp/smc' 2>&1); then
        status=0
    else
        status=$?
    fi
    if [ "$status" -ne 0 ] || ! printf '%s\n' "$output" | grep -q 'first=1 second=2'; then
        echo "not ok - smc-negative-control (status $status)"
        printf '%s\n' "$output"
        exit 1
    fi
    echo "ok - smc-negative-control"
}

run_traced "$stock_image" Tracer 1
run_traced "$stealth_image" kworkerd 5
run_naive "$stock_image" Tracer 1 'naive verdict: INSTRUMENTED' naive-stock
run_naive "$stealth_image" kworkerd 0 'naive verdict: clean' naive-cosmetic-patch
run_smc

echo "all traced tests passed"
