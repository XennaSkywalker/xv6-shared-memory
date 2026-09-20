#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
    int id;
    int addr;
    int ret;

    printf(1, "Testing shared memory syscalls...\n");

    id = shmget();
    printf(1, "shmget returned: %d\n", id);

    addr = shmat();
    printf(1, "shmat returned: 0x%x\n", addr);

    ret = shmdt();
    printf(1, "shmdt returned: %d\n", ret);

    exit();
}