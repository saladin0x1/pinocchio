# Pinocchio 🤥

> A hello-world that knows when Intel Pin is lying to it.

Linux x64 binary that detects execution under
[TracerPIN](https://github.com/llaubin/Tracer) (Intel Pin).
Assignment for the Quarkslab 2026-2027 internship
*"Fighting Dynamic Binary Instrumentation"*.

**The submission documents are [`docs/WRITEUP.md`](docs/WRITEUP.md)**
(choices, raw results, the patched-tracer round, attacker analysis)
and `src/poc.c`. Everything else supports them.

## Build and run

```sh
make          # cc, C11 -> ./pinocchio
./pinocchio   # exit 0 clean / 1 instrumented / 2 evidence unavailable
```

Under the tracer and the cosmetic-patched tracer, and the local
regression fixtures (memfd, inherited-fd, seccomp-denied /proc):

```sh
make test          # local fixtures
make test-traced   # requires the docker images below
```

Tracer images: build from the public
[llaubin/Tracer docker branch](https://github.com/llaubin/Tracer/tree/docker/TracerPIN),
or pull the digest-pinned image used for the recorded results:

```sh
docker pull ghcr.io/llaubin/tracer/tracerpin@sha256:cac02a14f9b801e25bf8d7fd992b46371e22d8406d0871de4a9bf0537890c367
docker tag  ghcr.io/llaubin/tracer/tracerpin@sha256:cac02a14f9b801e25bf8d7fd992b46371e22d8406d0871de4a9bf0537890c367 tracerpin
```

## Layout

```
src/poc.c                # the detector
docs/WRITEUP.md          # the write-up (submission)
docs/evidence.md         # raw observations
docs/research-antidbi.md # anti-DBI state of the art, referenced
lab/                     # attacker patch, stealth image, strawman, SMC canary
tests/                   # local + traced test suites
recon/                   # observability tooling
```

AI tools were used during development; the write-up discloses how,
including one suggestion verified wrong and rejected.

## License

MIT
