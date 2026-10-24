/* Attack fixture: executable regular-file mapping with no retained fd. */
#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static char path[] = "/tmp/pinocchio-file-cache-XXXXXX";

__attribute__((constructor))
static void create_file_mapping(void)
{
    int fd = mkstemp(path);
    if (fd < 0 || ftruncate(fd, 4096) != 0)
        _exit(99);
    if (mmap(NULL, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0) == MAP_FAILED)
        _exit(99);
    close(fd);
}

__attribute__((destructor))
static void remove_backing_file(void)
{
    unlink(path);
}
