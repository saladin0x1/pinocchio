/*
 * pinocchio - a Linux/x86-64 "hello world" that detects artifacts
 * observed under TracerPIN (Intel Pin).
 *
 * Voting sensors:
 *   1. executable anonymous or memfd-backed mappings;
 *   2. multiple descriptors that the application did not open.
 *
 * Two address probes (SIGILL context RIP and a return address) are printed
 * as non-voting evidence. Pin 3.30 presents both in application coordinates.
 *
 * Evidence acquisition uses raw system calls. Parsing and voting do not rely
 * on libc; libc is still used for signal setup and human-readable output.
 *
 * Exit codes: 0 = clean, 1 = instrumented, 2 = evidence unavailable.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <ucontext.h>

/* ---------- raw syscalls used to acquire kernel evidence ------------ */
static long sys3(long n, long a, long b, long c)
{
    long ret;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(n), "D"(a), "S"(b), "d"(c)
                 : "rcx", "r11", "memory");
    return ret;
}

static long sys4(long n, long a, long b, long c, long d)
{
    long ret;
    register long r10 asm("r10") = d;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
                 : "rcx", "r11", "memory");
    return ret;
}

#define R_OPEN(path, flags) \
    sys4(SYS_openat, AT_FDCWD, (long)(path), (long)(flags), 0)
#define R_READ(fd, b, n) \
    sys3(SYS_read, (long)(fd), (long)(b), (long)(n))
#define R_CLOSE(fd) \
    sys3(SYS_close, (long)(fd), 0, 0)
#define R_READLINK(path, b, n) \
    sys4(SYS_readlinkat, AT_FDCWD, (long)(path), (long)(b), (long)(n))
#define R_GETDENTS(fd, b, n) \
    sys3(SYS_getdents64, (long)(fd), (long)(b), (long)(n))

/* ---------- /proc/self/maps: complete read and local parser ---------- */
enum backing_kind {
    BACKING_FILE,
    BACKING_ANON,
    BACKING_MEMFD
};

struct region {
    uintptr_t start;
    uintptr_t end;
    unsigned char exec;
    unsigned char backing;
};

#define MAX_REGIONS 4096
#define MAPS_CAPACITY (512U * 1024U)

static struct region regions[MAX_REGIONS];
static int nregions;
static char maps_buf[MAPS_CAPACITY + 1];

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)(c | 32);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int parse_hex(const char **cursor, uintptr_t *out)
{
    const char *p = *cursor;
    uintptr_t value = 0;
    int digits = 0;
    int digit;

    while ((digit = hex_digit(*p)) >= 0) {
        if (value > (UINTPTR_MAX - (uintptr_t)digit) / 16U) return 0;
        value = value * 16U + (uintptr_t)digit;
        p++;
        digits++;
    }
    if (!digits) return 0;
    *cursor = p;
    *out = value;
    return 1;
}

static int parse_map_line(char *line, struct region *out)
{
    const char *p = line;
    uintptr_t start, end;

    if (!parse_hex(&p, &start) || *p++ != '-' ||
        !parse_hex(&p, &end) || *p++ != ' ') return 0;
    if (!p[0] || !p[1] || !p[2] || !p[3]) return 0;

    int exec = p[2] == 'x';
    p += 4;                                      /* permissions */

    /* Skip offset, device, and inode. */
    for (int field = 0; field < 3; field++) {
        while (*p == ' ') p++;
        if (!*p) return 0;
        while (*p && *p != ' ') p++;
    }
    while (*p == ' ') p++;

    enum backing_kind backing = BACKING_FILE;
    if (!*p || starts_with(p, "[anon:") || starts_with(p, "[anon_shmem:"))
        backing = BACKING_ANON;
    else if (starts_with(p, "/memfd:"))
        backing = BACKING_MEMFD;

    out->start = start;
    out->end = end;
    out->exec = (unsigned char)exec;
    out->backing = (unsigned char)backing;
    return start < end;
}

