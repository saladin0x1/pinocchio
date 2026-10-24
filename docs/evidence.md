# Evidence — clean and TracerPIN runs

Environment used for these observations: the public x86-64 image
`ghcr.io/llaubin/tracer/tracerpin@sha256:cac02a14f9b801e25bf8d7fd992b46371e22d8406d0871de4a9bf0537890c367`,
Intel Pin `3.30-98830-1d7b601b3`, and GCC 14.2 for the local rebuilt image.
Addresses are shortened because ASLR changes them on every run.

## Build checks

All project C programs compile with warnings promoted to errors:

```sh
cc -O2 -Wall -Wextra -Werror -std=gnu11 -o /tmp/pinocchio src/poc.c
cc -O2 -Wall -Wextra -Werror -std=gnu11 -o /tmp/naive lab/naive.c
cc -O2 -Wall -Wextra -Werror -std=gnu11 -o /tmp/recon recon/recon_linux.c
```

An AddressSanitizer/UndefinedBehaviorSanitizer clean run also completes without
a report. This specifically regresses the removed stack-scanning overflow.

## Clean run

```text
Hello, world!
  [rip-sig] signal context rip=... (file-backed) - application coordinates
  [rip-ret] return address=... (file-backed)
verdict: clean
```

Exit status: `0`.

With one descriptor inherited from the launcher:

```text
Hello, world!
  [fd]   inherited/unexpected fd 9 -> /etc/hosts
  [fd]   one descriptor is ambiguous and does not vote
  [rip-sig] signal context rip=... (file-backed) - application coordinates
  [rip-ret] return address=... (file-backed)
verdict: clean
```

Exit status: `0`.

## Stock TracerPIN

Representative run:

```text
Hello, world!
  [maps] executable anonymous region: ...-...
  [maps] executable anonymous region: ...-...
  [maps] executable anonymous region: ...-...
  [fd]   inherited/unexpected fd 128 -> /tmp/t.log
  [fd]   inherited/unexpected fd 129 -> /tmp/pinocchio
  [fd]   inherited/unexpected fd 130 -> .../ld-linux-x86-64.so.2
  [fd]   inherited/unexpected fd 131 -> .../libc.so.6
  [rip-sig] signal context rip=... (file-backed) - application coordinates
  [rip-ret] return address=... (file-backed)
verdict: INSTRUMENTED (7 artifacts: 3 map, 4 fd) - Pin may be instrumenting me
```

Exit status: `1`.

The exact count has varied between two and three executable regions across
builds, but at least one large Pin code-cache mapping and four descriptors have
remained present.

## Cosmetic patched tracer

`lab/patch.sh` renames the banner, shared object, and launcher, and unlinks the
trace path after opening it. The string-based `lab/naive.c` returns clean.
Pinocchio still reports:

```text
  [maps] executable anonymous region: ...-...
  [maps] executable anonymous region: ...-...
  [maps] executable anonymous region: ...-...
  [fd]   inherited/unexpected fd 128 -> /tmp/t.log (deleted)
  [fd]   inherited/unexpected fd 129 -> /tmp/pinocchio
  [fd]   inherited/unexpected fd 130 -> .../ld-linux-x86-64.so.2
  [fd]   inherited/unexpected fd 131 -> .../libc.so.6
verdict: INSTRUMENTED (7 artifacts: 3 map, 4 fd) - Pin may be instrumenting me
```

Five consecutive executions completed with exit status `1`; none crashed.

## Regression fixtures

`make test` uses preload fixtures without running Pin:

1. A memfd is truncated, mapped executable, and closed. `/proc/self/maps`
   contains `/memfd:pinocchio-test-cache (deleted)`. The map sensor detects it
   and exits `1` even though no backing FD remains.
2. A regular temporary file is mapped executable and its FD is closed. The
   detector returns clean, preserving a regression for the documented
   file-backed-cache blind spot.
3. Hundreds of alternating mappings enlarge `/proc/self/maps` beyond one
   procfs read, followed by an executable anonymous mapping. The detector
   reads to EOF, finds the mapping, and exits `1`.
4. A seccomp filter denies `openat` after process startup. Both required scans
   become unavailable and the detector exits `2` instead of reporting clean.

Current local suite output:

```text
ok - clean
ok - one-inherited-fd
ok - memfd-executable
ok - documented-file-backed-blind-spot
ok - maps-larger-than-one-read
ok - unavailable-proc-evidence
all local tests passed
```

## Correctness issue found during review

The original reader issued one 128 KiB `read()` and assumed that its requested
size guaranteed a complete procfs result. Under the patched tracer, an
equivalent read repeatedly returned 4,078 bytes while the complete maps file
was about 12.6 KiB. `[stack]` was outside the returned prefix.

The old stack scanner consequently failed to find its intended boundary and
segfaulted in three of five repeated runs; ASan also diagnosed an out-of-bounds
stack read. The fix was to consume maps until EOF and remove the nonessential
stack traversal. The five-run patched-tracer regression now guards this case.

## SMC negative result

A generated `mov eax, 1; ret` function was executed, rewritten to return `2`,
and executed again. Native execution and the pinned Pin image both produced
`first=1 second=2`. The semantic canary therefore does not detect this Pin
version, consistent with Pin’s documented default SMC support. It is retained
as a researched negative result, not added to the voting code. The proposed
follow-up is to measure randomized invalidation pressure as weak timing
evidence across a coverage matrix.

## Interpretation

The experiment establishes detection of the tested TracerPIN artifacts and
survival of cosmetic renaming/unlinking. It does not establish that kernel
records are unforgeable or that every code cache must be anonymous. Those
limitations and the next proposed semantic sensor are covered in
[`WRITEUP.md`](WRITEUP.md).
