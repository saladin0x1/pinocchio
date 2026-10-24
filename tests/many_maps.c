/* Test fixture: force /proc/self/maps to span multiple read() calls. */
#define _GNU_SOURCE
#include <stddef.h>
#include <sys/mman.h>

#define TEST_MAPPINGS 320
static void *mappings[TEST_MAPPINGS + 1];

__attribute__((constructor))
static void create_many_mappings(void)
{
    for (size_t i = 0; i < TEST_MAPPINGS; i++) {
        int protection = i % 2 == 0 ? PROT_NONE : PROT_READ;
        mappings[i] = mmap(NULL, 4096, protection,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    mappings[TEST_MAPPINGS] = mmap(NULL, 4096, PROT_READ | PROT_EXEC,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}