static int load_maps(void)
{
    long fd = R_OPEN("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;

    size_t used = 0;
    int complete = 0;
    while (used < MAPS_CAPACITY) {
        long n = R_READ(fd, maps_buf + used, MAPS_CAPACITY - used);
        if (n == -EINTR) continue;
        if (n < 0) break;
        if (n == 0) {
            complete = 1;
            break;
        }
        used += (size_t)n;
    }
    R_CLOSE(fd);
    if (!complete || used == 0) return 0;

    maps_buf[used] = 0;
    nregions = 0;
    char *line = maps_buf;
    char *end = maps_buf + used;

    while (line < end) {
        char *newline = line;
        while (newline < end && *newline != '\n') newline++;
        if (newline == end || nregions == MAX_REGIONS) return 0;
        *newline = 0;
        if (!parse_map_line(line, &regions[nregions])) return 0;
        nregions++;
        line = newline + 1;
    }
    return nregions > 0;
}

static int check_exec_mappings(void)
{
    int hits = 0;
    for (int i = 0; i < nregions; i++) {
        if (!regions[i].exec || regions[i].backing == BACKING_FILE) continue;
        const char *kind = regions[i].backing == BACKING_MEMFD
                         ? "memfd" : "anonymous";
        printf("  [maps] executable %s region: %lx-%lx\n", kind,
               (unsigned long)regions[i].start,
               (unsigned long)regions[i].end);
        hits++;
    }
    return hits;
}

/* ---------- /proc/self/fd via getdents64 ----------------------------- */
struct linux_dirent64_local {
    uint64_t ino;
    int64_t off;
    uint16_t reclen;
    uint8_t type;
    char name[];
};

struct sensor_result {
    int hits;
    int complete;
};

static int parse_fd_number(const char *s)
{
    unsigned int value = 0;
    int digits = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        unsigned int digit = (unsigned int)(*s++ - '0');
        if (value > ((unsigned int)INT_MAX - digit) / 10U) return -1;
        value = value * 10U + digit;
        digits++;
    }
    return digits ? (int)value : -1;
}

static void build_fd_path(char *out, int fd)
{
    const char *prefix = "/proc/self/fd/";
    while (*prefix) *out++ = *prefix++;

    char digits[16];
    int count = 0;
    do {
        digits[count++] = (char)('0' + fd % 10);
        fd /= 10;
    } while (fd > 0);
    while (count > 0) *out++ = digits[--count];
    *out = 0;
}

static struct sensor_result check_foreign_fds(void)
{
    static char buf[16384];
    static char link[512];
    char path[64];
    struct sensor_result result = {0, 1};

