/*
 * Negative experiment: Pin 3.30 preserves this self-modifying code's
 * semantics, so the canary is evidence only and is not part of the detector.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

int main(void)
{
    unsigned char *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) return 2;

    const unsigned char code[] = {0xb8, 1, 0, 0, 0, 0xc3}; /* mov eax,1; ret */
    memcpy(page, code, sizeof code);
    __builtin___clear_cache((char *)page, (char *)page + sizeof code);

    int (*function)(void);
    _Static_assert(sizeof function == sizeof page, "unexpected pointer size");
    memcpy(&function, &page, sizeof function);

    int first = function();
    page[1] = 2;
    __builtin___clear_cache((char *)page, (char *)page + sizeof code);
    int second = function();

    printf("first=%d second=%d\n", first, second);
    munmap(page, 4096);
    return first == 1 && second == 2 ? 0 : 1;
}
