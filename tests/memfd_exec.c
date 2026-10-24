/* Test fixture: create an executable memfd mapping and close its fd. */
#define _GNU_SOURCE
#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

__attribute__((constructor))
static void create_memfd_mapping(void)
{
    int fd = (int)syscall(SYS_memfd_create, "pinocchio-test-cache", MFD_CLOEXEC);
    if (fd < 0) return;
    if (ftruncate(fd, 4096) == 0)
        (void)mmap(NULL, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
    close(fd);
}
