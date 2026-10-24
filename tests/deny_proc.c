/* Test fixture: deny openat after the dynamic loader has started the process. */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

__attribute__((constructor))
static void deny_openat(void)
{
    static const struct sock_filter instructions[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 (unsigned int)offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_openat, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EACCES),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    static const struct sock_fprog program = {
        .len = (unsigned short)(sizeof instructions / sizeof instructions[0]),
        .filter = (struct sock_filter *)instructions,
    };

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0)
        (void)prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program);
}