    long opened = R_OPEN("/proc/self/fd", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (opened < 0) {
        result.complete = 0;
        return result;
    }
    if (opened > INT_MAX) {
        R_CLOSE(opened);
        result.complete = 0;
        return result;
    }
    int dfd = (int)opened;

    for (;;) {
        long n = R_GETDENTS(dfd, buf, sizeof buf);
        if (n == -EINTR) continue;
        if (n < 0) {
            result.complete = 0;
            break;
        }
        if (n == 0) break;

        size_t offset = 0;
        while (offset < (size_t)n) {
            size_t minimum = offsetof(struct linux_dirent64_local, name) + 1U;
            if ((size_t)n - offset < minimum) {
                result.complete = 0;
                break;
            }

            struct linux_dirent64_local *entry =
                (struct linux_dirent64_local *)(buf + offset);
            size_t reclen = entry->reclen;
            if (reclen < minimum || reclen > (size_t)n - offset) {
                result.complete = 0;
                break;
            }

            size_t name_capacity = reclen - offsetof(struct linux_dirent64_local, name);
            int terminated = 0;
            for (size_t i = 0; i < name_capacity; i++) {
                if (entry->name[i] == 0) {
                    terminated = 1;
                    break;
                }
            }
            if (!terminated) {
                result.complete = 0;
                break;
            }

            int fd = parse_fd_number(entry->name);
            offset += reclen;
            if (fd < 0 || fd <= 2 || fd == dfd) continue;

            /* The directory entry proves that the descriptor exists.
             * readlinkat is diagnostic only and cannot suppress the vote. */
            result.hits++;
            build_fd_path(path, fd);
            long length;
            do {
                length = R_READLINK(path, link, sizeof link - 1U);
            } while (length == -EINTR);

            if (length >= 0) {
                link[length] = 0;
                printf("  [fd]   inherited/unexpected fd %d -> %s\n", fd, link);
            } else {
                printf("  [fd]   inherited/unexpected fd %d -> <unavailable>\n", fd);
            }
        }
        if (!result.complete) break;
    }

    R_CLOSE(dfd);
    return result;
}

/* ---------- non-voting address probes ------------------------------- */
enum address_class {
    ADDRESS_UNMAPPED,
    ADDRESS_FILE,
    ADDRESS_ANON,
    ADDRESS_MEMFD
};

static enum address_class classify_address(uintptr_t address)
{
    for (int i = 0; i < nregions; i++) {
        if (address < regions[i].start || address >= regions[i].end) continue;
        if (regions[i].backing == BACKING_ANON) return ADDRESS_ANON;
        if (regions[i].backing == BACKING_MEMFD) return ADDRESS_MEMFD;
        return ADDRESS_FILE;
    }
    return ADDRESS_UNMAPPED;
}

static const char *address_class_name(enum address_class cls)
{
    switch (cls) {
    case ADDRESS_FILE:  return "file-backed";
    case ADDRESS_ANON:  return "anonymous";
    case ADDRESS_MEMFD: return "memfd-backed";
    default:            return "unmapped";
    }
}

static volatile uintptr_t signal_rip;

static void on_sigill(int signal_number, siginfo_t *info, void *context)
{
    (void)signal_number;
    (void)info;
    ucontext_t *ucontext = context;
    signal_rip = (uintptr_t)ucontext->uc_mcontext.gregs[REG_RIP];
    ucontext->uc_mcontext.gregs[REG_RIP] += 2;    /* skip x86 UD2 */
}

static void probe_signal_rip(int maps_complete)
{
    struct sigaction action = {0};
    struct sigaction previous;
    action.sa_sigaction = on_sigill;
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIGILL, &action, &previous) != 0) {
        printf("  [rip-sig] unavailable\n");
        return;
    }

    signal_rip = 0;
    asm volatile("ud2");
    (void)sigaction(SIGILL, &previous, NULL);

    if (!maps_complete) {
        printf("  [rip-sig] signal context rip=%lx (maps unavailable)\n",
               (unsigned long)signal_rip);
        return;
    }
    enum address_class cls = classify_address(signal_rip);
    printf("  [rip-sig] signal context rip=%lx (%s)%s\n",
           (unsigned long)signal_rip, address_class_name(cls),
           cls == ADDRESS_FILE ? " - application coordinates" : "");
}

static __attribute__((noinline)) uintptr_t grab_return_address(void)
{
    return (uintptr_t)__builtin_return_address(0);
}

static void probe_return_address(int maps_complete)
{
    uintptr_t address = grab_return_address();
    if (!maps_complete) {
        printf("  [rip-ret] return address=%lx (maps unavailable)\n",
               (unsigned long)address);
        return;
    }
    printf("  [rip-ret] return address=%lx (%s)\n",
           (unsigned long)address,
           address_class_name(classify_address(address)));
}

/* One inherited descriptor is common in service and test environments.
 * TracerPIN 3.30 exposes four in the tested setup, so require corroboration
 * before descriptors alone produce an instrumented verdict. */
#define FD_VOTE_THRESHOLD 2

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Hello, world!\n");

    int maps_complete = load_maps();
    int map_hits = 0;
    if (maps_complete)
        map_hits = check_exec_mappings();
    else
        printf("  [maps] unavailable or incomplete\n");

    struct sensor_result fds = check_foreign_fds();
    if (!fds.complete)
        printf("  [fd]   enumeration unavailable or incomplete\n");
    else if (fds.hits == 1 && map_hits == 0)
        printf("  [fd]   one descriptor is ambiguous and does not vote\n");

    probe_signal_rip(maps_complete);
    probe_return_address(maps_complete);

    int detected = map_hits > 0 || fds.hits >= FD_VOTE_THRESHOLD;
    int artifacts = map_hits + fds.hits;
    if (detected) {
        printf("verdict: INSTRUMENTED (%d artifact%s: %d map, %d fd)"
               " - Pin may be instrumenting me\n",
               artifacts, artifacts == 1 ? "" : "s", map_hits, fds.hits);
        return 1;
    }
    if (!maps_complete || !fds.complete) {
        printf("verdict: INCONCLUSIVE (kernel evidence unavailable)\n");
        return 2;
    }

    printf("verdict: clean\n");
    return 0;
}
