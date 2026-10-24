/* recon_linux.c - observability only, detects nothing.
 * Prints what the kernel says about THIS process so the same binary
 * can be diffed between a clean run and a TracerPIN run.
 * Build inside the tracerpin container (linux target), run twice:
 *   clean:  ./recon_linux
 *   traced: Tracer -o traced.log -- ./recon_linux
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/auxv.h>
#include <elf.h>

static void line(const char *s) { printf("%s\n", s); }

int main(void)
{
    char buf[512];

    line("== identity ==");
    printf("pid=%d ppid=%d\n", getpid(), getppid());
    snprintf(buf, sizeof buf, "/proc/%d/comm", getppid());
    FILE *f = fopen(buf, "r");
    if (f) { char c[128] = {0}; fgets(c, sizeof c, f); printf("parent_comm=%s", c); fclose(f); }

    line("== status ==");
    f = fopen("/proc/self/status", "r");
    if (f) while (fgets(buf, sizeof buf, f))
        if (!strncmp(buf, "TracerPid", 9) || !strncmp(buf, "Threads", 7))
            printf("  %s", buf);
    if (f) fclose(f);

    line("== threads (/proc/self/task) ==");
    int nthreads = 0;
    f = fopen("/proc/self/task", "r");   /* readdir needs dirent; count via ls-style not needed here */
    if (f) fclose(f);
    /* count via repeated gets on /proc/self/status Threads line is done above;
       list tids with opendir for accuracy: */
    {
        DIR *d = opendir("/proc/self/task");
        struct dirent *e;
        if (d) { while ((e = readdir(d))) if (e->d_name[0] != '.') { nthreads++; printf("  tid %s\n", e->d_name); } closedir(d); }
        printf("thread_count=%d\n", nthreads);
    }

    line("== fds ==");
    {
        DIR *d = opendir("/proc/self/fd");
        struct dirent *e;
        if (d) {
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                int fd = atoi(e->d_name);
                snprintf(buf, sizeof buf, "/proc/self/fd/%d", fd);
                char tgt[300];
                ssize_t n = readlink(buf, tgt, sizeof tgt - 1);
                if (n > 0) { tgt[n] = 0; printf("  fd %2d -> %s\n", fd, tgt); }
            }
            closedir(d);
        }
    }

    line("== maps: executable and anonymous-rwx regions ==");
    f = fopen("/proc/self/maps", "r");
    int total = 0;
    if (f) {
        char perms[8], path[256], addr[64];
        while (fgets(buf, sizeof buf, f)) {
            total++;
            path[0] = 0;
            if (sscanf(buf, "%63s %7s %*s %*s %*s %255[^\n]", addr, perms, path) >= 2) {
                int exec = perms[2] == 'x';
                int anon = (path[0] == 0);
                if ((exec && anon) || (exec && perms[1] == 'w'))
                    printf("  SUSPECT %s %s %s\n", addr, perms, anon ? "(anonymous)" : path);
            }
        }
        fclose(f);
    }
    printf("  total_map_lines=%d\n", total);
    ssize_t n = readlink("/proc/self/exe", buf, 500);
    if (n > 0) { buf[n] = 0; printf("  self_exe=%s\n", buf); }
    printf("  execfn=%s\n", (char *)getauxval(AT_EXECFN));

    return 0;
}
