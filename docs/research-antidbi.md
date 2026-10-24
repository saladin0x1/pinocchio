# Anti-DBI research notes

These notes separate mechanisms implemented in the PoC from possible next
steps. They are a design map, not evidence that every cited mechanism defeats
current Intel Pin.

## Implemented baseline: environment artifacts

Pinocchio currently observes:

- executable anonymous, named-anonymous, and memfd-backed mappings;
- descriptors retained in the application process;
- signal-context and return addresses as non-voting transparency probes.

The `/proc` data is acquired with raw Linux x86-64 system calls and parsed
locally. This bypasses libc-only interposition, but not instruction-level
system-call interception by a DBI.

These checks belong to the environment-artifact/code-cache-footprint family.
They are inexpensive and repeatable against the tested TracerPIN image, but
legitimate JITs can look similar and a hardened DBI can forge or redesign the
artifacts.

## Candidate independent sensors

### Self-modifying-code result and pressure experiment

`lab/smc_canary.c` executes generated code, changes its bytes, and executes it
again. Native execution and Pin 3.30 both observe the new bytes. This is an
expected negative result: Pin documents SMC support as enabled by default and
exposes `PIN_SetSmcSupport` and `TRACE_AddSmcDetectedFunction` [11]. The
semantic canary is therefore not a detector.

Repeated randomized rewriting remains useful as a pressure workload. Pin must
preserve semantics while managing translated-code coherence, and its API warns
that SMC callback tracking can consume unbounded memory. Any resulting timing
or resource signal still needs calibration and must remain weak evidence.

### Relative timing

Compare warmed execution against translation- and invalidation-heavy workloads
using distributions and within-process ratios rather than fixed cycle
thresholds [4][9]. Timing is weak evidence: scheduling, frequency scaling,
virtualization, and load all produce natural variance. It belongs in a
multi-sensor score, not as a lone verdict.

### Text and PLT/GOT integrity

Hash expected application text and verify selected dynamic-link targets.
These checks detect inline patching or interposition inside the original
image, but a code-cache DBI can leave both untouched. They complement rather
than replace code-cache sensors [1].

### Hardware performance counters

Translation and dispatch may alter branches, instruction counts, and cache
behavior. Counter availability and permissions vary across kernels and
virtualized environments, and Pin can potentially virtualize observations.
This is better suited to coverage experiments than the small submission PoC.

## Active pressure and evasion research

- **SMC pressure:** repeatedly modify translated code to force invalidation.
- **Code-cache pressure:** generate many unique basic blocks to force cache
  growth, eviction, or translation overhead [2][8].
- **Truman Show escape:** exploit differences between original and translated
  code to detect or leave instrumented execution [2].
- **ROP-based construction:** ROPfuscator compiles execution into gadget
  chains, disrupting ordinary basic-block discovery [3]. This is related work,
  not a practical sensor for this PoC.
- **Control-flow flattening:** dispatcher-heavy control flow complicates
  analysis but is an obfuscation transform rather than a direct Pin oracle
  [6].

## Attacker resilience ladder

The part-two patch currently covers only the first rungs:

1. rename banners, libraries, and launchers;
2. unlink the trace pathname while retaining its descriptor;
3. use W^X transitions rather than simultaneous writable/executable memory;
4. use memfd or regular-file backing for translated code;
5. move trace transport to a helper or shared memory;
6. intercept and sanitize mapping/descriptor observations;
7. harden translation behavior and timing in the style explored by Arancino
   [5].

Pinocchio covers rungs 1–3 and recognizes memfd backing at rung 4. Ordinary
file backing, externalized state, and forged observations remain limitations.
The useful engineering goal is not an “unhookable” check; it is a set of
independent sensors whose combined concealment cost can be measured across a
coverage matrix.

## References

[1] D. Aucsmith. “Integrity Properties of Software: Modeling and
Measurement” / tamper-resistance lineage (1996).

[2] H. Sun, K. Xu, C. Song. “Break Out of the Truman Show: Active Detection
and Escape of Dynamic Binary Instrumentation.” Black Hat Asia 2016.
https://www.blackhat.com/docs/asia-16/materials/asia-16-Sun-Break-Out-Of-The-Truman-Show-Active-Detection-And-Escape-Of-Dynamic-Binary-Instrumentation.pdf

[3] H. Koo, M. Polino, A. Continella, et al. “ROPfuscator: Robust Software
Obfuscation Against Automated Static and Dynamic Analyses.”
https://arxiv.org/abs/2012.09163

[4] “Unveiling Dynamic Binary Instrumentation Techniques” (survey).
https://arxiv.org/html/2508.00682v1

[5] M. Polino, A. Continella, S. Mariani, et al. “Measuring and Defeating
Anti-Instrumentation-Equipped Malware” (Arancino), DIMVA 2017.
https://github.com/necst/arancino
https://conand.me/publications/polino-arancino-2017.pdf

[6] T. Biondi, M. M. Chinea, D. Balzarotti. “Automated Detection of
Control-flow Flattening.” 2021.
https://synthesis.to/2021/03/03/flattening_detection.html

[7] OWASP MASTG anti-Frida detection demo.
https://mas.owasp.org/MASTG-DEMO-0108/

[8] TracerPIN source lineage and pinned test image:
https://github.com/SideChannelMarvels/Tracer/tree/master/TracerPIN
`ghcr.io/llaubin/tracer/tracerpin@sha256:cac02a14f9b801e25bf8d7fd992b46371e22d8406d0871de4a9bf0537890c367`

[9] D. C. D’Elia, E. Coppa, S. Nicchi, F. Palmaro, L. Cavallaro. “SoK: Using
Dynamic Binary Instrumentation for Security (And How You May Get Caught Red
Handed).” AsiaCCS 2019.
https://kclpure.kcl.ac.uk/portal/en/publications/sok-using-dynamic-binary-instrumentation-for-security-and-how-you-may-get-caught-red-handed(a7d279d0-0677-4872-a129-5d267244e065).html

[10] Linux `read(2)`, `/proc/PID/maps`, and `memfd_create(2)`:
https://man7.org/linux/man-pages/man2/read.2.html
https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html
https://man7.org/linux/man-pages/man2/memfd_create.2.html

[11] Intel Pin control, SMC callback, syscall API, and downloads:
https://software.intel.com/sites/landingpage/pintool/docs/98484/Pin/html/group__PIN__CONTROL.html
https://software.intel.com/sites/landingpage/pintool/docs/98484/Pin/html/group__TRACE.html
https://software.intel.com/sites/landingpage/pintool/docs/98484/Pin/html/group__SYSCALL.html
https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html
