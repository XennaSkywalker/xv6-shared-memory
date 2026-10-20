#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "shmuser.h"

int
shm_open_test(void)
{
    int fd1;
    int fd2;
    int fd3;

    printf(1, "===== shm_open TEST =====\n");

    /* Test 1: Create a shared-memory object. */
    fd1 = shmopen("/test", O_CREAT | O_RDWR, 0666);

    if (fd1 < 0) {
        printf(1, "FAIL: shm_open create\n");
        return -1;
    }

    printf(1, "PASS: Created object, fd=%d\n", fd1);

    /* Test 2: Open the existing object. */
    fd2 = shmopen("/test", O_RDWR, 0666);

    if (fd2 < 0) {
        printf(1, "FAIL: shm_open existing object\n");
        return -1;
    }

    printf(1, "PASS: Opened existing object, fd=%d\n", fd2);

    /* Test 3: O_CREAT | O_EXCL must fail for existing objects. */
    fd3 = shmopen("/test", O_CREAT | O_EXCL | O_RDWR, 0666);

    if (fd3 >= 0) {
        printf(1, "FAIL: O_EXCL should have failed\n");
        close(fd3);
        return -1;
    }

    printf(1, "PASS: O_EXCL correctly rejected existing object\n");

    /* Test 4: Opening a nonexistent object without O_CREAT. */
    fd3 = shmopen("/missing", O_RDWR, 0666);

    if (fd3 >= 0) {
        printf(1, "FAIL: Opened nonexistent object\n");
        close(fd3);
        return -1;
    }

    printf(1, "PASS: Missing object correctly rejected\n");

    close(fd1);
    close(fd2);

    printf(1, "===== TEST COMPLETE =====\n");
    return 0;
}

int
main(void)
{
    if (shm_open_test() == -1)
        exit();
    exit();
}
