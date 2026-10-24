/*
 * naive - the "string identification" detector the assignment warns about.
 * Deliberately fragile: it looks for the tracer's NAME in the process's
 * maps and parent process name. It exists to be broken by lab/patch.sh,
 * proving why pinocchio never reads names.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char buf[1024];
    int hit = 0;

    FILE *f = fopen("/proc/self/maps", "r");
    while (f && fgets(buf, sizeof buf, f)) {
        if (strstr(buf, "Tracer")) {
            buf[strcspn(buf, "\n")] = 0;
            printf("  [str] maps: %s\n", buf);
            hit = 1;
        }
    }
    if (f) fclose(f);

    snprintf(buf, sizeof buf, "/proc/%d/comm", getppid());
    f = fopen(buf, "r");
    if (f && fgets(buf, sizeof buf, f) && strstr(buf, "Tracer")) {
        printf("  [str] parent comm: %s", buf);
        hit = 1;
    }
    if (f) fclose(f);

    printf("naive verdict: %s\n", hit ? "INSTRUMENTED" : "clean");
    return hit ? 1 : 0;
}
